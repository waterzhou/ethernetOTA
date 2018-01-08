/**
 * \file
 *
 * \brief Upgrade.
 *
 * Copyright (c) 2013-2014 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */ 
#include "compiler.h"
#include "upgrade.h"
#include "tomcrypt.h"
#include "timer_mgt.h"
#include "efc.h"
#include "flash_efc.h"
#include "app_debug.h"

/* The first 2bytes major version, and the last 2bytes minor version */
static uint32_t app_version = (MAJOR_VERSION << 16) | MINOR_VERSION;

static uint8_t plaintext[MAX_PLAINTEXT_LEN];

/*CRC32 Table*/
static uint32_t crc32_table[256];
static void init_crc32_table(void);

static int16_t pre_pkt_seq = -1;
static uint32_t is_authenticated = 0;
static uint32_t file_len = 0;

static void switching_timer_start(uint32_t interval, upgrade_timer_fn fn);
static int32_t data_decrypt(uint8_t *buffer, int32_t buf_len);
static uint32_t data_write(uint32_t fl_addr, uint8_t *buffer, int32_t buf_len);
static uint32_t check_integrity(uint32_t fl_addr, uint32_t len);
__no_inline RAMFUNC static void code_switching(void);

static int32_t crypto_idx = -1;
static aes_gcm_t gcm_instance;
/* Key */
static uint8_t crypto_key[] = {
	0xfe, 0xff, 0xe9, 0x92, 0x86, 0x65, 0x73, 0x1c,
	0x6d, 0x6a, 0x8f, 0x94, 0x67, 0x30, 0x83, 0x08};
static uint32_t keylen = 16;

/* AAD data */	 
static uint8_t crypto_aad[] = {
	0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec, 0x13, 0xbd,
	0xf5, 0x3a, 0x67, 0xb2, 0x12, 0x57, 0xbd, 0xdf,
	0x58, 0xe2, 0xfc, 0xce, 0xfa, 0x7e, 0x30, 0x61,
	0x36, 0x7f, 0x1d, 0x57, 0xa4, 0xe7, 0x45, 0x5a};
static uint32_t aadlen = 32;

static uint8_t crypto_iv[] = {
	0xca, 0xfe, 0xba, 0xbe, 0xfa, 0xce, 0xdb, 0xad,
	0xde, 0xca, 0xf8, 0x88, 0x33, 0xd4, 0x82, 0xfe};
static uint32_t ivlen = 16;

#ifdef __APP_DEBUG__
static char ascii_buf[3] = {'\0','\0','\0'};
static char* hex2ascii(uint8_t hex, char *buf)
{
	uint8_t val = 0;
	val = (hex >> 4) & 0x0f;
	buf[0] = (val >= 10) ? ((val - 10) + 'A') : (val + '0');
	val = hex & 0x0f;
	buf[1] = (val >= 10) ? ((val - 10) + 'A') : (val + '0');
	return buf;
}
#endif

int32_t upgrade_init(void)
{
	/* Register cipher */
	register_cipher(&aes_desc);
	
	/* find aes */
	crypto_idx = find_cipher("aes");
	if (crypto_idx == -1) {
		dbg_error("Not find the Cipher.\r\n");
		return FAILURE;
	}
	gcm_instance.key = &crypto_key[0];
	gcm_instance.keylen = keylen;

	/* 
	 * No use AAD to compromise with Android app.
	 * AAD will be used as plaintext to get the TAG.
	 */
	gcm_instance.aad = NULL;
	gcm_instance.aadlen = 0;
	
	gcm_instance.iv = &crypto_iv[0];
	gcm_instance.ivlen = ivlen;
	
	gcm_instance.taglen = GCM_TAG_LEN;
		
	init_crc32_table();

	return SUCCESS;
}
/* CCITT8023, Polynominal 0x04C11DB7 */
static void init_crc32_table()
{
	uint32_t   i,j;
	uint32_t   crc;
	for(i = 0; i < 256; i++) {
		crc = i;
		for(j = 0; j < 8; j++) {
			if(crc & 1) {
				crc = (crc >> 1) ^ 0xEDB88320;
			}
			else {
				crc = crc >> 1;
			}
		}
		crc32_table[i] = crc;
	}
}

static uint32_t calc_crc( uint8_t *buf, uint32_t len)
{
	uint32_t ret = 0xFFFFFFFF;
	uint32_t   i;
	for(i = 0; i < len;i++)
	{
		ret = crc32_table[((ret & 0xFF) ^ buf[i])] ^ (ret >> 8);
	}
	ret = ~ret;
	return ret;
}

static int32_t is_pkt_valid(uint16_t type, uint16_t len, uint8_t *buffer, uint32_t *ack)
{
	uint8_t *pdata = buffer;
	uint8_t *ptemp = NULL;
	uint32_t i, pkt_crc, calculated_crc;
	if(type == AUTHENTICATED_FRAME) {
		if(*(pdata + PKT_HEADER_LEN + PKT_TYPE_LEN + GCM_IV_LEN + GCM_TAG_LEN) != PACKET_TAIL) {
			*ack = u_htonl(AUTHENTICATED_INVALID);
			dbg_error("Authenticated frame is invalid\r\n");
			return PKT_INVALID;
		}
	}
	else if(type == DATA_FRAME) {
		if(*(pdata + PKT_PAYLOAD_POS + len + PKT_CRC_LEN) == PACKET_TAIL) {
			ptemp = (uint8_t*)&pkt_crc;
			for(i = 0; i < PKT_CRC_LEN; i++){
				*(ptemp + i) = *(pdata + PKT_PAYLOAD_POS + len + i);
			}
			pkt_crc = u_htonl(pkt_crc);
			calculated_crc = calc_crc(buffer, PKT_PAYLOAD_POS + len);
			if(pkt_crc != calculated_crc) {
				dbg_error("CRC check error. Packet CRC: %x, Calculated CRC: %x\r\n", pkt_crc, calculated_crc);
				*ack = u_htonl(DATA_FRAME_CRC_ERR);
				return PKT_INVALID;
			}
		}
		else {
			*ack = u_htonl(DATA_FRAME_INVALID);
			return PKT_INVALID;
		}
	}
	else if(type == ACK_FRAME) {
		*ack = u_htonl(PKT_INVALID);
		dbg_error("Error, ACK frame received.\r\n");
		return PKT_INVALID;
	}
	else if(type == COMMAND_FRAME) {
		if(*(pdata + PKT_HEADER_LEN + PKT_TYPE_LEN + PKT_CMD_LEN) != PACKET_TAIL) {
			*ack = u_htonl(COMMAND_FRAME_INVALID);
			dbg_error("Command frame is invalid\r\n");
			return PKT_INVALID;
		}
	}
	else {
		*ack = u_htonl(PKT_INVALID);
		dbg_error("Invalid frame type, %x\r\n", type);
		return PKT_INVALID;
	}
	return PKT_VALID;
}

static inline void upgrade_start(void)
{
	dbg_info("Firmware upgrading start..\r\n");
	is_authenticated = 1;
	pre_pkt_seq = -1;
	file_len = 0;
}

static inline void upgrade_end(void)
{
	dbg_info("Firmware upgrading end.\r\n");
	is_authenticated = 0;
	pre_pkt_seq = -1;
	file_len = 0;
}

static int32_t authenticating_frame(uint8_t *buffer)
{
	int32_t i, err;
	uint8_t *pdata;
	
	for(i = 0; i < GCM_IV_LEN; i++) {
		crypto_iv[i] = *(buffer + i);
	}
	pdata = buffer + i;
	
	if(crypto_idx == -1) {
		dbg_error("No Cipher found.\r\n");
		return FAILURE;
	}
	
	/* Use AAD data as plaintext to do the authentication */
	gcm_instance.pt = &crypto_aad[0];
	gcm_instance.ptlen = aadlen;
	
	/* Use plaintext buffer as ciphertext buffer temporarily */
	gcm_instance.ct = &plaintext[0];
	
	/* Get the TAG */
	err = gcm_memory(crypto_idx, gcm_instance.key, gcm_instance.keylen,
					 gcm_instance.iv, gcm_instance.ivlen, gcm_instance.aad,
					 gcm_instance.aadlen, gcm_instance.pt, gcm_instance.ptlen,
					 gcm_instance.ct, gcm_instance.tag, &(gcm_instance.taglen), GCM_ENCRYPT);
	if(err != CRYPT_OK) {
		dbg_error("Get TAG error.\r\n");
		return FAILURE;
	}
	for(i = 0; i < GCM_TAG_LEN; i++) {
		if(gcm_instance.tag[i] != *(pdata + i)) {
			dbg_info("TAG value mismatch.\r\n");
#ifdef __APP_DEBUG__
			dbg_info("Received IV value:");
			for(i = 0; i < GCM_IV_LEN; i++) {
				if((i % 8) == 0) {
					dbg_info("\r\n");
				}
				dbg_info("%s ", hex2ascii(*(gcm_instance.iv + i), ascii_buf));
			}
			dbg_info("\r\nReceived TAG value:");
			for(i = 0; i < GCM_TAG_LEN; i++) {
				if((i % 8) == 0) {
					dbg_info("\r\n");
				}
				dbg_info("%s ", hex2ascii(*(pdata + i), ascii_buf));
			}
			dbg_info("\r\nDecrypt TAG value:");
			for(i = 0; i < GCM_TAG_LEN; i++) {
				if((i % 8) == 0) {
					dbg_info("\r\n");
				}
				dbg_info("%s ", hex2ascii(gcm_instance.tag[i], ascii_buf));
			}
			dbg_info("\r\nDecrypt KEY value:");
			for(i = 0; i < keylen; i++) {
				if((i % 8) == 0) {
					dbg_info("\r\n");
				}
				dbg_info("%s ", hex2ascii(*(gcm_instance.key + i), ascii_buf));
			}
#endif
			return FAILURE;
		}
	}
	return SUCCESS;
}

int32_t handle_upgrade_packet(uint8_t *buffer, uint32_t *ack)
{
	uint16_t i, type, seq, len;
	uint32_t fl_addr, crc = 0;
	uint32_t cmd;
	uint8_t *pdata = buffer;
	uint8_t *ptemp = NULL;
	
	/* pdata is aligned by MEM_ALIGNMENT */
	type = *(uint16_t*)(pdata + PKT_HEADER_LEN);
	seq = *(uint16_t*)(pdata + PKT_HEADER_LEN + PKT_TYPE_LEN);
	len = *(uint16_t*)(pdata + PKT_HEADER_LEN + PKT_TYPE_LEN + PKT_SEQ_LEN);
	
	*ack = (seq << 16) | len; //for data frame if no error occurs
	
	type = u_htons(type);
	seq = u_htons(seq);
	len = u_htons(len);
	
	if(is_pkt_valid(type, len, buffer, ack) != PKT_VALID) {
		return FAILURE;
	}
	
	if(is_authenticated == 1) {
		if(type == AUTHENTICATED_FRAME) {
			pdata = buffer + PKT_HEADER_LEN + PKT_TYPE_LEN;
			
			if(authenticating_frame(pdata) != SUCCESS) {
				dbg_error("Authentication Fail\r\n");
				upgrade_end();
				*ack = u_htonl(AUTHENTICATED_FAIL);
				return FAILURE;
			}
			
			*ack = u_htonl(AUTHENTICATED_PASS);
			upgrade_start();
		}
		else if(type == DATA_FRAME) {
			if(seq > MAX_SEQ_NUM) {
				*ack = u_htonl(DATA_FRAME_SEQ_ERR);
				dbg_error("Sequence %x exceeds the maximum value %x.\r\n", seq, MAX_SEQ_NUM);
				return FAILURE;
			}
			
			if(pre_pkt_seq + 1 != seq) {
				*ack = u_htonl(DATA_FRAME_SEQ_ERR);
				dbg_error("Sequence error. Current Seq %x, it should be %x\r\n", seq, pre_pkt_seq+1);
				upgrade_end();
				return FAILURE;
			}
			
			if((len > MAX_PLAINTEXT_LEN) || (len == 0)) {
				*ack = u_htonl(DATA_FRAME_LEN_ERR);
				dbg_error("Data frame Length error, received len is %x\r\n", len);
				return FAILURE;
			}
			
			pdata = buffer + PKT_PAYLOAD_POS;
			if(data_decrypt(pdata, len) != CRYPT_OK) {
				*ack = u_htonl(DATA_FRAME_DECRYPT_ERR);
				dbg_error("Data decrypt error.\r\n");
				return FAILURE;
			}
			
			/* update the packet sequence */
			pre_pkt_seq = seq;
			
			fl_addr = (seq * MAX_PLAINTEXT_LEN) + IFLASH1_ADDR;
			if(data_write(fl_addr, gcm_instance.pt, len) != SUCCESS) {
				*ack = u_htonl(DATA_FRAME_FLASH_WR_ERR);
				dbg_error("Data write error\r\n");
				upgrade_end();
				return FAILURE;
			}
			if(check_integrity(fl_addr, len) != SUCCESS) {
				*ack = u_htonl(DATA_FRAME_FLASH_WR_ERR);
				dbg_error("Data written mismatch the data read.\r\n");
				upgrade_end();
				return FAILURE;
			}
			file_len += len;				
		}
		else if(type == COMMAND_FRAME) {
			ptemp = (uint8_t*)&cmd;
			pdata = buffer + PKT_HEADER_LEN + PKT_TYPE_LEN;
			for(i = 0; i < PKT_CMD_LEN; i++) {
				*(ptemp + i) = *(pdata + i);
			}
			cmd = u_htonl(cmd);
			if(cmd == GET_VERSION) {
				*ack = u_htonl(app_version);
			}
			else if(cmd == GET_CRC) {
				if((file_len > 0) && (file_len < IFLASH1_SIZE)) {
					pdata = (uint8_t*)IFLASH1_ADDR;
					crc = calc_crc(pdata, file_len);
					*ack = u_htonl(crc);
				}
				else {
					/* Return a random CRC value to indicate error */
					*ack = u_htonl(crc);
					dbg_error("Invalid file length(%x) to calculate CRC.\r\n", file_len);
				}
			}
			else if(cmd == EXEC) {
				//code_exec();
				*ack = u_htonl(COMMAND_FRAME_PASS);
				switching_timer_start(CODE_SWITCHING_INTERVAL, code_switching);
			}
			else {
				//Not support command
				dbg_error("Command: %x, Not Support.\r\n", cmd);
				*ack = u_htonl(COMMAND_FRAME_CMD_ERR);
				return FAILURE;
			}
		}
		else {
			dbg_error("Wrong packet, packet type: %x\r\n", type);
			*ack = u_htonl(DATA_FRAME_TYPE_ERR);
			return FAILURE;
		}
		
	}
	else {
		if(type == AUTHENTICATED_FRAME) {
			pdata = buffer + PKT_HEADER_LEN + PKT_TYPE_LEN;
			
			if(authenticating_frame(pdata) != SUCCESS) {
				dbg_error("Authentication Fail\r\n");
				upgrade_end();
				*ack = u_htonl(AUTHENTICATED_FAIL);
				return FAILURE;
			}
			
			*ack = u_htonl(AUTHENTICATED_PASS);
			upgrade_start();
		}
		else {
			dbg_error("Not authenticated.\r\n");
			*ack = u_htonl(AUTHENTICATED_FAIL);
			return FAILURE;
		}
	}
	return SUCCESS;
}

static int32_t data_decrypt(uint8_t *buffer, int32_t buf_len)
{
	int32_t err;
	if(crypto_idx == -1) {
		return CRYPT_NOP;
	}
	gcm_instance.pt = &plaintext[0];
	memset(gcm_instance.pt, 0xff, MAX_PLAINTEXT_LEN);
	gcm_instance.ct = buffer;
	gcm_instance.ptlen = buf_len;
	
	/* Decrypt the data */
	err = gcm_memory(crypto_idx, gcm_instance.key, gcm_instance.keylen,
					 gcm_instance.iv, gcm_instance.ivlen, gcm_instance.aad,
					 gcm_instance.aadlen, gcm_instance.pt, gcm_instance.ptlen,
					 gcm_instance.ct, gcm_instance.tag, &(gcm_instance.taglen), GCM_DECRYPT);
	
	return err;
}

/* Write the data into the flash  */
static uint32_t data_write(uint32_t fl_addr, uint8_t *buffer, int32_t buf_len)
{
	uint32_t status;
	uint32_t ul_page;
	uint32_t of_addr = 0;
	
	if(fl_addr < IFLASH1_ADDR) {
		return FAILURE;
	}
	
	while(buf_len > 0) {
		ul_page = (fl_addr + of_addr - IFLASH1_ADDR)/IFLASH0_PAGE_SIZE;
		if((ul_page % PAGES_TO_ERASE) == 0) {//8 pages erase
			status = flash_erase_page(fl_addr + of_addr, IFLASH_ERASE_PAGES_8);
			if(status != SUCCESS) {
				dbg_error("Error: %s -> flash_erase_page: rc %u", __FUNCTION__, status);
				return status;
			}
		}
		status = flash_write(fl_addr + of_addr, buffer + of_addr, IFLASH0_PAGE_SIZE, 0);
		if(status != SUCCESS) {
			dbg_error("Error: %s -> flash_write: rc %u", __FUNCTION__, status);
			return status;
		}
		of_addr += IFLASH0_PAGE_SIZE;
		buf_len -= IFLASH0_PAGE_SIZE;
	}
	return SUCCESS;
}

__no_inline
RAMFUNC
static void code_switching(void)
{
	uint32_t status;
	Rstc *rstc = RSTC;
	
	cpu_irq_disable();
	if(flash_is_gpnvm_set(GPNVM2) == 1) {
		status = flash_clear_gpnvm(GPNVM2);
		if(status != SUCCESS) {
			dbg_error("Error: %s -> flash_clear_gpnvm: rc %u", __FUNCTION__, status);
		}
	}
	else {
		status = flash_set_gpnvm(GPNVM2);
		if(status != SUCCESS) {
			dbg_error("Error: %s -> flash_set_gpnvm: rc %u", __FUNCTION__, status);
		}
	}
	/* Software reset */
	rstc->RSTC_CR = RSTC_CR_KEY_PASSWD | RSTC_CR_PROCRST | RSTC_CR_PERRST;
}


static uint32_t check_integrity(uint32_t fl_addr, uint32_t len)
{
	uint32_t i;
	char *page_buf = (char *)fl_addr;
	
	for(i = 0; i < len; i++) {
		if(page_buf[i] != gcm_instance.pt[i]) {
#ifdef __APP_DEBUG__
			uint32_t j, num;
			dbg_info("Index: %x, Buffer Content: %x, Flash Contents: %x\r\n",
			i, gcm_instance.pt[i], page_buf[i]);
			num = (len - i) > BYTES_TO_DISPLAY ? BYTES_TO_DISPLAY : (len - i);
			dbg_info("%u bytes buffer contents after index %u\r\n",num, i);
			for(j = i; j < num; j++) {
				if((j - i) % 8 == 0) {
					dbg_info("\r\n");
				}
				dbg_info("%x ", gcm_instance.pt[j]);
			}
			dbg_info("\r\n%u bytes Flash contents after index %u\r\n",num, i);
			for(j = i; j < num; j++) {
				if((j - i) % 8 == 0) {
					dbg_info("\r\n");
				}
				dbg_info("%x ", page_buf[j]);
			}
#endif
			return FAILURE;
		}
	}
	return SUCCESS;
}

static upgrade_timer_t switching_timer;
upgrade_timer_fn switching_timer_poll = NULL;
/**
 * \brief Timer management function.
 */
static void switching_timer_update(void)
{
	uint32_t ul_last_time = switching_timer.last_time;
	uint32_t ul_cur_time, ul_time_diff;
	upgrade_timer_t *p_tmr_inf;

	ul_cur_time = sys_get_ms();
	if (ul_cur_time >= ul_last_time) {
		ul_time_diff = ul_cur_time - ul_last_time;
	} else {
		ul_time_diff = 0xFFFFFFFF - ul_last_time + ul_cur_time;
	}

	if (ul_time_diff) {
		switching_timer.last_time = ul_cur_time;

		p_tmr_inf = &switching_timer;
		p_tmr_inf->timer += ul_time_diff;
		if (p_tmr_inf->timer > p_tmr_inf->timer_interval) {
			if (p_tmr_inf->timer_func) {
				p_tmr_inf->timer_func();
			}
			p_tmr_inf->timer -= p_tmr_inf->timer_interval;
		}
	}
}

static void switching_timer_start(uint32_t interval, upgrade_timer_fn fn)
{
	switching_timer.timer = 0;
	switching_timer.last_time = sys_get_ms();
	switching_timer.timer_interval = interval;
	switching_timer.timer_func = fn;
	switching_timer_poll = switching_timer_update;
}
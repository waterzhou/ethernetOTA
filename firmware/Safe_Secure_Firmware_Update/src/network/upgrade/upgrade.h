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


#ifndef UPGRADE_H_
#define UPGRADE_H_

#define MAJOR_VERSION		1
#define MINOR_VERSION		0

#define VALID_FIRMWARE		MINOR_VERSION << 24 | MAJOR_VERSION << 16 | \
							'T' << 8 | 'A'

#define UPGRADE_PKT_HEADER		"[UPGRADE"
#define GPNVM2					2
#define PAGES_TO_ERASE			8

#define BYTES_TO_DISPLAY		64

#define MAX_PLAINTEXT_LEN		1024
#define GCM_TAG_LEN				16
#define GCM_IV_LEN				16

#define PACKET_TAIL				']'

typedef struct _aes_gcm_{
	uint8_t *key;
	uint32_t keylen;
	uint8_t *pt;
	uint32_t ptlen;
	uint8_t *aad;
	uint32_t aadlen;
	uint8_t *iv;
	uint32_t ivlen;
	uint8_t *ct;
	uint8_t tag[GCM_TAG_LEN];
	uint32_t taglen;
}aes_gcm_t;

/* Timer structure */
typedef struct _upgrade_timer_t {
	uint32_t timer;
	uint32_t timer_interval;
	uint32_t last_time;
	void (*timer_func)(void);
} upgrade_timer_t;

typedef void (*upgrade_timer_fn)(void);

#define u_htons(n)		(((n & 0xff) << 8) | ((n & 0xff00) >> 8))
#define u_ntohs(n)		(((n & 0xff) << 8) | ((n & 0xff00) >> 8))
#define u_htonl(n)		(((n & 0xff) << 24) | \
						((n & 0xff00) << 8) | \
						((n & 0xff0000UL) >> 8) | \
						((n & 0xff000000UL) >> 24))
#define u_ntohl(n)		(((n & 0xff) << 24) | \
						((n & 0xff00) << 8) | \
						((n & 0xff0000UL) >> 8) | \
						((n & 0xff000000UL) >> 24))

#define PKT_HEADER_LEN				8
#define PKT_TYPE_LEN				2
#define PKT_SEQ_LEN					2
#define PKT_LENGTH_LEN				2
#define PKT_PAYLOAD_POS				14
#define PKT_ACK_LEN					4
#define PKT_CRC_LEN					4
#define PKT_CMD_LEN					4
#define MAX_SEQ_NUM					1023

#define AUTHENTICATED_FRAME			0x0800
#define DATA_FRAME					0x0801
#define COMMAND_FRAME				0x0802
#define ACK_FRAME					0x0803

#define ACK_FRAME_LEN				15

#define SUCCESS					0
#define FAILURE					1

/* Authenticated frame error code */
#define AUTHENTICATED_PASS			((0xffff << 16) | 0x0)
#define AUTHENTICATED_FAIL			((0xffff << 16) | 0x1)
#define AUTHENTICATED_INVALID		((0xffff << 16) | 0x2)
#define AUTHENTICATED_TYPE_ERR		((0xffff << 16) | 0x3)

/* Data frame error code */
#define DATA_FRAME_INVALID			((0xffff << 16) | 0x2)
#define DATA_FRAME_TYPE_ERR			((0xffff << 16) | 0x3)
#define DATA_FRAME_SEQ_ERR			((0xffff << 16) | 0x4)
#define DATA_FRAME_LEN_ERR			((0xffff << 16) | 0x5)
#define DATA_FRAME_CRC_ERR			((0xffff << 16) | 0x6)
#define DATA_FRAME_DECRYPT_ERR		((0xffff << 16) | 0x7)
#define DATA_FRAME_FLASH_WR_ERR		((0xffff << 16) | 0x8)

/* Command frame error code */
#define COMMAND_FRAME_PASS			((0xffff << 16) | 0x0)
#define COMMAND_FRAME_FAIL			((0xffff << 16) | 0x1)
#define COMMAND_FRAME_INVALID		((0xffff << 16) | 0x2)
#define COMMAND_FRAME_CMD_ERR		((0xffff << 16) | 0x3)

/* Supported Command */
#define GET_VERSION					0x55AA0400
#define GET_CRC						0x55AA0401
#define EXEC						0x55AA0402

#define PKT_VALID					((0xffff << 16) | 0x0)
#define PKT_INVALID					((0xffff << 16) | 0x2)

/* Upgrade initialize */
int32_t upgrade_init(void);

/* Upgrade packet handler */
int32_t handle_upgrade_packet(uint8_t *buffer, uint32_t *ack);

extern upgrade_timer_fn switching_timer_poll;

#define CODE_SWITCHING_INTERVAL		3000 //3seconds

#endif /* UPGRADE_H_ */
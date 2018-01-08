/**
 * \file
 *
 * \brief TCP Server.
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

#include "server.h"
#include "ioport.h"
#include "upgrade/upgrade.h"
#include "app_debug.h"
/**
 * \brief Callback called on connection error.
 *
 * \param arg Pointer to a structure of data buffer.
 * \param err Error code.
 */
static void server_connection_err(void *arg, err_t err)
{
	LWIP_UNUSED_ARG(err);
	
	app_to_eth_t *ptemp = (app_to_eth_t*)arg;
	app_to_eth_t *ntemp = NULL;
	
	if(ptemp != NULL) {
		while(ptemp->next != NULL) {
			ntemp = ptemp->next;
			mem_free(ptemp);
			ptemp = ntemp;
		}
		mem_free(ptemp);
	}
}

/**
 * \brief Close the connection.
 *
 * \param pcb Pointer to a TCP connection structure.
 * \param arg Pointer to a structure of data buffer.
 */
static void server_close_connection(struct tcp_pcb *pcb, void *arg)
{
	app_to_eth_t *ptemp = (app_to_eth_t*)arg;
	app_to_eth_t *ntemp = NULL;
	
	if(ptemp != NULL) {
		while(ptemp->next != NULL) {
			ntemp = ptemp->next;
			mem_free(ptemp);
			ptemp = ntemp;
		}
		mem_free(ptemp);
	}	
	tcp_arg(pcb, NULL);
	tcp_sent(pcb, NULL);
	tcp_recv(pcb, NULL);
	tcp_close(pcb);
}

/**
 * \brief Send data to client.
 *
 * \param pcb Pointer to a TCP connection structure.
 * \param to_client Pointer to a structure of data buffer.
 */
static void send_data_to_client(struct tcp_pcb *pcb, app_to_eth_t *to_client)
{
	err_t err;
	uint32_t len;

	/* We cannot send more data than space available in the send buffer. */
	if (tcp_sndbuf(pcb) < to_client->left) {
		len = tcp_sndbuf(pcb);
	}
	else {
		len = to_client->left;
	}

	do {
		/* Use copy flag to avoid using flash as a DMA source (forbidden). */
		err = tcp_write(pcb, to_client->data, len, TCP_WRITE_FLAG_COPY);
		if (err == ERR_MEM) {
			len /= 2;
		}
	} while (err == ERR_MEM && len > 1);

	if (err == ERR_OK) {
		to_client->data = (uint8_t*)to_client->data + len;
		to_client->left = to_client->left - len;
	} else {
		server_connection_err(pcb->callback_arg, ERR_MEM);
	}
}

static void send_data(struct tcp_pcb *pcb, uint8_t *buf, uint16_t len)
{
	app_to_eth_t *pdata = NULL;
	app_to_eth_t *ptemp = NULL;
		
	pdata = (app_to_eth_t*)mem_malloc(LWIP_MEM_ALIGN_SIZE(sizeof(app_to_eth_t))
									  + LWIP_MEM_ALIGN_SIZE(len));
	if(pdata != NULL) {
		pdata->len = len;
		pdata->left = len;
		pdata->acked = 0;
		pdata->data = LWIP_MEM_ALIGN((void*)((uint8_t *)pdata + sizeof(app_to_eth_t)));
		pdata->next = NULL;

		memcpy(pdata->data, buf, len);
		
		if(pcb->callback_arg != NULL) { //previous data is still in sending
			ptemp = (app_to_eth_t*)pcb->callback_arg;
			while(ptemp->next != NULL) {
				ptemp = ptemp->next;
			}
			ptemp->next = pdata;
		}
		else {
			tcp_arg(pcb, (void*)pdata);
			send_data_to_client(pcb, pdata);
		}
	}
	else {
		dbg_error("mem_malloc failed.\r\n");
	}

}

/**
 * \brief Callback to handle data transfer.
 *
 * \param arg Pointer to a structure of data buffer.
 * \param pcb Pointer to a TCP connection structure.
 * \param len Unused.
 *
 * \return ERR_OK on success, ERR_ABRT otherwise.
 */
static err_t server_sent(void *arg, struct tcp_pcb *pcb, uint16_t len)
{
	app_to_eth_t *to_client;
	app_to_eth_t *ptemp;
	
	if(arg != NULL) {

		to_client = (app_to_eth_t*)arg;
		to_client->acked += len;

		if (to_client->left > 0) {
			send_data_to_client(pcb, to_client);
		}
		else if(to_client->acked < to_client->len) {
			return ERR_OK;
		}
		else if (to_client->next != NULL) {
			ptemp = to_client->next;
			mem_free(to_client);
			tcp_arg(pcb, ptemp);
			send_data_to_client(pcb, ptemp);		
		} 
		else {
			mem_free(to_client);
			tcp_arg(pcb, NULL);
		}
	}
	return ERR_OK;
}

static void generate_ack_frame(uint8_t *buffer, uint32_t ack)
{
	uint32_t i;
	uint8_t *pdata = NULL;
	uint8_t *ptemp = NULL;
	uint16_t type = u_htons(ACK_FRAME);
	
	strcpy((char*)buffer, UPGRADE_PKT_HEADER);
	pdata = buffer + PKT_HEADER_LEN;
	
	ptemp = (uint8_t *)&type;
	for(i = 0; i < PKT_TYPE_LEN; i++) {
		*pdata = *(ptemp + i);
		pdata++;
	}
	
	ptemp = (uint8_t *)&ack;
	for(i = 0; i < PKT_ACK_LEN; i++) {
		*pdata = *(ptemp + i);
		pdata++;
	}
	*pdata = PACKET_TAIL;
}

/**
 * \brief Core server receive function. Handle the request and process it.
 *
 * \param arg Pointer to a structure of data buffer.
 * \param pcb Pointer to a TCP connection structure.
 * \param p Incoming request.
 * \param err Connection status.
 *
 * \return ERR_OK.
 */
static err_t server_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{	
	uint8_t *data;
	uint32_t datasize;
	uint32_t i;
	struct pbuf *ptemp = NULL;
	app_from_eth_t *app_client = NULL;
	sub_data_t *p_eth = NULL;
	sub_data_t *p_eth_next = NULL;
	LWIP_UNUSED_ARG(arg);

	if (err == ERR_OK && p != NULL) {
				
		uint8_t *ciphertext = NULL;
		uint32_t ack;
		uint8_t ack_frame[ACK_FRAME_LEN];
		
		data = (uint8_t*)p->payload;
		if(strncmp((const char*)data, UPGRADE_PKT_HEADER, PKT_HEADER_LEN) == 0) {
			/* LED0 indicates upgrading is on-going */
			ioport_toggle_pin_level(LED_0_PIN);
			if(p->tot_len > p->len) {
				ciphertext = (uint8_t*)mem_malloc(LWIP_MEM_ALIGN_SIZE(MAX_PLAINTEXT_LEN));
				if(ciphertext == NULL) {
					dbg_error("mem_malloc failed.\r\n");
					tcp_recved(pcb, p->tot_len);
					pbuf_free(p);
					return ERR_OK;
				}
				memcpy(ciphertext, data, p->len);
				datasize = p->len;
				ptemp = p->next;
				while(ptemp != NULL) {
					data = (uint8_t*)ptemp->payload;
					memcpy(ciphertext + datasize, data, ptemp->len);
					datasize += ptemp->len;
					ptemp = ptemp->next;
				}
				if(handle_upgrade_packet(ciphertext, &ack) != SUCCESS) {
					ioport_set_pin_level(LED_0_PIN, LED0_ACTIVE);
				}
				mem_free(ciphertext);
			}
			else {
				if(handle_upgrade_packet(data, &ack) != SUCCESS) {
					ioport_set_pin_level(LED_0_PIN, LED0_ACTIVE);
				}
			}
			/* Inform TCP that APP has taken the data. */
			tcp_recved(pcb, p->tot_len);
			/* send ACK packet */
			generate_ack_frame(ack_frame, ack);
			send_data(pcb, ack_frame, ACK_FRAME_LEN);
		}
		else {
			for(i = 0; i < MAXIMUM_POOL_FOR_DATA_FROM_ETH; i++) {
				if(data_from_eth[i].dirty == 1) {
					if((data_from_eth[i].ipaddr == pcb->remote_ip.addr) && 
						(data_from_eth[i].port == pcb->remote_port)) {
							app_client = &data_from_eth[i];
							break;
					}
				}
			}
			if(app_client == NULL) {
				for(i = 0; i < MAXIMUM_POOL_FOR_DATA_FROM_ETH; i++) {
					if(data_from_eth[i].dirty != 1) {
						app_client = &data_from_eth[i];
						app_client->ipaddr = pcb->remote_ip.addr;
						app_client->port = pcb->remote_port;
						break;
					}
				}
			}
			if(app_client == NULL) {
				//No enough pool for the data
				dbg_error("No enough pool for the data\r\n");
				tcp_recved(pcb, p->tot_len);
				pbuf_free(p);
				return ERR_OK;
			}
		
			p_eth = (sub_data_t*)mem_malloc(LWIP_MEM_ALIGN_SIZE(sizeof(sub_data_t) 
											+ LWIP_MEM_ALIGN_SIZE(p->tot_len)));
			if(p_eth == NULL) {
				dbg_error("mem_malloc failed.\r\n");
				tcp_recved(pcb, p->tot_len);
				pbuf_free(p);
				return ERR_OK;
			}
			p_eth->data = LWIP_MEM_ALIGN((void*)((uint8_t *)p_eth + sizeof(sub_data_t)));
		
			if(app_client->next == NULL) {
				app_client->next = p_eth;
			}
			else {
				p_eth_next = app_client->next;
				while(p_eth_next->next != NULL) {
					p_eth_next = p_eth_next->next;
				}
				p_eth_next->next = p_eth;
			}
		
			p_eth->len = p->tot_len;
			p_eth->state = (void *)pcb;
			p_eth->next = NULL;
		
			data = (uint8_t*)p->payload;
			
			if(p->tot_len > p->len) {
				memcpy(p_eth->data, data, p->len);
				datasize = p->len;
				ptemp = p->next;
				while(ptemp != NULL) {
					data = (uint8_t*)ptemp->payload;
					memcpy((uint8_t*)p_eth->data + datasize, data, ptemp->len);
					datasize += ptemp->len;
					ptemp = ptemp->next; 
				}	
			}
			else {
				memcpy(p_eth->data, data, p->len);
			}
			app_client->dirty = 1;
		}
		pbuf_free(p);//Application's responsibility to free the pbuf	
	}

	if (err == ERR_OK && p == NULL) {
		server_close_connection(pcb, arg);
	}

	return ERR_OK;
}


/**
 * \brief Accept incoming server connection requests.
 *
 * \param arg Pointer to a structure of data buffer.
 * \param pcb Pointer to a TCP connection structure.
 * \param err Connection status.
 *
 * \return ERR_OK on success.
 */
static err_t server_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	LWIP_UNUSED_ARG(arg);
	LWIP_UNUSED_ARG(err);

	tcp_setprio(pcb, TCP_PRIO_MIN);

	/* Tell TCP that we wish to be informed of incoming data by a call
	to the server_recv() function. */
	tcp_recv(pcb, server_recv);
	
	/* Tell TCP that we wish be to informed of data that has been
	successfully sent by a call to the server_sent() function. */
	tcp_sent(pcb, server_sent);

	tcp_err(pcb, server_connection_err);

	return ERR_OK;
}

void start_server(void)
{
	struct tcp_pcb *server_pcb;

	server_pcb = tcp_new();
	if(server_pcb != NULL) {
		tcp_bind(server_pcb, IP_ADDR_ANY, LOCAL_SERVER_PORT);
		server_pcb = tcp_listen(server_pcb);
		tcp_accept(server_pcb, server_accept);
	}
	else {
		dbg_error("PCB allocated failed.\r\n");
	}
}

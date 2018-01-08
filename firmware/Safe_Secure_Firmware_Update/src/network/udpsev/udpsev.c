/**
 * \file
 *
 * \brief UDP Server.
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

#include "udpsev.h"
#include "app_debug.h"

ip_addr_t g_local_ip;

#define DEVICE_INFO		","MAC_ADDRESS",SAM4S-XPlained-PRO"

static void udp_server_recv(void *arg, struct udp_pcb *pcb, struct pbuf *p,
ip_addr_t *addr, u16_t port)
{
	char *data = NULL;
	struct pbuf *send_buf = NULL;
	char tbuf[64];
	
	if(p != NULL) {
		data = p->payload;
		if(strncmp(data, MAGIC_STRING, strlen(MAGIC_STRING)) == 0) {
			memset(tbuf, 0, sizeof(tbuf));
			strcpy(tbuf, inet_ntoa(g_local_ip));
			strcat((char*)tbuf, DEVICE_INFO);
			send_buf = pbuf_alloc(PBUF_TRANSPORT, 64, PBUF_RAM);
			memcpy((char*)send_buf->payload, tbuf, strlen(tbuf));
			send_buf->len = strlen(tbuf);
			send_buf->tot_len = strlen(tbuf);
			udp_sendto(pcb, send_buf, addr, port);
			pbuf_free(send_buf); //Actually, the send_buf is freed by the low level output function
		}
		pbuf_free(p);
	} 
}

void udp_server_init(void)
{
	struct udp_pcb *udp_server = NULL;
	
	g_local_ip.addr = 0;
	udp_server = udp_new();
	
	if(udp_server != NULL) {
		udp_bind(udp_server, IP_ADDR_ANY, UDP_SERVER_PORT);
		udp_recv(udp_server, udp_server_recv, NULL);
	}
	else {
		dbg_error("udp pcb alloc failed.\r\n");		
	}
}
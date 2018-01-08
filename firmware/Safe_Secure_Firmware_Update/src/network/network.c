/**
 * \file
 *
 * \brief Network.
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

#include "server/server.h"
#include "udpsev/udpsev.h"
#include "conf_uart_serial.h"
#include "uart.h"
#include "timer_mgt.h"
#include "board.h"
#include "pdc.h"
#include "network.h"

static app_from_eth_t *cur_pool = NULL;
app_from_eth_t data_from_eth[MAXIMUM_POOL_FOR_DATA_FROM_ETH];

/**
 * \brief Network init.
 */
void network_init(void)
{
	memset(data_from_eth, 0, sizeof(data_from_eth));

	/* Configure and enable interrupt for UART1. */
	NVIC_EnableIRQ((IRQn_Type)ID_UART1);

	udp_server_init();
	start_server();
}

void UART1_Handler(void)
{
	uint32_t status;
	Pdc *p_pdc = NULL;
	
	status = uart_get_status(CONF_UART);
	
	if(status & UART_SR_ENDTX){
		//PDC end of transmitter transfer
		if(cur_pool != NULL) {
			cur_pool->ongoing = 0;
		}
		
		p_pdc = uart_get_pdc_base(CONF_UART);
		pdc_disable_transfer(p_pdc, PERIPH_PTCR_TXTDIS);
		uart_disable_interrupt(CONF_UART, UART_IDR_ENDTX);
	}
}

static void pdc_uart_send_data(sub_data_t *pdata)
{
	Pdc *p_pdc = NULL;
	uint32_t pdc_status;
	pdc_packet_t packet;
	
	packet.ul_addr = (uint32_t)pdata->data;
	packet.ul_size = pdata->len;
	
	p_pdc = uart_get_pdc_base(CONF_UART);
	pdc_status = pdc_read_status(p_pdc);
	
	if((pdc_status & PERIPH_PTSR_TXTEN) == 0) {
		pdc_tx_init(p_pdc, &packet, NULL);
		pdc_enable_transfer(p_pdc, PERIPH_PTCR_TXTEN);
		uart_enable_interrupt(CONF_UART, UART_IER_ENDTX);
	}
}

void check_uart_data_pool()
{
	uint32_t i;
	sub_data_t *to_uart_data = NULL;
	sub_data_t *ptemp = NULL;
	
	if(cur_pool != NULL) {
		if(cur_pool->dirty == 1) {
			if(cur_pool->ongoing == 1) {
				return;
			}
			to_uart_data = cur_pool->next;
			if(to_uart_data != NULL) {
				ptemp = to_uart_data->next;
				/* Inform TCP that APP has taken the data. */
				tcp_recved((struct tcp_pcb*)to_uart_data->state, to_uart_data->len);
				mem_free(to_uart_data);
				cur_pool->next = ptemp;
				to_uart_data = ptemp;
			}

			if(to_uart_data != NULL) {
				cur_pool->ongoing = 1;
				pdc_uart_send_data(to_uart_data);
				return;
			}
			else {
				cur_pool->dirty = 0;
				cur_pool->ipaddr = 0;
				cur_pool->port = 0;
				cur_pool->next = NULL;
				cur_pool = NULL;
			}
		}
	}
	if(cur_pool == NULL) {
		for(i = 0; i < MAXIMUM_POOL_FOR_DATA_FROM_ETH; i++) {
			if(data_from_eth[i].dirty == 1) {
				cur_pool = &data_from_eth[i];
				break;
			}
		}
		if(cur_pool != NULL) {
			if(cur_pool->next != NULL) {
				cur_pool->ongoing = 1;		
				pdc_uart_send_data(cur_pool->next);
			}
		}
	}
}
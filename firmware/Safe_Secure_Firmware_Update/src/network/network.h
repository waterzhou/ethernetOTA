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


#ifndef NETWORK_H_
#define NETWORK_H_

#define MAXIMUM_POOL_FOR_DATA_FROM_ETH	3

typedef struct _sub_data_t {
	uint32_t len;
	void *data;
	/*Here we use state to point the tcp_pcb structure*/
	void *state;
	struct _sub_data_t *next;
}sub_data_t;

typedef struct _app_from_eth_t {
	uint32_t dirty;
	uint32_t ongoing;
	uint32_t ipaddr;
	uint32_t port;
	sub_data_t *next;
}app_from_eth_t;

typedef struct _app_to_eth_t {
	uint32_t len;
	uint32_t left;
	uint32_t acked;
	void *data;
	struct _app_to_eth_t *next;
}app_to_eth_t;

void network_init(void);
void check_uart_data_pool(void);
extern app_from_eth_t data_from_eth[MAXIMUM_POOL_FOR_DATA_FROM_ETH];

#endif /* NETWORK_H_ */
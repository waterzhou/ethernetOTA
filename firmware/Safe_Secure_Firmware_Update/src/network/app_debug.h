/**
 * \file
 *
 * \brief Application Debug.
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

#ifndef APP_DEBUG_H_
#define APP_DEBUG_H_


/* Open debug information output */
//#define __APP_DEBUG__

/**
 * \name Wrappers for printing debug information
 *
 * \note The debug primitives feeds its output to printf. It is left up to
 * the real application to set up the stream.
 */
//@{
#ifdef __APP_DEBUG__

#define DBGFMT        "File: %s, Function: %s, Line: %d\r\n"
#define DBGARGS       __FILE__, __FUNCTION__, __LINE__

#  define dbg(__fmt_)                                                          \
	printf(__fmt_)
#  define dbg_info(__fmt_, ...)                                                \
	printf(__fmt_, ##__VA_ARGS__)
#  define dbg_error(__fmt_, ...)                                                       \
	do { \
		printf(__fmt_, ##__VA_ARGS__); \
		printf(DBGFMT, DBGARGS); \
	}while(0)
#  define dbg_val(__fmt_)

#  define dbg_putchar(c)                                                       \
	putc(c, stdout)
#  define dbg_vprintf_pgm(...)                                                 \
	vfprintf(stdout, __VA_ARGS__)

#else

#  define dbg(__fmt_)
#  define dbg_val(__fmt_)
#  define dbg_info(__fmt_, ...)
#  define dbg_error(...)
#endif


#endif /* APP_DEBUG_H_ */
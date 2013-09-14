/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 * $Id: example-trickle.c,v 1.2 2009/03/12 21:58:21 adamdunkels Exp $
 */

/**
 * \file
 *         Example for using the trickle code in Rime
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "net/rime/trickle.h"

#include "dev/leds.h"

#include <stdio.h>

struct parameters {
	rtimer_clock_t on_time;
	rtimer_clock_t off_time;
	uint8_t max_transmissions;
};

/*---------------------------------------------------------------------------*/
PROCESS(example_trickle_process, "Trickle example")
;
AUTOSTART_PROCESSES(&example_trickle_process);
/*---------------------------------------------------------------------------*/
static void parameters_received(struct trickle_conn *c) {
	static struct parameters p;
	packetbuf_copyto(&p);
	printf("%d.%d: received parameters (on_time = %u, off_time = %u, max_transmissions = %u)\n",
			rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
			p.on_time, p.off_time, p.max_transmissions);
}
/*---------------------------------------------------------------------------*/
const static struct trickle_callbacks cb = { parameters_received };
static struct trickle_conn trickle;
/*---------------------------------------------------------------------------*/
static void send_parameters(struct parameters *p) {
	packetbuf_copyfrom(p, sizeof(struct parameters));
	trickle_send(&trickle);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_trickle_process, ev, data) {
	PROCESS_EXITHANDLER(trickle_close(&trickle);)
	PROCESS_BEGIN();

		trickle_open(&trickle, CLOCK_SECOND, 128, &cb);

		if(!(rimeaddr_node_addr.u8[0] == 1 && rimeaddr_node_addr.u8[1] == 0)) {
			PROCESS_WAIT_EVENT_UNTIL(0);
		}

		static struct etimer et;
		while(1) {
			etimer_set(&et, CLOCK_SECOND * 100);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

			static struct parameters p;
			p.on_time = 25;
			p.off_time = 2000;
			p.max_transmissions = 3;
			send_parameters(&p);
		}
		PROCESS_END();
	}
	/*---------------------------------------------------------------------------*/

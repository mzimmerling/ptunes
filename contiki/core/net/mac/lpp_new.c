/*
 * Copyright 2013 ETH Zurich and SICS Swedish ICT
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
 */

/*
 * author: Federico Ferrari <ferrari@tik.ee.ethz.ch>
 */

#include "dev/leds.h"
#include "lib/random.h"
#include "net/rime.h"
#include "net/mac/lpp_new.h"
#include "sys/energest.h"
#include "contiki-conf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if MAC_PROTOCOL == LPP_NEW
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
#define TYPE_PROBE            0
#define TYPE_DATA             1
#define TYPE_ACK              2

#define WITH_DATA_ACK         1
#if WITH_DATA_ACK
#define WAIT_FOR_DATA_ACK RTIMER_SECOND/240
#endif /* WITH_DATA_ACK */

#ifdef LPP_NEW_DEFAULT_ON_TIME
#define ON_TIME LPP_NEW_DEFAULT_ON_TIME
#else
#define ON_TIME (RTIMER_SECOND / 128)
#endif /** LP_CONF_ON_TIME */

#ifdef LPP_NEW_DEFAULT_OFF_TIME
#define OFF_TIME LPP_NEW_DEFAULT_OFF_TIME
#else
#define OFF_TIME (RTIMER_SECOND / 2)
#endif /* LPP_CONF_OFF_TIME */

/*---------------------------------------------------------------------------*/
struct lpp_new_config lpp_new_config = {
	ON_TIME,
	OFF_TIME,
	LPP_NEW_DEFAULT_LISTEN_TIME
};

struct lpp_new_hdr {
	uint16_t type;
	rimeaddr_t sender;
	rimeaddr_t receiver;
};

struct send_buffer {
	uint8_t packet[PACKETBUF_SIZE];
	uint16_t len;
	rimeaddr_t dest;
	uint8_t valid, is_broadcast, timed_out, probe_received, got_data_ack, add_timestamp;
	rtimer_clock_t t_start;
};

/*---------------------------------------------------------------------------*/
static uint8_t lpp_new_is_on, dutycycle_on;
static uint32_t handshakes_total, handshakes_succ, probes_total, probes_succ = 0;
#if !GLOSSY
static struct rtimer rt;
#endif /* GLOSSY */
static struct pt pt_dutycycle;
static struct send_buffer buffer;
static rtimer_clock_t now;
#if EXCLUDE_TRICKLE_ENERGY
static unsigned long energest_listen, energest_transmit = 0;
#endif /* EXCLUDE_TRICKLE_ENERGY */

/*---------------------------------------------------------------------------*/
static const struct radio_driver *radio;
static void (* receiver_callback)(const struct mac_driver *);

/*---------------------------------------------------------------------------*/
static inline void
radio_on(void) {
	radio->on();
}

/*---------------------------------------------------------------------------*/
static inline void
radio_off(void) {
	radio->off();
}

/*---------------------------------------------------------------------------*/
static void
send_probe(void) {
	// construct the probe packet
	struct lpp_new_hdr probe;
	probe.type = TYPE_PROBE;
	rimeaddr_copy(&probe.sender, &rimeaddr_node_addr);
	rimeaddr_copy(&probe.receiver, &rimeaddr_node_addr);
	// send the probe packet
	radio->send((const uint8_t *) &probe, sizeof(struct lpp_new_hdr));
}

/*---------------------------------------------------------------------------*/
PROCESS(send_probe_process, "LPP_NEW send probe process");

/*---------------------------------------------------------------------------*/
#if GLOSSY
char dutycycle(void) {
	PT_BEGIN(&pt_dutycycle);

	while (1) {
		now = TBR;
		TBCCR3 = now + lpp_new_config.on_time;
		if (lpp_new_is_on) {
			process_poll(&send_probe_process);
		}
		PT_YIELD(&pt_dutycycle);

		now = TBR;
		rtimer_clock_t random_off_time = random_rand() % (LPP_NEW_MAX_RANDOM_OFF_TIME + 1);
		TBCCR3 = now + lpp_new_config.off_time + random_off_time;
		if (lpp_new_is_on) {
			leds_off(LEDS_BLUE);
			dutycycle_on = 0;
			if (!buffer.valid) {
				// turn off the radio only if we have no packets waiting to be sent
				radio_off();
			}
		}
#if GLOSSY
		if (TIME_TO_GLOSSY < 2 * lpp_new_config.off_time + random_off_time + lpp_new_config.on_time) {
			// do not schedule the next on phase of the duty-cycle: Glossy is approaching
			TBCCTL3 = 0;
		}
#endif /* GLOSSY */
		PT_YIELD(&pt_dutycycle);
	}

	PT_END(&pt_dutycycle);
}
#else /* GLOSSY */
static char
dutycycle(struct rtimer *t, void *ptr) {
	PT_BEGIN(&pt_dutycycle);

	while (1) {
		now = RTIMER_NOW();
		rtimer_set(t, now + lpp_new_config.on_time, 1,
				(rtimer_callback_t)dutycycle, ptr);
		if (lpp_new_is_on) {
			process_poll(&send_probe_process);
		}
		PT_YIELD(&pt_dutycycle);

		now = RTIMER_NOW();
		rtimer_clock_t random_off_time = random_rand() % (LPP_NEW_MAX_RANDOM_OFF_TIME + 1);
		rtimer_set(t, now + lpp_new_config.off_time + random_off_time, 1,
				(rtimer_callback_t)dutycycle, ptr);
		if (lpp_new_is_on) {
			leds_off(LEDS_BLUE);
			dutycycle_on = 0;
			if (!buffer.valid) {
				// turn off the radio only if we have no packets waiting to be sent
				radio_off();
			}
		}
		PT_YIELD(&pt_dutycycle);
	}

	PT_END(&pt_dutycycle);
}
#endif /* GLOSSY */

/*---------------------------------------------------------------------------*/
static inline void
remove_packet_from_buffer(void) {
	buffer.valid = 0;
	// disable the send timeout timer
#if GLOSSY
	TBCCTL2 = 0;
#else
	TACCTL0 = 0;
#endif /* GLOSSY */
	// turn off the radio only if the dutycycle is not on
	GIO3_OFF();
	leds_off(LEDS_GREEN);
	if (!dutycycle_on) {
		radio_off();
	}
	if (!buffer.is_broadcast) {
		// update the handshake counters
		probes_total++;
		if (probes_total == 2) {
			// two probes are equivalent to one handshake
//			handshakes_total++;
//			handshakes_succ += (probes_succ >> 1);
			probes_total = 0;
			probes_succ = 0;
		}
		if (handshakes_total >= HANDSHAKES_RESET_PERIOD) {
			// halve the handshake counters
			handshakes_total >>= 1;
			handshakes_succ >>= 1;
		}
		// notify relunicast that the transmission attempt completed
		post_send(buffer.got_data_ack);
	}
#if EXCLUDE_TRICKLE_ENERGY
	else {
		unsigned long diff_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT) - energest_transmit;
		unsigned long diff_listen = energest_type_time(ENERGEST_TYPE_LISTEN) - energest_listen;
		energest_type_set(ENERGEST_TYPE_LPM, energest_type_time(ENERGEST_TYPE_LPM) + diff_listen + diff_transmit);
		energest_type_set(ENERGEST_TYPE_LISTEN, energest_listen);
		energest_type_set(ENERGEST_TYPE_TRANSMIT, energest_transmit);
	}
#endif /* EXCLUDE_TRICKLE_ENERGY */
}

/*---------------------------------------------------------------------------*/
static int
send_packet(void) {
	if (!lpp_new_is_on) {
		post_send(0);
		return 0;
	}

	if (buffer.valid) {
		if (rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &rimeaddr_null)) {
			// we are already sending a packet and the new one is a broadcast: just discard it
			return 0;
		}
#if EXCLUDE_TRICKLE_ENERGY
		if (buffer.is_broadcast &&
				!rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &rimeaddr_null)) {
			// we are already sending a broadcast packet and the new one is a unicast:
			// remove the packet from the buffer
			remove_packet_from_buffer();
		}
#endif /* EXCLUDE_TRICKLE_ENERGY */
	}
	// create the LPP header for the data packet and copy it to the packetbuf
	struct lpp_new_hdr hdr;
	hdr.type = TYPE_DATA;
	rimeaddr_copy(&hdr.sender, &rimeaddr_node_addr);
	rimeaddr_copy(&hdr.receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	if (rimeaddr_cmp(&hdr.receiver, &rimeaddr_null)) {
		buffer.is_broadcast = 1;
#if EXCLUDE_TRICKLE_ENERGY
		energest_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
		energest_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
#endif /* EXCLUDE_TRICKLE_ENERGY */
	} else {
		buffer.is_broadcast = 0;
	}
	packetbuf_hdralloc(sizeof(struct lpp_new_hdr));
	memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct lpp_new_hdr));
	packetbuf_compact();
	// copy the content of the packetbuf to the send buffer
	buffer.len = packetbuf_totlen();
	rimeaddr_copy(&buffer.dest, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	memcpy(buffer.packet, packetbuf_hdrptr(), buffer.len);
	if (packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP)) {
		buffer.add_timestamp = 1;
	} else {
		buffer.add_timestamp = 0;
	}
	// clear the packetbuf
	packetbuf_clear();
	buffer.timed_out = 0;
	buffer.got_data_ack = 0;
	buffer.valid = 1;
	buffer.probe_received = 0;
#if GLOSSY
	// set and enable the send timeout timer (Timer B)
	buffer.t_start = TBR;
	if (!buffer.is_broadcast) {
		TBCCR2 = buffer.t_start + lpp_new_config.tx_timeout;
	} else {
		TBCCR2 = buffer.t_start + 2*lpp_new_config.on_time + lpp_new_config.off_time + LPP_NEW_MAX_RANDOM_OFF_TIME;
	}
	TBCCTL2 = CCIE;
#else
	// set and enable the send timeout timer (Timer A!!!)
	buffer.t_start = TAR;
	if (!buffer.is_broadcast) {
		TACCR0 = buffer.t_start + lpp_new_config.tx_timeout;
	} else {
		TACCR0 = buffer.t_start + 2*lpp_new_config.on_time + lpp_new_config.off_time + LPP_NEW_MAX_RANDOM_OFF_TIME;
	}
	TACCTL0 = CCIE;
#endif /* GLOSSY */
	// turn on the radio for possible probes
	GIO3_ON();
	radio_on();
	leds_on(LEDS_GREEN);
	return 1;
}

/*---------------------------------------------------------------------------*/
static inline void
send_data_packet(void) {
	if (buffer.is_broadcast) {
		if (buffer.add_timestamp) {
			packetbuf_set_attr(PACKETBUF_ATTR_TIMESTAMP, 1);
		} else {
			packetbuf_set_attr(PACKETBUF_ATTR_TIMESTAMP, 0);
		}
		radio->send(buffer.packet, buffer.len);
		packetbuf_set_attr(PACKETBUF_ATTR_TIMESTAMP, 0);
		buffer.probe_received = 0;
	} else {
		// consider only the first probe that could have been received
		if (RTIMER_CLOCK_LT(RTIMER_NOW(), buffer.t_start +
				2*lpp_new_config.on_time + lpp_new_config.off_time +
				LPP_NEW_MAX_RANDOM_OFF_TIME)) {
			probes_succ++;
		}
		packetbuf_set_attr(PACKETBUF_ATTR_TIMESTAMP, 0);
		radio->send(buffer.packet, buffer.len);
#if WITH_DATA_ACK
		struct lpp_new_hdr data_ack;
		rtimer_clock_t t = RTIMER_NOW();
		while (buffer.got_data_ack == 0 &&
				RTIMER_CLOCK_LT(RTIMER_NOW(), t + WAIT_FOR_DATA_ACK)) {
			int len = radio->read((uint8_t *) &data_ack, sizeof(struct lpp_new_hdr));
			if (len > 0) {
				if (rimeaddr_cmp(&data_ack.sender, &buffer.dest)
						&& rimeaddr_cmp(&data_ack.receiver, &rimeaddr_node_addr)) {
					PRINTF("LPP_NEW: send_packet: got DATA_ACK\n");
					buffer.got_data_ack = 1;
				}
			}
		}
		handshakes_total++;
		if (buffer.got_data_ack) {
			handshakes_succ++;
		}
#endif /* WITH_DATA_ACK */
		remove_packet_from_buffer();
	}
}

/*---------------------------------------------------------------------------*/
static int
read_packet(void) {
	packetbuf_clear();
	uint8_t len = radio->read(packetbuf_dataptr(), PACKETBUF_SIZE);

	if (len > 0) {
		packetbuf_set_datalen(len);
		struct lpp_new_hdr *hdr = packetbuf_dataptr();
		packetbuf_hdrreduce(sizeof(struct lpp_new_hdr));

		if (hdr->type == TYPE_PROBE) {
			// we are interested in a probe only if we have a valid packet in the buffer
			// and either it is a broadcast or the probe comes from the recipient
			if (buffer.valid && (buffer.is_broadcast || rimeaddr_cmp(&hdr->sender, &buffer.dest))) {
				buffer.probe_received = 1;
				send_data_packet();
			}
		} else if (hdr->type == TYPE_DATA) {
			if (rimeaddr_cmp(&hdr->receiver, &rimeaddr_node_addr) ||
					rimeaddr_cmp(&hdr->receiver, &rimeaddr_null)) {
				// we are interested in a data packet only if it is for us or it is a broadcast
				packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &hdr->sender);
#if WITH_DATA_ACK
				if (!rimeaddr_cmp(&hdr->receiver, &rimeaddr_null)) {
					packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &hdr->receiver);
					struct lpp_new_hdr data_ack;
					data_ack.type = TYPE_ACK;
					rimeaddr_copy(&data_ack.sender, &hdr->receiver);
					rimeaddr_copy(&data_ack.receiver, &hdr->sender);
					radio->send((uint8_t *) &data_ack, sizeof(struct lpp_new_hdr));
				}
#endif /* WITH_DATA_ACK */
				return packetbuf_totlen();
			}
		}
	}
	return 0;
}

/*---------------------------------------------------------------------------*/
static void
set_receive_function(void (* recv)(const struct mac_driver *)) {
	receiver_callback = recv;
}

/*---------------------------------------------------------------------------*/
static void
input_packet(const struct radio_driver *d)
{
	if (receiver_callback) {
		receiver_callback(&lpp_new_driver);
	}
}

/*---------------------------------------------------------------------------*/
static int
mac_on(void) {
	lpp_new_is_on = 1;
#if GLOSSY
	TBCCR3 = TBR + (random_rand() % lpp_new_config.off_time);
	TBCCTL3 = CCIE;
#else /* GLOSSY */
	rtimer_set(&rt, RTIMER_NOW() + lpp_new_config.off_time, 1,
			(rtimer_callback_t)dutycycle, NULL);
#endif /* GLOSSY */

	return 1;
}

/*---------------------------------------------------------------------------*/
static int
mac_off(int keep_radio_on) {
	TBCCTL2 = 0;
	TBCCTL3 = 0;
	lpp_new_is_on = 0;
	if (keep_radio_on) {
		radio_on();
	} else {
		radio_off();
	}
	if (buffer.valid) {
		remove_packet_from_buffer();
	}
	return 1;
}

/*---------------------------------------------------------------------------*/
const struct mac_driver lpp_new_driver = {
		"LPP_NEW", lpp_new_init, send_packet,
		read_packet, set_receive_function, mac_on, mac_off
};

/*---------------------------------------------------------------------------*/
PROCESS(send_timeout_process, "LPP_NEW send timeout process");

/*---------------------------------------------------------------------------*/
const struct mac_driver *
lpp_new_init(const struct radio_driver *d) {
	radio = d;
	radio->set_receive_function(input_packet);

	PT_INIT(&pt_dutycycle);
	process_start(&send_timeout_process, NULL);
	process_start(&send_probe_process, NULL);

	lpp_new_is_on = 1;
	buffer.valid = 0;
	radio_off();
	dutycycle_on = 0;
	GIO2_INIT();
	GIO3_INIT();
#if GLOSSY
	TBCCR3 = TBR + lpp_new_config.off_time;
	TBCCTL3 = CCIE;
#else /* GLOSSY */
	rtimer_set(&rt, RTIMER_NOW() + lpp_new_config.off_time, 1,
			(rtimer_callback_t)dutycycle, NULL);
#endif /* GLOSSY */

	return &lpp_new_driver;
}

/*---------------------------------------------------------------------------*/
const struct lpp_new_config *
get_lpp_new_config() {
	return &lpp_new_config;
}

/*---------------------------------------------------------------------------*/
void
set_lpp_new_config(const struct lpp_new_config *config) {
	mac_off(0);
	lpp_new_config.on_time = config->on_time;
	lpp_new_config.off_time = config->off_time;
	lpp_new_config.tx_timeout = config->tx_timeout;
	PRINTF("%u %u %u\n",
			lpp_new_config.on_time,
			lpp_new_config.off_time,
			lpp_new_config.tx_timeout);
	mac_on();
}

/*---------------------------------------------------------------------------*/
uint16_t lpp_new_get_prr() {
	PRINTF("handshakes_succ %lu, handshakes_total %lu\n", handshakes_succ, handshakes_total);
	return (uint16_t) (((handshakes_succ+1)*1000) / (handshakes_total+1));
}

/*---------------------------------------------------------------------------*/
void lpp_new_reset_prr() {
	handshakes_total = 0;
	handshakes_succ = 0;
}

/*---------------------------------------------------------------------------*/
#if !GLOSSY
// send timeout timer interrupt
interrupt(TIMERA0_VECTOR) timera0 (void) {
	ENERGEST_ON(ENERGEST_TYPE_IRQ);

	TACCTL0 = 0;
	process_poll(&send_timeout_process);
	LPM4_EXIT;

	ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
#endif /* !GLOSSY */

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(send_timeout_process, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("send_timeout_process: started\n");

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
    if (buffer.valid) {
    	remove_packet_from_buffer();
    }
  }

  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(send_probe_process, ev, data)
{
  PROCESS_BEGIN();

  PRINTF("send_probe_process: started\n");

  while(1) {
    PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
    leds_on(LEDS_BLUE);
    dutycycle_on = 1;
    radio_on();
    send_probe();
  }

  PROCESS_END();
}

#endif /* MAC_PROTOCOL */

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
 * $Id: xmac.c,v 1.35 2009/08/20 18:59:17 adamdunkels Exp $
 */

/**
 * \file
 *         A simple power saving MAC protocol based on X-MAC [SenSys 2006]
 * \author
 *         Adam Dunkels <adam@sics.se>
 *         Marco Zimmerling <zimmerling@tik.ee.ethz.ch>
 *         Federico Ferrari <ferrari@tik.ee.ethz.ch>
 */

#include "sys/pt.h"
#include "net/mac/xmac.h"
#include "sys/rtimer.h"
#include "dev/leds.h"
#include "net/rime.h"
#include "net/rime/timesynch.h"
#include "dev/radio.h"
#include "dev/watchdog.h"
#include "lib/random.h"
#include "sys/energest.h"

#include "sys/compower.h"

#include "contiki-conf.h"

#include <string.h>

#define WITH_CHANNEL_CHECK           0    /* Seems to work badly when enabled */
#define WITH_TIMESYNCH               0
#define WITH_QUEUE                   0
#define WITH_ACK_OPTIMIZATION        0
#define WITH_RANDOM_WAIT_BEFORE_SEND 0
#define WITH_DATA_ACK                1

struct announcement_data {
	uint16_t id;
	uint16_t value;
};

/* The maximum number of announcements in a single announcement
   message - may need to be increased in the future. */
#define ANNOUNCEMENT_MAX 10

/* The structure of the announcement messages. */
struct announcement_msg {
	uint16_t num;
	struct announcement_data data[ANNOUNCEMENT_MAX];
};

/* The length of the header of the announcement message, i.e., the
   "num" field in the struct. */
#define ANNOUNCEMENT_MSG_HEADERLEN (sizeof (uint16_t))

#define TYPE_STROBE       0
#define TYPE_DATA         1
#define TYPE_ANNOUNCEMENT 2
#define TYPE_STROBE_ACK   3
#define TYPE_DATA_ACK     4

struct xmac_hdr {
	uint16_t type;
	rimeaddr_t sender;
	rimeaddr_t receiver;
};

#ifdef XMAC_CONF_ON_TIME
#define DEFAULT_ON_TIME (XMAC_CONF_ON_TIME)
#else
#define DEFAULT_ON_TIME (RTIMER_ARCH_SECOND / 200)
#endif

#ifdef XMAC_CONF_OFF_TIME
#define DEFAULT_OFF_TIME (XMAC_CONF_OFF_TIME)
#else
#define DEFAULT_OFF_TIME (RTIMER_ARCH_SECOND / 2 - DEFAULT_ON_TIME)
#endif

#define DEFAULT_STROBE_WAIT_TIME (7 * DEFAULT_ON_TIME / 8)

/* The cycle time for announcements. */
#define ANNOUNCEMENT_PERIOD 4 * CLOCK_SECOND

/* The time before sending an announcement within one announcement
   cycle. */
#define ANNOUNCEMENT_TIME (random_rand() % (ANNOUNCEMENT_PERIOD))

struct xmac_config xmac_config = {
	RTIMER_SECOND/178, // 23; min = 23, max = 43
	RTIMER_SECOND/2,
	2*(RTIMER_SECOND/178) + RTIMER_SECOND/2, // 2*Ton + Toff
	RTIMER_SECOND/240 // 17
};

#include <stdio.h>
#if !GLOSSY
static struct rtimer rt;
#endif /* !GLOSSY */
static struct pt pt;

static int xmac_is_on = 0;
static int got_data_ack = 0;

static volatile unsigned char waiting_for_packet = 0;
static volatile unsigned char we_are_sending = 0;
static volatile unsigned char radio_is_on = 0;

static volatile int was_timeout = 0;
static volatile rtimer_clock_t last_on = 0;

static uint32_t handshakes_total = 0;
static uint32_t handshakes_succ = 0;

static const struct radio_driver *radio;

#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE

#define LEDS_ON(x) leds_on(x)
#define LEDS_OFF(x) leds_off(x)
#define LEDS_TOGGLE(x) leds_toggle(x)
#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#undef LEDS_ON
#undef LEDS_OFF
#undef LEDS_TOGGLE
#define LEDS_ON(x)
#define LEDS_OFF(x)
#define LEDS_TOGGLE(x)
#define PRINTF(...)
#endif

#if XMAC_CONF_ANNOUNCEMENTS
/* Timers for keeping track of when to send announcements. */
static struct ctimer announcement_cycle_ctimer, announcement_ctimer;

static int announcement_radio_txpower;

/* Flag that is used to keep track of whether or not we are listening
   for announcements from neighbors. */
static uint8_t is_listening = 0;
#endif /* XMAC_CONF_ANNOUNCEMENTS */



static void (* receiver_callback)(const struct mac_driver *);

#if XMAC_CONF_COMPOWER
static struct compower_activity current_packet;
#endif /* XMAC_CONF_COMPOWER */

#define TIMEOUT_TIME RTIMER_SECOND/166
#define T_WAIT RTIMER_SECOND/408

#if !GLOSSY
static char powercycle(struct rtimer *t, void *ptr);
static void timeout(struct rtimer *t, void *ptr);
#endif /* !GLOSSY */

/*---------------------------------------------------------------------------*/
static void set_receive_function(void(* recv)(const struct mac_driver *)) {
	receiver_callback = recv;
}
/*---------------------------------------------------------------------------*/
static void on(void) {
	if (xmac_is_on && radio_is_on == 0) {
		radio_is_on = 1;
		leds_on(LEDS_BLUE);
		radio->on();
	}
}
/*---------------------------------------------------------------------------*/
static void off(void) {
	if (xmac_is_on && radio_is_on != 0) {
		radio_is_on = 0;
		leds_off(LEDS_BLUE);
		radio->off();
	}
}
/*---------------------------------------------------------------------------*/
#if GLOSSY
void timeout(void) {
  waiting_for_packet = 0;
  leds_off(LEDS_GREEN);
  if (we_are_sending == 0)
  {
	off();
  }
  TBCCR3 = last_on + xmac_config.on_time + xmac_config.off_time;
  TBCCTL3 = CCIE;
}
#else
static void timeout(struct rtimer *t, void *ptr) {
  // it is assumed that the timeout rtimer always expires within
  // the off phase of powercycle immediately after the first received strobe
  waiting_for_packet = 0;
  leds_off(LEDS_GREEN);
  if (we_are_sending == 0)
  {
    off();
  }
  rtimer_set(t, last_on + xmac_config.on_time + xmac_config.off_time, 1,
	     (void (*)(struct rtimer *, void *))powercycle, ptr);
}
#endif /* GLOSSY */
/*---------------------------------------------------------------------------*/
#if GLOSSY
static volatile int in_on_phase = 0;
char powercycle(void) {
	PT_BEGIN(&pt);

	while(1)
	{
		if((xmac_config.off_time > 0) && (!was_timeout))
		{
			if(we_are_sending == 0)
			{
				in_on_phase = 1;
				off();
			}

			if(xmac_is_on)
			{
				TBCCR3 += xmac_config.off_time;
			}
			PT_YIELD(&pt);
		}

		last_on = TBCCR3;
		was_timeout = 0;

		if(we_are_sending == 0)
		{
			in_on_phase = 0;
			on();
		}
		if(xmac_is_on)
		{
			TBCCR3 += xmac_config.on_time;
			in_on_phase = 0;
		}
		PT_YIELD(&pt);
	}

	PT_END(&pt);
}
#else /* GLOSSY */
static char powercycle(struct rtimer *t, void *ptr) {
	int r;

	PT_BEGIN(&pt);

	while(1)
	{
		if((xmac_config.off_time > 0) && (!was_timeout))
		{
			if(we_are_sending == 0)
			{
				off();
			}

			if(xmac_is_on)
			{
				r = rtimer_set(t, RTIMER_TIME(t) + xmac_config.off_time, 1,
					(void (*)(struct rtimer *, void *))powercycle, ptr);

			}
			PT_YIELD(&pt);
		}

		last_on = RTIMER_TIME(t);
		was_timeout = 0;

		if(we_are_sending == 0)
		{
			on();
		}
		if(xmac_is_on)
		{
			r = rtimer_set(t, RTIMER_TIME(t) + xmac_config.on_time, 1,
					(void (*)(struct rtimer *, void *))powercycle, ptr);
		}
		PT_YIELD(&pt);
	}

	PT_END(&pt);
}
#endif /* GLOSSY */
/*---------------------------------------------------------------------------*/
#if XMAC_CONF_ANNOUNCEMENTS
static int
parse_announcements(rimeaddr_t *from)
{
  /* Parse incoming announcements */
  struct announcement_msg *adata = packetbuf_dataptr();
  int i;

  /*  printf("%d.%d: probe from %d.%d with %d announcements\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	 from->u8[0], from->u8[1], adata->num);*/
  /*  for(i = 0; i < packetbuf_datalen(); ++i) {
    printf("%02x ", ((uint8_t *)packetbuf_dataptr())[i]);
  }
  printf("\n");*/

  for(i = 0; i < adata->num; ++i) {
    /*   printf("%d.%d: announcement %d: %d\n",
	  rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	  adata->data[i].id,
	  adata->data[i].value);*/

    announcement_heard(from,
		       adata->data[i].id,
		       adata->data[i].value);
  }
  return i;
}
/*---------------------------------------------------------------------------*/
static int
format_announcement(char *hdr)
{
  struct announcement_msg *adata;
  struct announcement *a;

  /* Construct the announcements */
  adata = (struct announcement_msg *)hdr;

  adata->num = 0;
  for(a = announcement_list();
      a != NULL && adata->num < ANNOUNCEMENT_MAX;
      a = a->next) {
    adata->data[adata->num].id = a->id;
    adata->data[adata->num].value = a->value;
    adata->num++;
  }

  if(adata->num > 0) {
    return ANNOUNCEMENT_MSG_HEADERLEN +
      sizeof(struct announcement_data) * adata->num;
  } else {
    return 0;
  }
}
#endif /* XMAC_CONF_ANNOUNCEMENTS */
/*---------------------------------------------------------------------------*/
static int send_packet(void) {
	if (!xmac_is_on) {
		return 1;
	}

	struct {
		struct xmac_hdr hdr;
	} strobe, ack;

	volatile int len = 0;
	rtimer_clock_t t, t0;
	got_data_ack = 0;

#if WITH_RANDOM_WAIT_BEFORE_SEND
	{
		rtimer_clock_t t = RTIMER_NOW() + (random_rand() % (xmac_config.on_time * 4));
		while(RTIMER_CLOCK_LT(RTIMER_NOW(), t));
	}
#endif /* WITH_RANDOM_WAIT_BEFORE_SEND */

#if WITH_CHANNEL_CHECK
	/* Check if there are other strobes in the air. */
	waiting_for_packet = 1;
	on();
	t0 = RTIMER_NOW();
	while(RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + xmac_config.strobe_wait_time * 2)) {
		len = radio->read(&strobe.hdr, sizeof(strobe.hdr));
		if(len > 0) {
			someone_is_sending = 1;
		}
	}
	waiting_for_packet = 0;

	while(someone_is_sending);

#endif /* WITH_CHANNEL_CHECK */

	/* By setting we_are_sending to one, we ensure that the rtimer
	 powercycle interrupt do not interfere with us sending the packet. */
	we_are_sending = 1;

	off();

	/* Create the X-MAC header for the data packet. We cannot do this
	 in-place in the packet buffer, because we cannot be sure of the
	 alignment of the header in the packet buffer. */
	struct xmac_hdr hdr;
	hdr.type = TYPE_DATA;
	rimeaddr_copy(&hdr.sender, &rimeaddr_node_addr);
	rimeaddr_copy(&hdr.receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	int is_broadcast = 0;
	if (rimeaddr_cmp(&hdr.receiver, &rimeaddr_null)) {
		is_broadcast = 1;
	}

	/* Copy the X-MAC header to the header portion of the packet buffer. */
	packetbuf_hdralloc(sizeof(struct xmac_hdr));
	memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct xmac_hdr));
	packetbuf_compact();

	watchdog_stop();

	/* Construct the strobe packet. */
	strobe.hdr.type = TYPE_STROBE;
	rimeaddr_copy(&strobe.hdr.sender, &rimeaddr_node_addr);
	rimeaddr_copy(&strobe.hdr.receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));

	/* Turn on the radio to listen for the strobe ACK. */
	if (!is_broadcast) {
		on();
	}

#if EXCLUDE_TRICKLE_ENERGY
	unsigned long energest_listen = 0;
	unsigned long energest_transmit = 0;
	if (is_broadcast) {
		energest_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
		energest_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
	}
#endif /* EXCLUDE_TRICKLE_ENERGY */

	/* Send strobes until the receiver replies with an ACK */
	int strobes = 0;
	int got_strobe_ack = 0;
	int interferred = 0;
	rtimer_clock_t strobe_wait_time;
	t0 = RTIMER_NOW();
	for (strobes = 0; got_strobe_ack == 0 && interferred == 0 && RTIMER_CLOCK_LT(RTIMER_NOW(), t0 + xmac_config.strobe_time); strobes++) {
		if (is_broadcast){
			/* Send the data packet. */
			radio->send(packetbuf_hdrptr(), packetbuf_totlen());
			strobe_wait_time = xmac_config.strobe_wait_time;
		} else {
			/* Send the strobe packet. */
			radio->send((const uint8_t *) &strobe, sizeof(struct xmac_hdr));
			strobe_wait_time = xmac_config.strobe_wait_time;
		}

		t = RTIMER_NOW();
		while (got_strobe_ack == 0 && interferred == 0 && RTIMER_CLOCK_LT(RTIMER_NOW(), t + strobe_wait_time)) {
			/* See if we got an ACK */
			if (!is_broadcast) {
				len = radio->read((uint8_t *) &ack, sizeof(struct xmac_hdr));
				if (len > 0) {
					if (  ack.hdr.type == TYPE_STROBE_ACK
					   && rimeaddr_cmp(&ack.hdr.sender, &rimeaddr_node_addr)
					   && rimeaddr_cmp(&ack.hdr.receiver, &rimeaddr_node_addr)) {
						/* We got an ACK from the receiver, so we can immediately send the packet. */
						got_strobe_ack = 1;
					}// else if (ack.hdr.type != TYPE_DATA_ACK) {
						/* We got a STROBE or a DATA packet, so we immediately stop strobing. */
					//	interferred = 1;
					//}
				}
			}
		}
	}
	if (!is_broadcast) {
		handshakes_total++;
		if (got_strobe_ack) {
			handshakes_succ++;
		}
		// update the handshake counters
		if (handshakes_total >= HANDSHAKES_RESET_PERIOD) {
			handshakes_total >>= 1;
			handshakes_succ >>= 1;
		}
	}

#if EXCLUDE_TRICKLE_ENERGY
	if (is_broadcast) {
		unsigned long diff_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT) - energest_transmit;
		unsigned long diff_listen = energest_type_time(ENERGEST_TYPE_LISTEN) - energest_listen;
		energest_type_set(ENERGEST_TYPE_LPM, energest_type_time(ENERGEST_TYPE_LPM) + diff_listen + diff_transmit - xmac_config.on_time);
		energest_type_set(ENERGEST_TYPE_LISTEN, energest_listen + xmac_config.on_time);
		energest_type_set(ENERGEST_TYPE_TRANSMIT, energest_transmit);
	}
#endif /* EXCLUDE_TRICKLE_ENERGY */

	/* If we have received the strobe ACK, and we are sending a packet
	 that will need an upper layer ACK (as signified by the
	 PACKETBUF_ATTR_RELIABLE packet attribute), we keep the radio on. */
	PRINTF("xmac: send_packet: got_strobe_ack = %i, PACKETBUF_ATTR_RELIABLE = %u, PACKETBUF_ATTR_ERELIABLE = %u",
			got_strobe_ack, packetbuf_attr(PACKETBUF_ATTR_RELIABLE), packetbuf_attr(PACKETBUF_ATTR_ERELIABLE));

	if (  got_strobe_ack
	   && (  packetbuf_attr(PACKETBUF_ATTR_RELIABLE)
		  || packetbuf_attr(PACKETBUF_ATTR_ERELIABLE))) {
#if WITH_ACK_OPTIMIZATION
		on(); /* Wait for ACK packet */
		waiting_for_packet = 1;
#else /* WITH_ACK_OPTIMIZATION */
		off();
#endif /* WITH_ACK_OPTIMIZATION */
	} else {
#if WITH_DATA_ACK
		if(got_strobe_ack) {
			on();
			//waiting_for_packet = 1;
		} else {
			off();
		}
#else /* WITH_DATA_ACK */
		off();
#endif /* WITH_DATA_ACK */
	}

	/* Send the data packet. */
	//if (is_broadcast || got_strobe_ack) {
	if (!is_broadcast && got_strobe_ack) {
		PRINTF("x-mac: send_packet: sending DATA packet\n");
		radio->send(packetbuf_hdrptr(), packetbuf_totlen());

#if WITH_DATA_ACK
		if(!is_broadcast) {
			// Wait some time for DATA_ACK
			//PRINTF("x-mac: send_packet: waiting for DATA_ACK\n");
			handshakes_total++;
			t = RTIMER_NOW();
			while (got_data_ack == 0 && RTIMER_CLOCK_LT(RTIMER_NOW(), t + T_WAIT)) {
				// Check whether we got a DATA_ACK
				len = radio->read((uint8_t *) &strobe, sizeof(struct xmac_hdr));
				if (len > 0) {
					if (  rimeaddr_cmp(&strobe.hdr.sender, &rimeaddr_node_addr)
							&& rimeaddr_cmp(&strobe.hdr.receiver, &rimeaddr_node_addr)) {
						PRINTF("x-mac: send_packet: got DATA_ACK\n");
						got_data_ack = 1;
						handshakes_succ++;
					}
				}
			}
			// update the handshake counters
			if (handshakes_total >= HANDSHAKES_RESET_PERIOD) {
				handshakes_total >>= 1;
				handshakes_succ >>= 1;
			}
		}
#endif /* WITH_DATA_ACK */

		off();
		waiting_for_packet = 0;
	}

	watchdog_start();

	PRINTF("xmac: send (strobes=%u,len=%u,%s), done\n", strobes, packetbuf_totlen(), got_strobe_ack ? "ack" : "no ack");

#if XMAC_CONF_COMPOWER
	/* Accumulate the power consumption for the packet transmission. */
	compower_accumulate(&current_packet);

	/* Convert the accumulated power consumption for the transmitted
	 packet to packet attributes so that the higher levels can keep
	 track of the amount of energy spent on transmitting the
	 packet. */
	compower_attrconv(&current_packet);

	/* Clear the accumulated power consumption so that it is ready for
	 the next packet. */
	compower_clear(&current_packet);
#endif /* XMAC_CONF_COMPOWER */

	we_are_sending = 0;

	return 1;
}
/*---------------------------------------------------------------------------*/
static void input_packet(const struct radio_driver *d) {
	if (receiver_callback) {
		receiver_callback(&xmac_driver);
	}
}
/*---------------------------------------------------------------------------*/
static int read_packet(void) {
	struct xmac_hdr *hdr;
	uint8_t len;

	packetbuf_clear();
	len = radio->read(packetbuf_dataptr(), PACKETBUF_SIZE);

	if (len > 0)
	{
		packetbuf_set_datalen(len);
		hdr = packetbuf_dataptr();

		packetbuf_hdrreduce(sizeof(struct xmac_hdr));

		if (hdr->type == TYPE_STROBE)
		{

			if (rimeaddr_cmp(&hdr->receiver, &rimeaddr_node_addr))
			{
				/* This is a strobe packet for us. */
				if (rimeaddr_cmp(&hdr->sender, &rimeaddr_node_addr))
				{
					/* If the sender address is our node address, the strobe is
					 a stray strobe ACK to us, which we ignore unless we are
					 currently sending a packet.  */
					//someone_is_sending = 0;
				}
				else
				{
					/* If the sender address is someone else, we set a timeout
					   unless there is already an active timeout. */

					/* Construct the STROBE_ACK. By using the same address as both
					   sender and receiver, we flag the message is a strobe ack.*/
					struct xmac_hdr msg;
					msg.type = TYPE_STROBE_ACK;
					rimeaddr_copy(&msg.receiver, &hdr->sender);
					rimeaddr_copy(&msg.sender, &hdr->sender);

					// we set the timeout rtimer only after receiving the first strobe
					// i.e. only when waiting_for_packet is zero
					if (!was_timeout)
					{
#if GLOSSY
						TBCCTL3 = 0;
						TBCCR2 = TBR + TIMEOUT_TIME;
						TBCCTL2 = CCIE;
#else
						rtimer_clock_t t = RTIMER_NOW();
						rtimer_reset(&rt, t + TIMEOUT_TIME, 1, (void (*)(struct rtimer *, void *))timeout, NULL);
#endif /* GLOSSY */
						was_timeout = 1;
						leds_on(LEDS_GREEN);

						/* We turn on the radio in anticipation of the DATA packet and
						   send the STROBE ACK. */
						waiting_for_packet = 1;
						on();
						radio->send((const uint8_t *) &msg, sizeof(struct xmac_hdr));
					}
				}
			}
			else if (rimeaddr_cmp(&hdr->receiver, &rimeaddr_null))
			{

				/* If the receiver address is null, the strobe is sent to
				 prepare for an incoming broadcast packet. If this is the
				 case, we turn on the radio and wait for the incoming
				 broadcast packet. */
				waiting_for_packet = 1;
				on();
			}

			/* We are done processing the strobe and we therefore return to the caller. */
			return RIME_OK;
		} else if (hdr->type == TYPE_DATA) {
			if (rimeaddr_cmp(&hdr->receiver, &rimeaddr_node_addr) || rimeaddr_cmp(&hdr->receiver, &rimeaddr_null)) {
				/* This is a regular packet that is destined to us or to the broadcast address. */

				/* We have received the final packet, so we can go back to being asleep. */
				off();

				/* Set sender and receiver packet attributes */
				if (!rimeaddr_cmp(&hdr->receiver, &rimeaddr_null)) {
					packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &hdr->receiver);
				}
				packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &hdr->sender);

#if WITH_DATA_ACK
				/* Send DATA_ACK, but only if it was a unicast DATA packet. */
				if (!rimeaddr_cmp(&hdr->receiver, &rimeaddr_null)) {
					struct xmac_hdr msg;
					msg.type = TYPE_DATA_ACK;
					rimeaddr_copy(&msg.receiver, &hdr->sender);
					rimeaddr_copy(&msg.sender, &hdr->sender);
					radio->send((const uint8_t *) &msg, sizeof(struct xmac_hdr));
					PRINTF("x-mac: read_packet: sent DATA_ACK\n");
				}
#endif /* WITH_DATA_ACK */

#if XMAC_CONF_COMPOWER
				/* Accumulate the power consumption for the packet reception. */
				compower_accumulate(&current_packet);
				/* Convert the accumulated power consumption for the received
				 packet to packet attributes so that the higher levels can
				 keep track of the amount of energy spent on receiving the
				 packet. */
				compower_attrconv(&current_packet);

				/* Clear the accumulated power consumption so that it is ready
				 for the next packet. */
				compower_clear(&current_packet);
#endif /* XMAC_CONF_COMPOWER */

				//someone_is_sending = 0;
				waiting_for_packet = 0;

				return packetbuf_totlen();
			}
#if XMAC_CONF_ANNOUNCEMENTS
		} else if(hdr->type == TYPE_ANNOUNCEMENT) {
			parse_announcements(&hdr->sender);
#endif /* XMAC_CONF_ANNOUNCEMENTS */
		}
	}

	return 0;
}
/*---------------------------------------------------------------------------*/
#if XMAC_CONF_ANNOUNCEMENTS
static void
send_announcement(void *ptr)
{
  struct xmac_hdr *hdr;
  int announcement_len;

  /* Set up the probe header. */
  packetbuf_clear();
  packetbuf_set_datalen(sizeof(struct xmac_hdr));
  hdr = packetbuf_dataptr();
  hdr->type = TYPE_ANNOUNCEMENT;
  rimeaddr_copy(&hdr->sender, &rimeaddr_node_addr);
  rimeaddr_copy(&hdr->receiver, &rimeaddr_null);

  announcement_len = format_announcement((char *)hdr +
					 sizeof(struct xmac_hdr));

  if(announcement_len > 0) {
    packetbuf_set_datalen(sizeof(struct xmac_hdr) + announcement_len);

    packetbuf_set_attr(PACKETBUF_ATTR_RADIO_TXPOWER, announcement_radio_txpower);
    radio->send(packetbuf_hdrptr(), packetbuf_totlen());
  }
}
/*---------------------------------------------------------------------------*/
static void
cycle_announcement(void *ptr)
{
  ctimer_set(&announcement_ctimer, ANNOUNCEMENT_TIME,
	     send_announcement, NULL);
  ctimer_set(&announcement_cycle_ctimer, ANNOUNCEMENT_PERIOD,
	     cycle_announcement, NULL);
  if(is_listening > 0) {
    is_listening--;
    /*    printf("is_listening %d\n", is_listening);*/
  }
}
/*---------------------------------------------------------------------------*/
static void
listen_callback(int periods)
{
  is_listening = periods + 1;
}
#endif /* XMAC_CONF_ANNOUNCEMENTS */
/*---------------------------------------------------------------------------*/
void
xmac_set_announcement_radio_txpower(int txpower)
{
#if XMAC_CONF_ANNOUNCEMENTS
  announcement_radio_txpower = txpower;
#endif /* XMAC_CONF_ANNOUNCEMENTS */
}
/*---------------------------------------------------------------------------*/
const struct mac_driver *
xmac_init(const struct radio_driver *d) {
	radio_is_on = 0;
	waiting_for_packet = 0;
	PT_INIT(&pt);
#if GLOSSY
	TBCCR3 = TBR + xmac_config.off_time;
	TBCCTL3 = CCIE;
#else /* GLOSSY */
	rtimer_set(&rt, RTIMER_NOW() + xmac_config.off_time, 1, (void(*)(
			struct rtimer *, void *)) powercycle, NULL);
#endif /* GLOSSY */
	xmac_is_on = 1;
	radio = d;
	radio->set_receive_function(input_packet);

#if XMAC_CONF_ANNOUNCEMENTS
  announcement_register_listen_callback(listen_callback);
  ctimer_set(&announcement_cycle_ctimer, ANNOUNCEMENT_TIME,
	     cycle_announcement, NULL);
#endif /* XMAC_CONF_ANNOUNCEMENTS */
	return &xmac_driver;
}
/*---------------------------------------------------------------------------*/
int turn_on(void) {
	xmac_is_on = 1;
#if GLOSSY
	if(in_on_phase) {
		TBCCR3 = TBR + (random_rand() % xmac_config.off_time);
	} else {
		TBCCR3 = TBR + xmac_config.on_time;
	}
	TBCCTL3 = CCIE;
#else
	rtimer_reset(&rt, RTIMER_NOW() + xmac_config.off_time, 1, (void(*)(struct rtimer *, void *)) powercycle, NULL);
#endif /* GLOSSY */
	return 1;
}
/*---------------------------------------------------------------------------*/
int turn_off(int keep_radio_on) {
	xmac_is_on = 0;
	if (keep_radio_on) {
		leds_on(LEDS_BLUE);
		return radio->on();
	} else {
		leds_off(LEDS_BLUE);
		return radio->off();
	}
}
/*---------------------------------------------------------------------------*/
const struct mac_driver xmac_driver = { "X-MAC", xmac_init, send_packet,
		read_packet, set_receive_function, turn_on, turn_off };
/*---------------------------------------------------------------------------*/
const struct xmac_config *
get_xmac_config() {
	return &xmac_config;
}
/*---------------------------------------------------------------------------*/
void set_xmac_config(const struct xmac_config *config) {
	turn_off(0);
	xmac_config.on_time = config->on_time;
	xmac_config.off_time = config->off_time;
	xmac_config.strobe_time = config->strobe_time;
	xmac_config.strobe_wait_time = config->strobe_wait_time;
	PRINTF("%u %u %u %u\n",
			xmac_config.on_time,
			xmac_config.off_time,
			xmac_config.strobe_time,
			xmac_config.strobe_wait_time);

	turn_on();
}
/*---------------------------------------------------------------------------*/
void printFlags() {
	PRINTF("x-mac flags: xmac_is_on = %i\n", xmac_is_on);
	PRINTF("x-mac flags: waiting_for_packet = %u\n", waiting_for_packet);
	PRINTF("x-mac flags: someone_is_sending = %u\n", someone_is_sending);
	PRINTF("x-mac flags: we_are_sending = %u\n", we_are_sending);
	PRINTF("x-mac flags: radio_is_on = %u\n", radio_is_on);
}
/*---------------------------------------------------------------------------*/
int xmac_got_data_ack(){
  return got_data_ack;
}
/*---------------------------------------------------------------------------*/
uint16_t xmac_get_prr() {
  PRINTF("handshakes_succ %lu, handshakes_total %lu\n", handshakes_succ, handshakes_total);
  if (handshakes_succ && handshakes_total) {
	  return (uint16_t) ((handshakes_succ*1000) / handshakes_total);
  } else {
	  // avoid returning 0 as a PRR
	  return (uint16_t) 1;
  }
}
/*---------------------------------------------------------------------------*/
void xmac_reset_prr(){
  handshakes_total = 0;
  handshakes_succ = 0;
}
/*---------------------------------------------------------------------------*/

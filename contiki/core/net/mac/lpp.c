/*
 * Copyright (c) 2008, Swedish Institute of Computer Science.
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
 * $Id: lpp.c,v 1.25 2009/09/09 21:09:23 adamdunkels Exp $
 */

/**
 * \file
 *         Low power probing (R. Musaloiu-Elefteri, C. Liang,
 *         A. Terzis. Koala: Ultra-Low Power Data Retrieval in
 *         Wireless Sensor Networks, IPSN 2008)
 *
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 *
 * This is an implementation of the LPP (Low-Power Probing) MAC
 * protocol. LPP is a power-saving MAC protocol that works by sending
 * a probe packet each time the radio is turned on. If another node
 * wants to transmit a packet, it can do so after hearing the
 * probe. To send a packet, the sending node turns on its radio to
 * listen for probe packets.
 *
 */

#include "dev/spi.h"
#include "dev/leds.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime.h"
#include "net/mac/mac.h"
#include "net/mac/lpp.h"
#include "net/rime/packetbuf.h"
#include "net/rime/announcement.h"
#include "sys/compower.h"
#include "sys/energest.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define WITH_DATA_ACK                 1
#define WITH_LPP_ANNOUNCEMENTS        0

#ifdef LPP_DEFAULT_ON_TIME
#define ON_TIME LPP_DEFAULT_ON_TIME
#else
#define ON_TIME (CLOCK_SECOND / 128)
#endif /** LP_CONF_ON_TIME */

#ifdef LPP_DEFAULT_OFF_TIME
#define OFF_TIME LPP_DEFAULT_OFF_TIME
#else
#define OFF_TIME (CLOCK_SECOND / 2)
#endif /* LPP_CONF_OFF_TIME */

/* If CLOCK_SECOND is less than 4, we may end up with an OFF_TIME that
   is 0 which will make compilation fail due to a modulo operation in
   the code. To ensure that OFF_TIME is greater than zero, we use the
   construct below. */
#if OFF_TIME == 0
#undef OFF_TIME
#define OFF_TIME 1
#endif

extern void rel_send_queued_packet(void);

static uint32_t handshakes_total = 0;
static uint32_t handshakes_succ = 0;

#if WITH_DATA_ACK
static int got_data_ack;
#define WAIT_FOR_DATA_ACK RTIMER_SECOND/240 // 17
#endif /* WITH_DATA_ACK */

#ifdef QUEUEBUF_CONF_NUM
#define MAX_QUEUED_PACKETS QUEUEBUF_CONF_NUM / 2
#else /* QUEUEBUF_CONF_NUM */
#define MAX_QUEUED_PACKETS 4
#endif /* QUEUEBUF_CONF_NUM */

struct lpp_config lpp_config = {
		ON_TIME,		// 1
		OFF_TIME,		// 64
		2*ON_TIME + OFF_TIME + LPP_RANDOM_OFF_TIME_ADJUSTMENT // 66
};

#define UNICAST_TIMEOUT lpp_config.tx_timeout
#define BROADCAST_TIMEOUT_FACTOR 1

#if EXCLUDE_TRICKLE_ENERGY
static unsigned long energest_probe, energest_on, diff_probe, diff_on = 0;
#endif /* EXCLUDE_TRICKLE_ENERGY */

struct announcement_data {
	uint16_t id;
	uint16_t value;
};

#define ANNOUNCEMENT_MSG_HEADERLEN 2
struct announcement_msg {
	uint16_t num;
	struct announcement_data data[];
};

#define LPP_PROBE_HEADERLEN 2

#define TYPE_PROBE        1
#define TYPE_DATA         2
#define TYPE_ACK          3
struct lpp_hdr {
	uint16_t type;
	rimeaddr_t sender;
	rimeaddr_t receiver;
};

static uint8_t lpp_is_on;

static struct compower_activity current_packet;

static const struct radio_driver *radio;
static void (* receiver_callback)(const struct mac_driver *);
static struct pt dutycycle_pt;
static struct ctimer timer;

#if WITH_LPP_ANNOUNCEMENTS
static uint8_t is_listening = 0;
#endif /* WITH_LPP_ANNOUNCEMENTS */

struct queue_list_item {
	struct queue_list_item *next;
	struct queuebuf *packet;
	struct ctimer removal_timer;
	struct compower_activity compower;
	uint8_t acknowledged;
#if EXCLUDE_TRICKLE_ENERGY
	unsigned long energest_listen;
	unsigned long energest_transmit;
#endif /* EXCLUDE_TRICKLE_ENERGY */
};

#define BROADCAST_FLAG_NONE    0
#define BROADCAST_FLAG_WAITING 1
#define BROADCAST_FLAG_PENDING 2
#define BROADCAST_FLAG_SEND    3

LIST(pending_packets_list);
LIST(queued_packets_list);
MEMB(queued_packets_memb, struct queue_list_item, MAX_QUEUED_PACKETS);

extern void post_send(int acknowledged);

/*---------------------------------------------------------------------------*/
static void
turn_radio_on(void)
{
	radio->on();
//	leds_on(LEDS_BLUE);
}
/*---------------------------------------------------------------------------*/
static void
turn_radio_off(void)
{
	if(lpp_is_on) {
		radio->off();
//		leds_off(LEDS_BLUE);
	}
}

/*---------------------------------------------------------------------------*/
/* The outbound packet is put on either the pending_packets_list or
   the queued_packets_list, depending on if the packet should be sent
   immediately.
 */
static void
turn_radio_on_for_neighbor(rimeaddr_t *neighbor, struct queue_list_item *i)
{

	if(rimeaddr_cmp(neighbor, &rimeaddr_null)) {
		/* We have been asked to turn on the radio for a broadcast, so we
       just turn on the radio. */
#if EXCLUDE_TRICKLE_ENERGY
		i->energest_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
		i->energest_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
#endif /* EXCLUDE_TRICKLE_ENERGY */
		turn_radio_on();
		list_add(queued_packets_list, i);
		leds_on(LEDS_BLUE);
		return;
	}

	/* We did not find the neighbor in the list of recent encounters, so
     we just turn on the radio. */
	/*  printf("Neighbor %d.%d not found in recent encounters\n",
      neighbor->u8[0], neighbor->u8[1]);*/
	turn_radio_on();
	list_add(queued_packets_list, i);
	leds_on(LEDS_BLUE);
	return;
}
/*---------------------------------------------------------------------------*/
static void
remove_queued_packet(void *item)
{
	struct queue_list_item *i = item;
	uint8_t acknowledged = i->acknowledged;

	PRINTF("%d.%d: removing queued packet\n",
			rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);

	int was_broadcast = 0;
	struct lpp_hdr *qhdr;
	qhdr = queuebuf_dataptr(i->packet);
	if(rimeaddr_cmp(&qhdr->receiver, &rimeaddr_null))
		was_broadcast = 1;

	ctimer_stop(&i->removal_timer);
	queuebuf_free(i->packet);
	list_remove(pending_packets_list, i);
	list_remove(queued_packets_list, i);
	if(list_length(queued_packets_list) == 0)
		leds_off(LEDS_BLUE);

	/* XXX potential optimization */
#if WITH_LPP_ANNOUNCEMENTS
	if (is_listening == 0)
#endif /* WITH_LPP_ANNOUNCEMENTS */
	{
		if(list_length(queued_packets_list) == 0) {
			turn_radio_off();
			compower_accumulate(&i->compower);
		}
	}

	if (!was_broadcast)
	{
		post_send((int)acknowledged);
	}
#if EXCLUDE_TRICKLE_ENERGY
	else
	{
		unsigned long diff_listen, diff_transmit = 0;
		diff_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT) - i->energest_transmit;
		diff_listen = energest_type_time(ENERGEST_TYPE_LISTEN) - i->energest_listen;
//		printf("diff_listen %lu, diff_transmit %lu, diff_on %lu, diff_probe %lu\n", diff_listen, diff_transmit, diff_on, diff_probe);
		energest_type_set(ENERGEST_TYPE_LPM, energest_type_time(ENERGEST_TYPE_LPM) + diff_listen + diff_transmit - (diff_on + diff_probe) * BROADCAST_TIMEOUT_FACTOR);
		energest_type_set(ENERGEST_TYPE_LISTEN, i->energest_listen + diff_on * BROADCAST_TIMEOUT_FACTOR);
		energest_type_set(ENERGEST_TYPE_TRANSMIT, i->energest_transmit + diff_probe * BROADCAST_TIMEOUT_FACTOR);
	}
#endif /* EXCLUDE_TRICKLE_ENERGY */
	memb_free(&queued_packets_memb, i);
	rel_send_queued_packet();
}
/*---------------------------------------------------------------------------*/
static void
listen_callback(int periods)
{
#if WITH_LPP_ANNOUNCEMENTS
	is_listening = periods;
#endif /* WITH_LPP_ANNOUNCEMENTS */
	turn_radio_on();
}
/*---------------------------------------------------------------------------*/
/**
 * Send a probe packet.
 */
static int
send_probe(void)
{
	struct lpp_hdr *msg;
	packetbuf_clear();
	packetbuf_set_datalen(sizeof(struct lpp_hdr));
	msg = packetbuf_dataptr();
	msg->type = TYPE_PROBE;
	rimeaddr_copy(&msg->sender, &rimeaddr_node_addr);
	rimeaddr_copy(&msg->receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	packetbuf_set_datalen(sizeof(struct lpp_hdr));
	radio->send(packetbuf_hdrptr(), sizeof(struct lpp_hdr));
//	struct lpp_hdr *hdr;
//	struct announcement_msg *adata;
//	struct announcement *a;
//
//	/* Set up the probe header. */
//	packetbuf_clear();
//	packetbuf_set_datalen(sizeof(struct lpp_hdr));
//	hdr = packetbuf_dataptr();
//	hdr->type = TYPE_PROBE;
//	rimeaddr_copy(&hdr->sender, &rimeaddr_node_addr);
//	rimeaddr_copy(&hdr->receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
//
//
//	/* Construct the announcements */
//	adata = (struct announcement_msg *)((char *)hdr + sizeof(struct lpp_hdr));
//
//	adata->num = 0;
//	for(a = announcement_list(); a != NULL; a = a->next) {
//		adata->data[adata->num].id = a->id;
//		adata->data[adata->num].value = a->value;
//		adata->num++;
//	}
//
//	packetbuf_set_datalen(sizeof(struct lpp_hdr) +
//			ANNOUNCEMENT_MSG_HEADERLEN +
//			sizeof(struct announcement_data) * adata->num);
//
//	/*  PRINTF("Sending probe\n");*/
//
//	/*  printf("probe\n");*/
//
//	/* XXX should first check access to the medium (CCA - Clear Channel
//     Assessment) and add LISTEN_TIME to off_time_adjustment if there
//     is a packet in the air. */
//	radio->send(packetbuf_hdrptr(), packetbuf_totlen());

	compower_accumulate(&compower_idle_activity);
	return 1;
}
/*---------------------------------------------------------------------------*/
static int
num_packets_to_send(void)
{
	return list_length(queued_packets_list);
}
/*---------------------------------------------------------------------------*/
/**
 * Duty cycle the radio and send probes. This function is called
 * repeatedly by a ctimer. The function restart_dutycycle() is used to
 * (re)start the duty cycling.
 */
static int
dutycycle(void *ptr)
{

	struct ctimer *t = ptr;
	static clock_time_t last_on_time;

	PT_BEGIN(&dutycycle_pt);

	while(1) {
		/* Turn on the radio for sending a probe packet and
       anticipating a data packet from a neighbor. */
#if EXCLUDE_TRICKLE_ENERGY
		energest_on = energest_type_time(ENERGEST_TYPE_LISTEN);
		energest_probe = energest_type_time(ENERGEST_TYPE_TRANSMIT);
#endif /* EXCLUDE_TRICKLE_ENERGY */
		leds_on(LEDS_GREEN);
		turn_radio_on();

		/* Send a probe packet. */
		send_probe();
#if EXCLUDE_TRICKLE_ENERGY
		diff_probe = energest_type_time(ENERGEST_TYPE_TRANSMIT) - energest_probe;
//		printf("diff_probe %lu\n", diff_probe);
#endif /* EXCLUDE_TRICKLE_ENERGY */

		/* Set a timer so that we keep the radio on for LISTEN_TIME. */
		ctimer_set(t, lpp_config.on_time, (void (*)(void *))dutycycle, t);
		last_on_time = clock_time();
		PT_YIELD(&dutycycle_pt);

		// correct a possible desynchronization in the duty cycle
		// (happens at the sink when printing after receiving a packet)
		clock_time_t displacement = (clock_time_t)(clock_time() - last_on_time - lpp_config.on_time);

		/* If we have no packets to send (indicated by the list length of
       queued_packets_list being zero), we should turn the radio
       off. Othersize, we keep the radio on. */
		if(num_packets_to_send() == 0) {

			/* If we are not listening for announcements, we turn the radio
	 off and wait until we send the next probe. */
#if WITH_LPP_ANNOUNCEMENTS
			is_listening = 0; //TODO: hack
			if(is_listening == 0) {
#endif /* WITH_LPP_ANNOUNCEMENTS */
				turn_radio_off();
				leds_off(LEDS_GREEN);
				compower_accumulate(&compower_idle_activity);
#if EXCLUDE_TRICKLE_ENERGY
				diff_on = energest_type_time(ENERGEST_TYPE_LISTEN) - energest_on;
//				printf("diff_on %lu\n", diff_on);
#endif /* EXCLUDE_TRICKLE_ENERGY */

				// random sleep delay in order to desynchronize beacons
				// use rtimer instead of ctimer for a finer granularity
//				printf("t_tx = %lu t_rx = %lu t_lpm + t_cpu = %lu\n",
//						energest_type_time(ENERGEST_TYPE_TRANSMIT),
//						energest_type_time(ENERGEST_TYPE_LISTEN),
//						energest_type_time(ENERGEST_TYPE_CPU) + energest_type_time(ENERGEST_TYPE_LPM));

				// increase off_time to a random amount of {0, 1, ..., LPP_RANDOM_OFF_TIME_ADJUSTMENT} etimer ticks
				clock_time_t tr = random_rand() % LPP_RANDOM_OFF_TIME_ADJUSTMENT;
				if(rimeaddr_node_addr.u8[0] == SINK_ID && rimeaddr_node_addr.u8[1] == 0 && displacement < 10) {
					ctimer_set(t, (clock_time_t) (lpp_config.off_time + tr - displacement), (void (*)(void *))dutycycle, t);
				} else {
					ctimer_set(t, (clock_time_t) (lpp_config.off_time + tr), (void (*)(void *))dutycycle, t);
				}
//				if (displacement)
//					printf("displacement %u\n", displacement);
				PT_YIELD(&dutycycle_pt);

#if WITH_LPP_ANNOUNCEMENTS
			} else {
				/* We are listening for annonucements, so we count down the
	   listen time, and keep the radio on. */
				leds_off(LEDS_GREEN);
				is_listening--;
				ctimer_set(t, lpp_config.off_time, (void (*)(void *))dutycycle, t);
				PT_YIELD(&dutycycle_pt);
			}
#endif /* WITH_LPP_ANNOUNCEMENTS */
		} else {
			/* We had pending packets to send, so we do not turn the radio off. */

			ctimer_set(t, lpp_config.off_time, (void (*)(void *))dutycycle, t);
//			turn_radio_on(); // in the rare case the radio has been turned off after sending an ack
			leds_off(LEDS_GREEN);
			PT_YIELD(&dutycycle_pt);
		}
	}

	PT_END(&dutycycle_pt);
}
/*---------------------------------------------------------------------------*/
static void
restart_dutycycle(clock_time_t initial_wait)
{
	PT_INIT(&dutycycle_pt);
	ctimer_set(&timer, initial_wait, (void (*)(void *))dutycycle, &timer);
}
/*---------------------------------------------------------------------------*/
/**
 *
 * Send a packet. This function builds a complete packet with an LPP
 * header and queues the packet. When a probe is heard (in the
 * read_packet() function), and the sender of the probe matches the
 * receiver of the queued packet, the queued packet is sent.
 *
 * ACK packets are treated differently from other packets: if a node
 * sends a packet that it expects to be ACKed, the sending node keeps
 * its radio on for some time after sending its packet. So we do not
 * need to wait for a probe packet: we just transmit the ACK packet
 * immediately.
 *
 */
static int
send_packet(void)
{
	struct lpp_hdr hdr;
	clock_time_t timeout;
	uint8_t is_broadcast = 0;


	rimeaddr_copy(&hdr.sender, &rimeaddr_node_addr);
	rimeaddr_copy(&hdr.receiver, packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
	if(rimeaddr_cmp(&hdr.receiver, &rimeaddr_null)) {
		is_broadcast = 1;
	}
	hdr.type = TYPE_DATA;

	packetbuf_hdralloc(sizeof(struct lpp_hdr));
	memcpy(packetbuf_hdrptr(), &hdr, sizeof(struct lpp_hdr));

	packetbuf_compact();
	PRINTF("%d.%d: queueing packet to %d.%d, channel %d\n",
			rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
			hdr.receiver.u8[0], hdr.receiver.u8[1],
			packetbuf_attr(PACKETBUF_ATTR_CHANNEL));

	{
		struct queue_list_item *i;
		i = memb_alloc(&queued_packets_memb);
		if(i != NULL) {
			i->packet = queuebuf_new_from_packetbuf();
			if(i->packet == NULL) {
				memb_free(&queued_packets_memb, i);
				return 0;
			} else {
				if(is_broadcast) {
					timeout = lpp_config.tx_timeout * BROADCAST_TIMEOUT_FACTOR;
				} else {
					timeout = lpp_config.tx_timeout;
				}
				ctimer_set(&i->removal_timer, timeout, remove_queued_packet, i);
				i->acknowledged = 0;
				/* Wait for a probe packet from a neighbor. The actual packet
	   transmission is handled by the read_packet() function,
	   which receives the probe from the neighbor. */
				turn_radio_on_for_neighbor(&hdr.receiver, i);


			}
		}

	}
	return 1;
}


static void
send_packet_to_radio(struct queue_list_item *i) {

	int len;
	got_data_ack = 0;

	struct lpp_hdr *qhdr;
	qhdr = queuebuf_dataptr(i->packet);

	int is_broadcast = 0;
	if (rimeaddr_cmp(&qhdr->receiver, &rimeaddr_null)) {
		is_broadcast = 1;
	}

	queuebuf_to_packetbuf(i->packet);

		radio->send(queuebuf_dataptr(i->packet),
				queuebuf_datalen(i->packet));
		PRINTF("%d.%d: got a probe from %d.%d, sent packet to %d.%d\n",
				rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
				hdr->sender.u8[0], hdr->sender.u8[1],
				qhdr->receiver.u8[0], qhdr->receiver.u8[1]);



		/* Attribute the energy spent on listening for the probe
	to this packet transmission. */
		compower_accumulate(&i->compower);

	#if WITH_DATA_ACK
		struct lpp_hdr *hdr;
		if(!is_broadcast) {
			handshakes_total++;
			// Wait some time for DATA_ACK
			rtimer_clock_t t = RTIMER_NOW();
			while (got_data_ack == 0 && RTIMER_CLOCK_LT(RTIMER_NOW(), t + WAIT_FOR_DATA_ACK)) {
				// Check whether we got a DATA_ACK
				packetbuf_clear();
				len = radio->read(packetbuf_dataptr(), PACKETBUF_SIZE);
				if (len > 0) {
					packetbuf_set_datalen(len);
					hdr = packetbuf_dataptr();
					if(hdr->type == TYPE_ACK) {
						if (  rimeaddr_cmp(&hdr->sender, &rimeaddr_node_addr)
								&& rimeaddr_cmp(&hdr->receiver, &rimeaddr_node_addr)) {
							got_data_ack = 1;
							i->acknowledged = 1;
							handshakes_succ++;
						}
					}
				}
			}
			// update the handshake counters
			if (handshakes_total > HANDSHAKES_RESET_PERIOD) {
				handshakes_total >>= 1;
				handshakes_succ >>= 1;
			}
		}
	#endif /* WITH_DATA_ACK */

		/* If the packet was not a broadcast packet, we dequeue it
	now. Broadcast packets should be transmitted to all
	neighbors, and are dequeued by the dutycycling function
	instead, after the appropriate time. */
		if(!is_broadcast) {
			remove_queued_packet(i);
		}
}


/*---------------------------------------------------------------------------*/
/**
 * Read a packet from the underlying radio driver. If the incoming
 * packet is a probe packet and the sender of the probe matches the
 * destination address of the queued packet (if any), the queued packet
 * is sent.
 */
static int
read_packet(void)
{
	int len;
	struct lpp_hdr *hdr;

	packetbuf_clear();
	len = radio->read(packetbuf_dataptr(), PACKETBUF_SIZE);
	if(len > 0) {
		packetbuf_set_datalen(len);
		hdr = packetbuf_dataptr();
		packetbuf_hdrreduce(sizeof(struct lpp_hdr));
		/*    PRINTF("got packet type %d\n", hdr->type);*/

		if(hdr->type == TYPE_PROBE) {
			/* Parse incoming announcements */
//			struct announcement_msg *adata = packetbuf_dataptr();
//			int i;
//
//			/*	PRINTF("%d.%d: probe from %d.%d with %d announcements\n",
//		rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
//		hdr->sender.u8[0], hdr->sender.u8[1], adata->num);*/
//
//			for(i = 0; i < adata->num; ++i) {
//				/*	  PRINTF("%d.%d: announcement %d: %d\n",
//		  rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
//		  adata->data[i].id,
//		  adata->data[i].value);*/
//
//				announcement_heard(&hdr->sender,
//						adata->data[i].id,
//						adata->data[i].value);
//			}

			/* Go through the list of packets to be sent to see if any of
	 them match the sender of the probe, or if they are a
	 broadcast packet that should be sent. */
			if(list_length(queued_packets_list) > 0) {
				struct queue_list_item *i;
				for(i = list_head(queued_packets_list); i != NULL; i = i->next) {
					struct lpp_hdr *qhdr;
					qhdr = queuebuf_dataptr(i->packet);
					if(rimeaddr_cmp(&qhdr->receiver, &hdr->sender) ||
							rimeaddr_cmp(&qhdr->receiver, &rimeaddr_null)) {
						send_packet_to_radio(i);
					}
				}
			}

		} else if(hdr->type == TYPE_DATA) {
			PRINTF("%d.%d: got data from %d.%d\n",
					rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
					hdr->sender.u8[0], hdr->sender.u8[1]);

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


#if WITH_DATA_ACK
				/* Send DATA_ACK, but only if it was a unicast DATA packet. */
				if (rimeaddr_cmp(&hdr->receiver, &rimeaddr_node_addr)) {
					// store the data packet into a temporary buffer
					int l = packetbuf_datalen();
					uint8_t buffer[l];
					packetbuf_copyto((uint8_t *)buffer);
					// send the DATA ACK
					struct lpp_hdr *msg;
					packetbuf_clear();
					packetbuf_set_datalen(sizeof(struct lpp_hdr));
					msg = packetbuf_dataptr();
					msg->type = TYPE_ACK;
					rimeaddr_copy(&msg->sender, &hdr->sender);
					rimeaddr_copy(&msg->receiver, &hdr->sender);
					packetbuf_set_datalen(sizeof(struct lpp_hdr));
					radio->send(packetbuf_hdrptr(), sizeof(struct lpp_hdr));
					// restore the data packet into the packetbuf
					packetbuf_copyfrom((uint8_t *)buffer, l);
				}
#endif /* WITH_DATA_ACK */

				// turn the radio off only if we have nothing to send
				// (the radio was on because of duty cycle and not to catch a probe)
				if (num_packets_to_send() == 0)
					turn_radio_off();
		}

		len = packetbuf_datalen();
	}
	return len;
}
/*---------------------------------------------------------------------------*/
static void
set_receive_function(void (* recv)(const struct mac_driver *))
{
	receiver_callback = recv;
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
	lpp_is_on = 1;
	turn_radio_on();
	return 1;
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
	lpp_is_on = 0;
	if(keep_radio_on) {
		turn_radio_on();
	} else {
		turn_radio_off();
	}
	return 1;
}
/*---------------------------------------------------------------------------*/
const struct mac_driver lpp_driver = {
		"LPP",
		lpp_init,
		send_packet,
		read_packet,
		set_receive_function,
		on,
		off,
};
/*---------------------------------------------------------------------------*/
static void
input_packet(const struct radio_driver *d)
{
	if(receiver_callback) {
		receiver_callback(&lpp_driver);
	}
}
/*---------------------------------------------------------------------------*/
const struct mac_driver *
lpp_init(const struct radio_driver *d)
{
	radio = d;
	radio->set_receive_function(input_packet);
	restart_dutycycle(random_rand() % lpp_config.off_time);

	lpp_is_on = 1;
//	announcement_register_listen_callback(listen_callback);

	memb_init(&queued_packets_memb);
	list_init(queued_packets_list);
	list_init(pending_packets_list);
	return &lpp_driver;
}
/*---------------------------------------------------------------------------*/
const struct lpp_config *
get_lpp_config() {
	return &lpp_config;
}
/*---------------------------------------------------------------------------*/
void set_lpp_config(const struct lpp_config *config) {
	off(0);
	lpp_config.on_time = config->on_time;
	lpp_config.off_time = config->off_time;
	lpp_config.tx_timeout = config->tx_timeout;
	printf("%u %u %u\n",
			lpp_config.on_time,
			lpp_config.off_time,
			lpp_config.tx_timeout);

	on();
}
/*---------------------------------------------------------------------------*/
int lpp_got_data_ack() {
  return got_data_ack;
}
/*---------------------------------------------------------------------------*/
uint16_t lpp_get_prr() {
  PRINTF("handshakes_succ %lu, handshakes_total %lu\n", handshakes_succ, handshakes_total);
  return (handshakes_succ != 0) ? (uint16_t) ((handshakes_succ*1000) / handshakes_total) : 1000;
}
/*---------------------------------------------------------------------------*/
void lpp_reset_prr() {
  handshakes_total = 0;
  handshakes_succ = 0;
}
/*---------------------------------------------------------------------------*/
int lpp_queue_is_empty() {
	return (num_packets_to_send() == 0) ? 1 : 0;
}
/*---------------------------------------------------------------------------*/

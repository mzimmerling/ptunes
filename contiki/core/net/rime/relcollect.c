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
 * Copyright (c) 2006, Swedish Institute of Computer Science.
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
 * $Id: collect.c,v 1.28 2009/05/30 19:54:05 nvt-se Exp $
 */

/*
 * author: Marco Zimmerling <zimmerling@tik.ee.ethz.ch>
 *         Federico Ferrari <ferrari@tik.ee.ethz.ch>
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"

#include "net/rime.h"
#include "net/rime/neighbor.h"
#include "net/rime/relcollect.h"
#include "net/rime/packetqueue.h"

#if MAC_PROTOCOL == XMAC
  #include "net/mac/xmac.h"
#elif MAC_PROTOCOL == LPP
  #include "net/mac/lpp.h"
#endif /* MAC_PROTOCOL */

#include "dev/radio-sensor.h"
#include "dev/watchdog.h"
#include "dev/leds.h"

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <float.h>

#if GLOSSY
extern uint8_t parent_id;
#endif /* GLOSSY */
static const struct packetbuf_attrlist attributes[] = { RELCOLLECT_ATTRIBUTES PACKETBUF_ATTR_LAST };

// Buffer to avoid duplicate transmissions
#define NUM_RECENT_PACKETS 20
struct recent_packet {
  rimeaddr_t originator;
  uint8_t seqno;
};
static struct recent_packet recent_packets[NUM_RECENT_PACKETS];
static uint8_t recent_packet_ptr;

#if QUEUING_STATS
static uint32_t queuing_count = 0;
static uint32_t queuing_size_sum = 0;
static uint16_t dropped_packets_count = 0;
#endif /* QUEUING_STATS */

#define FORWARD_PACKET_LIFETIME 0
#define MAX_FORWARDING_QUEUE 50
PACKETQUEUE(forwarding_queue, MAX_FORWARDING_QUEUE);

#define SINK_METRIC 0
#define RTMETRIC_MAX RELCOLLECT_MAX_DEPTH

#ifndef COLLECT_CONF_ANNOUNCEMENTS
#define COLLECT_ANNOUNCEMENTS 1
#else
#define COLLECT_ANNOUNCEMENTS COLLECT_CONF_ANNOUNCEMENTS
#endif /* COLLECT_CONF_ANNOUNCEMENTS */

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
void rel_send_queued_packet(void) {

  struct queuebuf *q;
  struct neighbor *n;
  struct packetqueue_item *i;
  struct relcollect_conn *c;

  i = packetqueue_first(&forwarding_queue);
  if (i == NULL) {
    PRINTF("%d.%d: rel_send_queued_packet: nothing on queue\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    /* No packet on the queue, so there is nothing for us to send. */
    return;
  }
  c = packetqueue_ptr(i);
  if (c == NULL) {
    /* c should not be NULL, but we check it just to be sure. */
    PRINTF("%d.%d: rel_send_queued_packet: queue, c == NULL!\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    return;
  }
  
  if (c->forwarding) {
    /* If we are currently forwarding a packet, we wait until the
       packet is forwarded and try again then. */
    PRINTF("%d.%d: rel_send_queued_packet: queue, c is forwarding\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    return;
  }
  
#if MAC_PROTOCOL == LPP
  if (!lpp_queue_is_empty()) {
    PRINTF("d.%d: LPP queue not empty!\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
	leds_on(LEDS_RED);
    return;
  }
  leds_off(LEDS_RED);
#endif /* MAC_PROTOCOL */

  q = packetqueue_queuebuf(i);
  if (q != NULL) {
    PRINTF("%d.%d: rel_send_queued_packet: queue, q is on queue\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);

    queuebuf_to_packetbuf(q);
    
    /* Check if we have a neighbor at all. */
    n = neighbor_best();
    if (n != NULL) {
    	/* Check if our best neighbor is the node from which we received the packet (cycle!).
	       If so, we remove this node from our neighbor table and forward the packet to
	       the second best neighbor if such node exists. */
    	if (rimeaddr_cmp(&n->addr, packetbuf_addr(PACKETBUF_ADDR_SENDER))) {
    		neighbor_remove(&n->addr);
    		n = neighbor_best();
    	}
    }

    if (n != NULL) {
    	PRINTF("%d.%d: rel_send_queued_packet: sending packet to %d.%d\n",
   				rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
   				n->addr.u8[0], n->addr.u8[1]);

   		if (rimeaddr_cmp(&rimeaddr_node_addr, packetbuf_addr(PACKETBUF_ADDR_ESENDER))) {
   			if (c->parent_id != n->addr.u8[0]) {
   				PRINTF("%d.%d: new parent ID %d\n",
   						rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
   						n->addr.u8[0]);
   			}

   			c->parent_id = n->addr.u8[0];
#if GLOSSY
   			parent_id = n->addr.u8[0];
#endif /* GLOSSY */
   			packetbuf_set_attr(PACKETBUF_ATTR_RTMETRIC, (uint8_t) c->rtmetric);
   		}

#if QUEUING_STATS
   		/* Update queuing statistics */
   		queuing_count++;
   		queuing_size_sum += (uint32_t) get_queue_size(&forwarding_queue);
   		// Determine queuing delay; consider also overflow of clock_time()
   		uint16_t queuing_delay = 0;
   		clock_time_t dequeue_time = clock_time();
   		if (dequeue_time >= get_enqueue_time(i)) {
   			queuing_delay = (uint16_t)(dequeue_time - get_enqueue_time(i));
   		} else {
   			queuing_delay = (uint16_t)(65535 - get_enqueue_time(i) + dequeue_time + 1);
   		}

   		/* Add the local queuing delay to the delay already contained in the packet */
		packetbuf_set_attr(PACKETBUF_ATTR_QUEUING_DELAY, packetbuf_attr(PACKETBUF_ATTR_QUEUING_DELAY) + queuing_delay);

		/* Insert average queue size only if we are the originator of the packet */
		if (rimeaddr_cmp(&rimeaddr_node_addr, packetbuf_addr(PACKETBUF_ADDR_ESENDER))) {
			uint32_t queue_size_to_print = (queuing_size_sum * 10) / queuing_count;
			if (queue_size_to_print % 10 > 4) {
				queue_size_to_print = queue_size_to_print / 10 + 1;
			} else {
				queue_size_to_print = queue_size_to_print / 10;
			}
			packetbuf_set_attr(PACKETBUF_ATTR_QUEUE_SIZE, (uint8_t)queue_size_to_print);
		}

		/* Check if we should halve the statistics for averaging */
		if (queuing_count >= QUEUING_DELAY_RESET_PERIOD) {
			queuing_count >>= 1;
			queuing_size_sum >>= 1;
		}
#endif /* QUEUING_STATS */
   		c->forwarding = 1;
   		relunicast_send(&c->relunicast_conn,
   				&n->addr,
   				packetbuf_attr(PACKETBUF_ATTR_MAX_REXMIT),
   				(rimeaddr_t *)packetbuf_addr(PACKETBUF_ADDR_ESENDER));
    } else {
    	PRINTF("%d.%d: rel_send_queued_packet: didn't find any neighbor\n",
    			rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    }
  }
}
/*---------------------------------------------------------------------------*/
#if !STATIC
static void rel_update_rtmetric(struct relcollect_conn *tc) {
  struct neighbor *n;
  
  /* We should only update the rtmetric if we are not the sink. */
  if (tc->rtmetric != SINK_METRIC) {
    
    /* Find the neighbor with the lowest rtmetric. */
    n = neighbor_best();
    
    /* If n is NULL, we have no best neighbor. */
    if (n == NULL) {
      
      /* If we have don't have any neighbors, we set our rtmetric to
	 the maximum value to indicate that we do not have a route. */
      
      if (tc->rtmetric != RTMETRIC_MAX) {
	PRINTF("%d.%d: rel_update_rtmetric: didn't find a best neighbor, setting rtmetric to max\n",
	       rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
      }
      tc->rtmetric = RTMETRIC_MAX;
      
#if COLLECT_ANNOUNCEMENTS
      announcement_set_value(&tc->announcement, tc->rtmetric);
#endif /* COLLECT_ANNOUNCEMENTS */
    } else {
      
      /* We set our rtmetric to the rtmetric of our best neighbor plus
	 the expected transmissions to reach that neighbor. */
      uint8_t best_neighbor_etx = neighbor_etx(n);
      uint16_t best_neighbor_rtmetric = n->rtmetric;
      uint16_t old_rtmetric = tc->rtmetric;
      uint16_t new_rtmetric = best_neighbor_rtmetric + (uint16_t) best_neighbor_etx;
      if (new_rtmetric != old_rtmetric) {
	PRINTF("%d.%d: rel_update_rtmetric: updating rtmetric (old_rtmetric = %u, new_rtmetric = %u, best_neighbor_rtmetric = %u, best_neighbor_etx = %u)\n",
	       rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	       old_rtmetric,
	       new_rtmetric,
	       best_neighbor_rtmetric,
	       best_neighbor_etx);
	
	tc->rtmetric = new_rtmetric;
	
#if COLLECT_ANNOUNCEMENTS
	announcement_set_value(&tc->announcement, tc->rtmetric);
#endif /* COLLECT_ANNOUNCEMENTS */
	
	/* We got a new, working, route we send any queued packets we may have. */
	if (old_rtmetric == RTMETRIC_MAX) {
	  rel_send_queued_packet();
	}
      }
    }
  }
}
#endif /* !STATIC */
/*---------------------------------------------------------------------------*/
static void rel_node_packet_received(struct relunicast_conn *c, rimeaddr_t *from, uint8_t seqno) {

  struct relcollect_conn *tc = (struct relcollect_conn *) ((char *) 
							   c - offsetof(struct relcollect_conn, relunicast_conn));
  int i;
  
  /* To protect against forwarding duplicate packets, we keep a list
     of recently forwarded packet seqnos. If the seqno of the current
     packet exists in the list, we drop the packet. */
  for (i = 0; i < NUM_RECENT_PACKETS; i++) {
    if (  recent_packets[i].seqno == packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID)
	  && rimeaddr_cmp(&recent_packets[i].originator, packetbuf_addr(PACKETBUF_ADDR_ESENDER))) {
      PRINTF("%d.%d: rel_node_packet_received: dropping duplicate packet from %d.%d with seqno %d\n",
	     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	     recent_packets[i].originator.u8[0], recent_packets[i].originator.u8[1],
	     packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID));
      /* Drop the packet. */
      return;
    }
  }
  
  recent_packets[recent_packet_ptr].seqno = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
  rimeaddr_copy(&recent_packets[recent_packet_ptr].originator,packetbuf_addr(PACKETBUF_ADDR_ESENDER));
  recent_packet_ptr = (recent_packet_ptr + 1) % NUM_RECENT_PACKETS;
  
#if !STATIC
  /* If we receive a data packet containing an ETX value that is not higher
     than our own ETX value, we announce our own ETX value more often. */
  if (! (packetbuf_attr(PACKETBUF_ATTR_RTMETRIC) > tc->rtmetric)) {
#if !STATIC
	  rel_update_rtmetric(tc);
#endif /* STATIC */
  }
#endif

  if (tc->rtmetric == SINK_METRIC) {
    /* If we are the sink, we call the receive function. */
    PRINTF("%d.%d: rel_node_packet_received: sink received packet with seqno %u from %d.%d via %d.%d\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	   packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID),
	   packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[0],
	   packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[1],
	   from->u8[0], from->u8[1]);
    if (tc->cb->recv != NULL) {
      tc->cb->recv(packetbuf_addr(PACKETBUF_ADDR_ESENDER),
#if QUEUING_STATS
		   packetbuf_attr(PACKETBUF_ATTR_QUEUING_DELAY),
		   packetbuf_attr(PACKETBUF_ATTR_DROPPED_PACKETS_COUNT),
		   packetbuf_attr(PACKETBUF_ATTR_QUEUE_SIZE),
#endif /* QUEUING_STATS */
#if WITH_FTSP || GLOSSY
		   packetbuf_attr(PACKETBUF_ATTR_TIME_HIGH),
		   packetbuf_attr(PACKETBUF_ATTR_TIME_LOW));
#else
		   );
#endif /* WITH_FTSP */
    }
    return;
  } else {
#if !STATIC
    if(tc->rtmetric != RTMETRIC_MAX) {
#endif /* !STATIC */
      /* If we are not the sink, we forward the packet to the best neighbor. */
      PRINTF("%d.%d: rel_node_packet_received: packet received from %d.%d via %d.%d, forwarding %d\n",
	     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	     packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[0],
	     packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[1],
	     from->u8[0], from->u8[1], tc->forwarding);
      
      if (packetqueue_enqueue_packetbuf(&forwarding_queue, FORWARD_PACKET_LIFETIME, tc)) {
    	  rel_send_queued_packet();
      } else {
    	  dropped_packets_count++;
      }
#if !STATIC
    } else {
    	PRINTF("%d.%d: rel_node_packet_received: dropping received packet from %d.%d via %d.%d (rtmetric = %u)\n",
	     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	     packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[0],
	     packetbuf_addr(PACKETBUF_ADDR_ESENDER)->u8[1],
	     from->u8[0], from->u8[1],
	     tc->rtmetric);
    }
#endif /* !STATIC */
  }
}
/*---------------------------------------------------------------------------*/
static void rel_node_packet_sent(struct relunicast_conn *c, rimeaddr_t *to,	uint8_t retransmissions) {
  
  struct relcollect_conn *tc = (struct relcollect_conn *) ((char *) 
							   c - offsetof(struct relcollect_conn, relunicast_conn));
  
  PRINTF("%d.%d: rel_node_packet_sent: sent packet %u to %d.%d after %d retransmissions\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	 c->sndnxt,
	 to->u8[0], to->u8[1],
	 retransmissions);
  
  tc->forwarding = 0;
#if !STATIC
  neighbor_update_etx(neighbor_find(to), retransmissions + 1);
  rel_update_rtmetric(tc);
#endif /* !STATIC */
  
  /* Remove the first packet on the queue, the packet that was just sent. */
  packetqueue_dequeue(&forwarding_queue);
  
  /* Send the next packet in the queue, if any. */
  rel_send_queued_packet();
}
/*---------------------------------------------------------------------------*/
static void rel_node_packet_timedout(struct relunicast_conn *c, rimeaddr_t *to,	uint8_t retransmissions) {

  struct relcollect_conn *tc = (struct relcollect_conn *) ((char *) 
							   c	- offsetof(struct relcollect_conn, relunicast_conn));
  
  PRINTF("%d.%d: rel_node_packet_timedout: timedout after %d retransmissions\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1], retransmissions);
  
  tc->forwarding = 0;
#if !STATIC
  neighbor_timedout_etx(neighbor_find(to), retransmissions + 1);
  rel_update_rtmetric(tc);
#endif /* !STATIC */
  
  /* Remove the first packet on the queue, the packet that just timed out. */
  packetqueue_dequeue(&forwarding_queue);
  
  /* Send the next packet in the queue, if any. */
  rel_send_queued_packet();
}
/*---------------------------------------------------------------------------*/
#if COLLECT_ANNOUNCEMENTS
static void received_announcement(struct announcement *a, rimeaddr_t *from,	uint16_t id, uint16_t value) {
#if !STATIC
  //struct collect_conn *tc = (struct collect_conn *) ((char *) a - offsetof(struct collect_conn, announcement));
  struct relcollect_conn *tc = (struct relcollect_conn *) ((char *) 
							   a	- offsetof(struct relcollect_conn, announcement));
  struct neighbor *n;
  
  n = neighbor_find(from);
  if (n == NULL) {
    neighbor_add(from, value, 1);
    PRINTF("%d.%d: received_announcement: new neighbor %d.%d, etx %d\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	   from->u8[0], from->u8[1], value);
  } else {
    neighbor_update(n, value);
    PRINTF("%d.%d: received_announcement: updating neighbor %d.%d, etx %d\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	   n->addr.u8[0], n->addr.u8[1], value);
  }
  rel_update_rtmetric(tc);
#endif /* STATIC */
}
#endif /* !COLLECT_ANNOUNCEMENTS */
/*---------------------------------------------------------------------------*/
static const struct relunicast_callbacks relunicast_callbacks = { rel_node_packet_received,
								  rel_node_packet_sent,
								  rel_node_packet_timedout };
/*---------------------------------------------------------------------------*/
void relcollect_open(struct relcollect_conn *tc, uint16_t channels, const struct relcollect_callbacks *cb) {
  
  relunicast_open(&tc->relunicast_conn, channels + 1, &relunicast_callbacks);
  channel_set_attributes(channels + 1, attributes);
  tc->seqno = 1;
  tc->rtmetric = RTMETRIC_MAX;
  tc->cb = cb;
#if COLLECT_ANNOUNCEMENTS
  announcement_register(&tc->announcement, channels, tc->rtmetric, received_announcement);
  announcement_listen(2);
#endif /* COLLECT_ANNOUNCEMENTS */
#if STATIC
  rimeaddr_t n;
#if LOCATION == ETH
    if (rimeaddr_node_addr.u8[0] == 200) {
    	relcollect_set_sink(tc, 1);
    } else if (rimeaddr_node_addr.u8[0] == 1) {
    	n.u8[0] = 17; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 2) {
    	n.u8[0] = 12; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 3) {
    	n.u8[0] = 109; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 4) {
    	n.u8[0] = 23; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 5) {
    	n.u8[0] = 2; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 6) {
    	n.u8[0] = 2; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 7) {
    	n.u8[0] = 14; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 8) {
    	n.u8[0] = 23; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 9) {
    	n.u8[0] = 10; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 10) {
    	n.u8[0] = 29; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 11) {
    	n.u8[0] = 26; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 12) {
    	n.u8[0] = 29; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 13) {
    	n.u8[0] = 33; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 14) {
    	n.u8[0] = 4; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 15) {
    	n.u8[0] = 33; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 16) {
    	n.u8[0] = 26; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 17) {
    	n.u8[0] = 3; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 18) {
    	n.u8[0] = 6; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 19) {
    	n.u8[0] = 23; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 20) {
    	n.u8[0] = 13; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 21) {
    	n.u8[0] = 5; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 22) {
    	n.u8[0] = 11; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 23) {
    	n.u8[0] = 12; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 24) {
    	n.u8[0] = 6; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 25) {
    	n.u8[0] = 18; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 26) {
    	n.u8[0] = 30; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 27) {
    	n.u8[0] = 28; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 28) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 29) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 30) {
    	n.u8[0] = 5; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 31) {
    	n.u8[0] = 23; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 32) {
    	n.u8[0] = 102; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 33) {
    	n.u8[0] = 105; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 34) {
    	n.u8[0] = 105; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 101) {
    	n.u8[0] = 7; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 102) {
    	n.u8[0] = 13; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 103) {
    	n.u8[0] = 33; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 104) {
    	n.u8[0] = 33; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 105) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 106) {
    	n.u8[0] = 29; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 107) {
    	n.u8[0] = 12; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 108) {
    	n.u8[0] = 102; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 109) {
    	n.u8[0] = 29; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    }
#elif LOCATION == SICS
    if (rimeaddr_node_addr.u8[0] == 200) {
    	relcollect_set_sink(tc, 1);
    } else if (rimeaddr_node_addr.u8[0] == 1) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 2) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 3) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 4) {
    	n.u8[0] = 22; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 5) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 6) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 7) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 8) {
    	n.u8[0] = 6; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 9) {
    	n.u8[0] = 13; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 10) {
    	n.u8[0] = 11; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 11) {
    	n.u8[0] = 7; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 12) {
    	n.u8[0] = 13; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 13) {
    	n.u8[0] = 7; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 14) {
    	n.u8[0] = 17; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 15) {
    	n.u8[0] = 7; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 16) {
    	n.u8[0] = 15; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 17) {
    	n.u8[0] = 13; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 18) {
    	n.u8[0] = 6; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 19) {
    	n.u8[0] = 20; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 20) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 21) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 22) {
    	n.u8[0] = 2; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    } else if (rimeaddr_node_addr.u8[0] == 23) {
    	n.u8[0] = 200; n.u8[1] = 0;
    	neighbor_add(&n, 1, 1);
    }
#endif /* LOCATION */
#endif /* STATIC */
}
/*---------------------------------------------------------------------------*/
void relcollect_close(struct relcollect_conn *tc) {
#if COLLECT_ANNOUNCEMENTS
  announcement_remove(&tc->announcement);
#endif /* COLLECT_ANNOUNCEMENTS */
  relunicast_close(&tc->relunicast_conn);
  ctimer_stop(&tc->t);
}
/*---------------------------------------------------------------------------*/
void relcollect_set_sink(struct relcollect_conn *tc, int should_be_sink) {
  if (should_be_sink) {
    tc->rtmetric = SINK_METRIC;
  } else {
    tc->rtmetric = RTMETRIC_MAX;
  }
#if COLLECT_ANNOUNCEMENTS
  announcement_set_value(&tc->announcement, tc->rtmetric);
#endif /* COLLECT_ANNOUNCEMENTS */
#if !STATIC
  rel_update_rtmetric(tc);
#endif /* STATIC */
}
/*---------------------------------------------------------------------------*/
int relcollect_send(struct relcollect_conn *tc, int rexmits) {
  struct neighbor *n;

  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, tc->seqno++);
  packetbuf_set_addr(PACKETBUF_ADDR_ESENDER, &rimeaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_MAX_REXMIT, rexmits);
#if QUEUING_STATS
  packetbuf_set_attr(PACKETBUF_ATTR_QUEUING_DELAY, 0); // will be rewritten in rel_send_queued_packet
  packetbuf_set_attr(PACKETBUF_ATTR_DROPPED_PACKETS_COUNT, dropped_packets_count);
  packetbuf_set_attr(PACKETBUF_ATTR_QUEUE_SIZE, 0); // will be rewritten in rel_send_queued_packet
#endif /* QUEUING_STATS */
  packetbuf_set_attr(PACKETBUF_ATTR_RTMETRIC, 0); // will be rewritten in rel_send_queued_packet

  if (tc->rtmetric == SINK_METRIC) {
    if (tc->cb->recv != NULL) {
      tc->cb->recv(packetbuf_addr(PACKETBUF_ADDR_ESENDER),
#if QUEUING_STATS
		   packetbuf_attr(PACKETBUF_ATTR_QUEUING_DELAY),
		   packetbuf_attr(PACKETBUF_ATTR_DROPPED_PACKETS_COUNT),
		   packetbuf_attr(PACKETBUF_ATTR_QUEUE_SIZE),
#endif /* QUEUING_STATS */
#if WITH_FTSP || GLOSSY
		   packetbuf_attr(PACKETBUF_ATTR_TIME_HIGH),
		   packetbuf_attr(PACKETBUF_ATTR_TIME_LOW));
#else
		   );
#endif /* WITH_FTSP */
    }
    return 1;
  } else {
	  n = neighbor_best();
	  if (n != NULL) {
		  PRINTF("%d.%d: relcollect_send: sending to %d.%d\n",
				  rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
				  n->addr.u8[0], n->addr.u8[1]);
		  if (packetqueue_enqueue_packetbuf(&forwarding_queue, FORWARD_PACKET_LIFETIME, tc)) {
			  rel_send_queued_packet();
			  return 1;
		  } else {
			  dropped_packets_count++;
		  }
	  } else {
		  PRINTF("%d.%d: relcollect_send: did not find any neighbor to send to\n",
				  rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
#if COLLECT_ANNOUNCEMENTS
		  announcement_listen(1);
#endif /* COLLECT_ANNOUNCEMENTS */
		  if (packetqueue_enqueue_packetbuf(&forwarding_queue, FORWARD_PACKET_LIFETIME, tc)) {
			  return 1;
		  } else {
			  dropped_packets_count++;
		  }
	  }
  }
  
  return 0;
}
/*---------------------------------------------------------------------------*/
int relcollect_depth(struct relcollect_conn *tc) {
  return tc->rtmetric;
}
/*---------------------------------------------------------------------------*/

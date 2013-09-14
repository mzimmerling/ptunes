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

#ifndef __RELCOLLECT_H__
#define __RELCOLLECT_H__

#include "net/rime/announcement.h"
#include "net/rime/relunicast.h"

#if QUEUING_STATS
#define RELCOLLECT_ATTRIBUTES  { PACKETBUF_ADDR_ESENDER,               PACKETBUF_ADDRSIZE }, \
                               { PACKETBUF_ATTR_EPACKET_ID,            PACKETBUF_ATTR_BIT * 8 }, \
                               { PACKETBUF_ATTR_RTMETRIC,              PACKETBUF_ATTR_BIT * 8 }, \
                               { PACKETBUF_ATTR_MAX_REXMIT,            PACKETBUF_ATTR_BIT * 5 }, \
                               { PACKETBUF_ATTR_QUEUING_DELAY,         PACKETBUF_ATTR_BYTE * 2 }, \
                               { PACKETBUF_ATTR_DROPPED_PACKETS_COUNT, PACKETBUF_ATTR_BYTE * 2 }, \
                               { PACKETBUF_ATTR_QUEUE_SIZE,			   PACKETBUF_ATTR_BIT * 8 }, \
                               RELUNICAST_ATTRIBUTES
#else
#define RELCOLLECT_ATTRIBUTES  { PACKETBUF_ADDR_ESENDER,               PACKETBUF_ADDRSIZE }, \
                               { PACKETBUF_ATTR_EPACKET_ID,            PACKETBUF_ATTR_BIT * 8 }, \
                               { PACKETBUF_ATTR_RTMETRIC,              PACKETBUF_ATTR_BIT * 8 }, \
                               { PACKETBUF_ATTR_MAX_REXMIT,            PACKETBUF_ATTR_BIT * 5 }, \
                               RELUNICAST_ATTRIBUTES
#endif /* QUEUING_STATS */

#define RELCOLLECT_MAX_DEPTH 255

#define STATIC 1

struct relcollect_callbacks {
  void (* recv)(const rimeaddr_t *originator,
#if (WITH_FTSP || GLOSSY) && QUEUING_STATS
		  uint16_t queuing_delay, uint16_t dropped_packets_count, uint8_t queue_size, rtimer_clock_t time_high, rtimer_clock_t time_low);
#elif (WITH_FTSP || GLOSSY) && !QUEUING_STATS
		  rtimer_clock_t time_high, rtimer_clock_t time_low);
#elif !(WITH_FTSP || GLOSSY) && QUEUING_STATS
		  uint16_t queuing_delay, uint16_t dropped_packets_count, uint8_t queue_size);
#else
		  );
#endif /* WITH_FTSP */
};

struct relcollect_conn {
  struct relunicast_conn relunicast_conn;
  struct announcement announcement;
  const struct relcollect_callbacks *cb;
  struct ctimer t;
  uint8_t parent_id;
  uint16_t rtmetric;
  uint8_t forwarding;
  uint8_t seqno;
};

void relcollect_open(struct relcollect_conn *c, uint16_t channels, 
		     const struct relcollect_callbacks *callbacks);

void relcollect_close(struct relcollect_conn *c);

int relcollect_send(struct relcollect_conn *c, int rexmits);

void relcollect_set_sink(struct relcollect_conn *c, int should_be_sink);

int relcollect_depth(struct relcollect_conn *c);

#endif /* __RELCOLLECT_H__ */

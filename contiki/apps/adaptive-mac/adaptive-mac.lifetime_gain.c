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
 * author: Marco Zimmerling <zimmerling@tik.ee.ethz.ch>
 *         Federico Ferrari <ferrari@tik.ee.ethz.ch>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "time.h"

#include "random.h"

#include "dev/leds.h"
#include "dev/watchdog.h"
#include "net/rime.h"
#include "net/rime/relcollect.h"
#include "net/rime/neighbor.h"
#if GLOSSY
#include "glossy_interface.h"
extern unsigned long glossy_arrival_time;
#endif /* GLOSSY */

#if MAC_PROTOCOL == XMAC
 #include "net/mac/xmac.h"
#elif MAC_PROTOCOL == LPP
 #include "net/mac/lpp.h"
#elif MAC_PROTOCOL == LPP_NEW
 #include "net/mac/lpp_new.h"
#endif /* MAC_PROTOCOL */
#include "dev/serial-line.h"

#if WITH_FTSP
#include "net/rime/ftsp.h"
extern rtimer_long_clock_t ftsp_arrival_time;
#endif /* WITH_FTSP */

#define DEBUG 0
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#if !GLOSSY
/* Representation of MAC configuration */
struct config {
  uint16_t t_l;
  uint16_t t_s;
  uint8_t n;
};
#endif /* !GLOSSY */

/* Representation of a data packet */
struct message {
  uint16_t seq_no;
  uint32_t energest_listen;
  uint32_t energest_transmit;
  uint32_t energest_lmp;
  uint32_t energest_cpu;
#if GLOSSY
  uint8_t rx_cnt;
  rtimer_clock_t time_to_rx;
  int period_skew;
  uint32_t energest_glossy_listen;
  uint32_t energest_glossy_transmit;
  uint32_t energest_glossy_cpu;
#endif /* GLOSSY */
};

/* Representation of ADAPTMAC print message */
struct print_message {
  uint8_t rx;
  uint16_t seq_no;
  uint32_t t_tx;
  uint32_t t_rx;
  uint32_t t_lpm;
  uint32_t t_cpu;
#if WITH_FTSP || GLOSSY
  unsigned long latency;
#endif /* WITH_FTSP */
#if QUEUING_STATS
  uint16_t queuing_delay;
  uint16_t dropped_packets_count;
  uint8_t queue_size;
#endif /* QUEUING_STATS */
#if GLOSSY
  uint8_t rx_cnt;
  rtimer_clock_t time_to_rx;
  int period_skew;
  uint32_t t_glossy_rx;
  uint32_t t_glossy_tx;
  uint32_t t_glossy_cpu;
#endif /* GLOSSY */
};

static volatile struct print_message print_msg;

/* Default data rate (in seconds). */
#define DEFAULT_DATA_RATE 30

#define CHANGE_DATA_RATE 1

#if CHANGE_DATA_RATE
#define CHANGE_DATA_RATE_INTERVAL 30	// in minutes
static int data_rates[] = {10, 10, 10, 20, 20, 30, 30, 60, 60, 180, 180, 300, 300, 300, 300, 1200, 1200, 1200, 1200};	// in seconds
#define NUM_DATA_RATES (sizeof(data_rates) / sizeof(int))
#endif /* CHANGE_DATA_RATE */

#if !GLOSSY
static int data_rate;
static volatile struct config new_cfg;
#if MAC_PROTOCOL == XMAC
static volatile struct config current_cfg = {
  XMAC_DEFAULT_ON_TIME,
  XMAC_DEFAULT_OFF_TIME,
  DEFAULT_MAX_NRTX
};
#elif MAC_PROTOCOL == LPP
static volatile struct config current_cfg = {
  // t_l corresponds to tx_timeout for LPP
  2*LPP_DEFAULT_ON_TIME + LPP_DEFAULT_OFF_TIME + LPP_RANDOM_OFF_TIME_ADJUSTMENT,
  LPP_DEFAULT_OFF_TIME,
  DEFAULT_MAX_NRTX
};
#elif MAC_PROTOCOL == LPP_NEW
static volatile struct config current_cfg = {
  2*LPP_NEW_DEFAULT_ON_TIME + LPP_NEW_DEFAULT_OFF_TIME + MAX_RANDOM_OFF_TIME,
  LPP_NEW_DEFAULT_OFF_TIME,
  DEFAULT_MAX_NRTX
};
#endif
#define SCALE 1000
#endif /* !GLOSSY */

/*---------------------------------------------------------------------------*/
PROCESS(app_process, "Application Process");
PROCESS(print_process, "Print Process");

#if TRICKLE_ON || GLOSSY
PROCESS(dissemination_process, "Dissemination Process");
PROCESS(adaptation_process, "Adaptation Process");
#if CHANGE_DATA_RATE
PROCESS(change_data_rate_process, "Change Data-Rate Process");
AUTOSTART_PROCESSES(&app_process, &print_process, &dissemination_process, &adaptation_process, &change_data_rate_process);
#else
AUTOSTART_PROCESSES(&app_process, &print_process, &dissemination_process, &adaptation_process);
#endif /* CHANGE_DATA_RATE */
#else /* TRICKLE_ON */
#if CHANGE_DATA_RATE
PROCESS(change_data_rate_process, "Change Data-Rate Process");
AUTOSTART_PROCESSES(&app_process, &print_process, &change_data_rate_process);
#else
AUTOSTART_PROCESSES(&app_process, &print_process);
#endif /* CHANGE_DATA_RATE */
#endif /* TRICKLE_ON || GLOSSY */
/*---------------------------------------------------------------------------*/
static void
#if (WITH_FTSP || GLOSSY) && QUEUING_STATS
recv_relcollect(const rimeaddr_t *originator, uint16_t queuing_delay, uint16_t dropped_packets_count, uint8_t queue_size, rtimer_clock_t time_high, rtimer_clock_t time_low)
#elif (WITH_FTSP || GLOSSY) && !QUEUING_STATS
recv_relcollect(const rimeaddr_t *originator, rtimer_clock_t time_high, rtimer_clock_t time_low)
#elif !(WITH_FTSP || GLOSSY) && QUEUING_STATS
recv_relcollect(const rimeaddr_t *originator, uint16_t queuing_delay, uint16_t dropped_packets_count, uint8_t queue_size)
#else
recv_relcollect(const rimeaddr_t *originator)
#endif /* (WITH_FTSP || GLOSSY) && QUEUING_STATS */
{
  static struct message msg;
  packetbuf_copyto(&msg);
  
#if WITH_FTSP || GLOSSY
  unsigned long time, high, low = 0;
  high = time_high;
  low = time_low;
  time = (high << 16) | low;
#endif /* WITH_FTSP */

  print_msg.rx = originator->u8[0];
  print_msg.seq_no = msg.seq_no;
  print_msg.t_tx = msg.energest_transmit;
  print_msg.t_rx = msg.energest_listen;
  print_msg.t_lpm = msg.energest_lmp;
  print_msg.t_cpu = msg.energest_cpu;
#if WITH_FTSP
  print_msg.latency = (rtimer_long_clock_t) (ftsp_arrival_time - time) * 1000 / RTIMER_SECOND_LONG;
#endif /* WITH_FTSP */
#if QUEUING_STATS
  print_msg.queuing_delay = queuing_delay;
  print_msg.dropped_packets_count = dropped_packets_count;
  print_msg.queue_size = queue_size;
#endif /* QUEUING_STATS */
#if GLOSSY
  print_msg.t_glossy_tx = msg.energest_glossy_transmit;
  print_msg.t_glossy_rx = msg.energest_glossy_listen;
  print_msg.t_glossy_cpu = msg.energest_glossy_cpu;
  if (glossy_arrival_time < time) {
	  // there was a Glossy phase between the first packet transmission and the final reception
	  glossy_arrival_time += GLOSSY_PERIOD;
  }
  print_msg.latency = (unsigned long) (glossy_arrival_time - time) * 1000 / RTIMER_SECOND;
  print_msg.rx_cnt = msg.rx_cnt;
  print_msg.time_to_rx = msg.time_to_rx;
  print_msg.period_skew = msg.period_skew;
#endif /* GLOSSY */
  /* Poll the print process */
  process_poll(&print_process);
}
/*---------------------------------------------------------------------------*/
static struct relcollect_callbacks relcollect_callbacks = { recv_relcollect };
static struct relcollect_conn c;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(app_process, ev, data)
{
  PROCESS_EXITHANDLER(relcollect_close(&c);)
    
  PROCESS_BEGIN();
  
  process_start(&time_process, NULL);
  
  relcollect_open(&c, 128, &relcollect_callbacks);

  static struct etimer et;

#if GLOSSY
  if(rimeaddr_node_addr.u8[0] == SINK_ID
     && rimeaddr_node_addr.u8[1] == 0) {
	    etimer_set(&et, CLOCK_SECOND * 2);
	    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  }
  glossy_init();
#endif

  if(rimeaddr_node_addr.u8[0] == SINK_ID
     && rimeaddr_node_addr.u8[1] == 0) {
    relcollect_set_sink(&c, 1);
    PROCESS_WAIT_EVENT_UNTIL(0);
  }
  
  static struct message msg;
  msg.seq_no = 0;
#if CHANGE_DATA_RATE
  data_rate = data_rates[0];
#else
  data_rate = DEFAULT_DATA_RATE;
#endif /* CHANGE_DATA_RATE */

  /*  Initial delay (8 minutes) */
  etimer_set(&et, CLOCK_SECOND * 240);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  etimer_set(&et, CLOCK_SECOND * 240 + (random_rand() % (CLOCK_SECOND * data_rate)));
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  // Reset Energest values
  energest_init();
#if GLOSSY
  energest_glossy_init();
  ENERGEST_ON(ENERGEST_TYPE_CPU);
#endif /* GLOSSY */
  
  static uint16_t minutes;

  while(1) {
	if (data_rate < 512) {
		etimer_set(&et, CLOCK_SECOND * data_rate);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
	} else {
		// wait for (data_rate / 60) complete minutes
		for (minutes = 0; minutes < data_rate / 60; minutes++) {
			etimer_set(&et, CLOCK_SECOND * 60);
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
		if (data_rate % 60) {
			// wait for the remaining (data_rate % 60) seconds
			etimer_set(&et, CLOCK_SECOND * (data_rate % 60));
			PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		}
	}
    
    /* Construct data packet payload */
    msg.seq_no++;
    msg.energest_lmp = energest_type_time(ENERGEST_TYPE_LPM);
    msg.energest_listen = energest_type_time(ENERGEST_TYPE_LISTEN);
    msg.energest_transmit = energest_type_time(ENERGEST_TYPE_TRANSMIT);
    msg.energest_cpu = energest_type_time(ENERGEST_TYPE_CPU);
#if GLOSSY
    msg.rx_cnt = config_received;
    msg.time_to_rx = config_received ? get_time_to_first_rx() : -1;
    msg.period_skew = period_skew;
    msg.energest_glossy_listen = energest_glossy_type_time(ENERGEST_TYPE_LISTEN);
    msg.energest_glossy_transmit = energest_glossy_type_time(ENERGEST_TYPE_TRANSMIT);
    msg.energest_glossy_cpu = energest_glossy_type_time(ENERGEST_TYPE_CPU);
#endif /* GLOSSY */

    packetbuf_clear();
    packetbuf_copyfrom(&msg, sizeof (struct message));
    relcollect_send(&c, current_cfg.n);
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(print_process, ev, data)
{
  PROCESS_BEGIN();

  while(1) {
    /* Wait for being polled */
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);

    /* Print ADAPTMAC message */
#if (WITH_FTSP || GLOSSY) && QUEUING_STATS
    printf("A o=%u seq=%u qd=%u dp=%u qs=%u tx=%lu rx=%lu lpm=%lu cpu=%lu lat=%lu rxc=%u t2rx=%u psk=%d gtx=%lu grx=%lu gcpu=%lu\n",
#elif (WITH_FTSP || GLOSSY) && !QUEUING_STATS
    printf("A o=%u seq=%u tx=%lu rx=%lu lpm=%lu cpu=%lu lat=%lu rxc=%u t2rx=%u psk=%d gtx=%lu grx=%lu gcpu=%lu\n",
#elif !(WITH_FTSP || GLOSSY) && QUEUING_STATS
    printf("A o=%u seq=%u qd=%u dp=%u qs=%u tx=%lu rx=%lu lpm=%lu cpu=%lu\n",
#else
    printf("A o=%u seq=%u tx=%lu rx=%lu lpm=%lu cpu=%lu\n",
#endif /* (WITH_FTSP || GLOSSY) && QUEUING_STATS */
	print_msg.rx,						// originator of the packet (i.e., node id)
	print_msg.seq_no,					// packet sequence number
#if QUEUING_STATS
	print_msg.queuing_delay,			// average queuing delay
	print_msg.dropped_packets_count,	// total number of dropped packets
	print_msg.queue_size,				// number of packets currently queued
#endif /*QUEUING_STATS */
	print_msg.t_tx,						// energest time in tx mode
	print_msg.t_rx,						// energest time in rx mode
    print_msg.t_lpm,					// energest time in lpm mode
    print_msg.t_cpu,					// energest time in cpu mode
#if WITH_FTSP || GLOSSY
    print_msg.latency,					// end-to-end latency of the packet
    print_msg.rx_cnt,					//
    print_msg.time_to_rx,				//
    print_msg.period_skew,				//
	print_msg.t_glossy_tx,				// Glossy energest time in tx mode
	print_msg.t_glossy_rx,				// Glossy energest time in rx mode
    print_msg.t_glossy_cpu);			// Glossy energest time in cpu mode
#else
    print_msg.t_cpu);
#endif /* WITH_FTSP */
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
#if TRICKLE_ON
static void trickle_recv(struct trickle_conn *c) {
  /* We received a new configuration via Trickle. */
  packetbuf_copyto((struct config *)&new_cfg);
  PRINTF("%d.%d: got new configuration via Trickle (t_l = %u t_s = %u n = %u)\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	 new_cfg.t_l, new_cfg.t_s, new_cfg.n);
  
  /* Notify adaptation process. */
  process_poll(&adaptation_process);
}
const static struct trickle_callbacks trickle_call = { trickle_recv };
static struct trickle_conn trickle;
/*---------------------------------------------------------------------------*/
#endif /* TRICKLE_ON */
#if CHANGE_DATA_RATE
PROCESS_THREAD(change_data_rate_process, ev, data) {

	static struct etimer et;
	static int data_rates_idx = 0;
	static int min_counter = 0;

	PROCESS_BEGIN();

	// add some initial randomness
	etimer_set(&et, random_rand() % (CLOCK_SECOND * 120));
	PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

	while (1)
	{
		etimer_set(&et, CLOCK_SECOND * 60);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		min_counter++;

		if (min_counter == CHANGE_DATA_RATE_INTERVAL)
		{
			if (data_rates_idx < NUM_DATA_RATES - 1)
				data_rates_idx++;
			data_rate = data_rates[data_rates_idx];
			PRINTF("new data rate: %d\n", data_rate);
			min_counter = 0;
		}
	}

	PROCESS_END();
}
#endif /* CHANGE_DATA_RATE */
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(dissemination_process, ev, data) {

#if TRICKLE_ON
  PROCESS_EXITHANDLER(trickle_close(&trickle);)
#endif /* TRICKLE_ON */
    
  PROCESS_BEGIN();

#if TRICKLE_ON
  trickle_open(&trickle, CLOCK_SECOND, 127, &trickle_call);
#endif /* TRICKLE_ON */
  
  /* All nodes resign from this process, except for the sink. */
  if( !(  rimeaddr_node_addr.u8[0] == SINK_ID
	  && rimeaddr_node_addr.u8[1] == 0)) {
    PROCESS_WAIT_EVENT_UNTIL(0);
  }
  
  while(1) {
    /* Wait for configuration update over the UART. */
    PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message && data != NULL);

    PRINTF("%u.%u: got data over UART\n",
    		rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);

    /* Parse the configuration update. We assume the data is
       formatted like this: t_l,t_s,n, (Note the FINAL comma!!!) */
    char * s1 = (char *)data;
    char * s2 = (char *)data;
    uint8_t count;
    uint8_t commas_found = 0;
    unsigned long t_l_tmp = 0;
    unsigned long t_s_tmp = 0;
    while (commas_found < 3) {
      char buf[10] = "";
      count = 0;
      while (*s2 != ',') {
	s2++;
	count++;
      }
      commas_found++;
      strncpy(buf,s1,count);
      if (commas_found == 1) {
    t_l_tmp = (unsigned long)RTIMER_SECOND * 10 * atoi(buf) / SCALE;
      } else if (commas_found == 2) {
    t_s_tmp = (unsigned long)RTIMER_SECOND * 10 * atoi(buf) / SCALE;
      } else if (commas_found == 3) {
	new_cfg.n = (uint8_t) atoi(buf);
      }
      s2++;
      s1 = s2;
    }
    new_cfg.t_l = (rtimer_clock_t)(t_l_tmp / 10);
    new_cfg.t_s = (rtimer_clock_t)(t_s_tmp / 10);
    if (t_l_tmp % 10 > 4) {
    	new_cfg.t_l++;
    }
    if (t_s_tmp % 10 > 4) {
    	new_cfg.t_s++;
    }
    PRINTF("%u.%u: got new configuration over UART (t_l = %u t_s = %u n = %u)\n",
	   rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	   new_cfg.t_l, new_cfg.t_s, new_cfg.n);

#if TRICKLE_ON
    /* Disseminate the new configuration. */
    packetbuf_clear();
    packetbuf_copyfrom((struct config *)&new_cfg, sizeof (struct config));
    trickle_send(&trickle);
#endif /* TRICKLE_ON */
#if GLOSSY
    while (GLOSSY_IS_ON()) {
    	PROCESS_PAUSE();
    }
    /* Update Glossy data with new configuration. */
    glossy_config_data.seq_no++;
    glossy_config_data.t_l = new_cfg.t_l;
    glossy_config_data.t_s = new_cfg.t_s;
    glossy_config_data.n = new_cfg.n;
#endif
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(adaptation_process, ev, data) {

	PROCESS_BEGIN();

	/* Set initial configuration. */
#if MAC_PROTOCOL == XMAC
	static struct xmac_config new_xmac_config;
	new_xmac_config.on_time = (rtimer_clock_t) current_cfg.t_l;
	new_xmac_config.off_time = (rtimer_clock_t) current_cfg.t_s;
	new_xmac_config.strobe_time = (rtimer_clock_t) (2*current_cfg.t_l + current_cfg.t_s);
	new_xmac_config.strobe_wait_time = get_xmac_config()->strobe_wait_time;
	set_xmac_config(&new_xmac_config);
#elif MAC_PROTOCOL == LPP
	static struct lpp_config new_lpp_config;
	new_lpp_config.tx_timeout = (clock_time_t) current_cfg.t_l;
	new_lpp_config.off_time = (clock_time_t) current_cfg.t_s;
	new_lpp_config.on_time = (clock_time_t) get_lpp_config()->on_time;
	set_lpp_config(&new_lpp_config);
#elif MAC_PROTOCOL == LPP_NEW
	static struct lpp_new_config new_lpp_new_config;
	new_lpp_new_config.tx_timeout = (rtimer_clock_t) current_cfg.t_l;
	new_lpp_new_config.off_time = (rtimer_clock_t) current_cfg.t_s;
	new_lpp_new_config.on_time = (rtimer_clock_t) get_lpp_new_config()->on_time;
	set_lpp_new_config(&new_lpp_new_config);
#endif /* MAC_PROTOCOL */
	PRINTF("%u.%u: set initial configuration (t_l = %u t_s = %u n = %u)\n",
			rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
			current_cfg.t_l, current_cfg.t_s, current_cfg.n);
  
	while(1) {
		/* Wait for configuration update. */
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
		PRINTF("%u.%u: will adapt to new configuration (t_l = %u t_s = %u n = %u)\n",
				rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
				new_cfg.t_l,new_cfg.t_s, new_cfg.n);

#if TRICKLE_ON
		/* Wait until we haven't heard an outdated configuration
		   for 2 consecutive Trickle periods. */
		while (trickle.interval_scaling < 3) {
			PROCESS_PAUSE();
		}
#endif /* TRICKLE_ON */

		/* Adapt to new configuration. */
#if MAC_PROTOCOL == XMAC
		new_xmac_config.on_time = (rtimer_clock_t) new_cfg.t_l;
		new_xmac_config.off_time = (rtimer_clock_t) new_cfg.t_s;
		new_xmac_config.strobe_time = (rtimer_clock_t) (2*new_cfg.t_l + new_cfg.t_s);
		new_xmac_config.strobe_wait_time = get_xmac_config()->strobe_wait_time;
		set_xmac_config(&new_xmac_config);
		current_cfg.t_l = new_cfg.t_l;
		current_cfg.t_s = new_cfg.t_s;
		current_cfg.n = new_cfg.n;
#elif MAC_PROTOCOL == LPP
		new_lpp_config.tx_timeout = (clock_time_t) new_cfg.t_l;
		new_lpp_config.off_time = (clock_time_t) new_cfg.t_s;
		new_lpp_config.on_time = (clock_time_t) get_lpp_config()->on_time;
		set_lpp_config(&new_lpp_config);
		current_cfg.t_l = new_cfg.t_l;
		current_cfg.t_s = new_cfg.t_s;
		current_cfg.n = new_cfg.n;
#elif MAC_PROTOCOL == LPP_NEW
		new_lpp_new_config.tx_timeout = (rtimer_clock_t) new_cfg.t_l;
		new_lpp_new_config.off_time = (rtimer_clock_t) new_cfg.t_s;
		new_lpp_new_config.on_time = (rtimer_clock_t) get_lpp_new_config()->on_time;
		set_lpp_new_config(&new_lpp_new_config);
		current_cfg.t_l = new_cfg.t_l;
		current_cfg.t_s = new_cfg.t_s;
		current_cfg.n = new_cfg.n;
#endif /* MAC_PROTOCOL */
		PRINTF("%u.%u: adapted to new configuration (t_l = %u t_s = %u n = %u)\n",
				rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
				current_cfg.t_l, current_cfg.t_s, current_cfg.n);
	}
  
	PROCESS_END();
}

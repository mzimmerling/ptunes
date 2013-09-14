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

#ifndef GLOSSY_INTERFACE_H_
#define GLOSSY_INTERFACE_H_

#include "glossy.h"
#include "net/rime.h"
#include "contiki-conf.h"

// Glossy data struct
struct config {
  uint16_t t_l;
  uint16_t t_s;
  uint8_t n;
};
typedef struct {
	uint16_t t_l;
	uint16_t t_s;
	uint8_t n;
	uint8_t seq_no;
} glossy_config_struct;
typedef struct {
	uint16_t pkt_rate;
	uint16_t prr;
	uint8_t  node_id;
	uint8_t  parent_id;
} glossy_report_struct;
typedef struct {
	glossy_report_struct glossy_report;
//	uint8_t node_id;
	uint8_t received;
} report_struct;

#if LOCATION == ETHZ
static uint8_t schedule[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 101, 102, 103, 104, 105, 106, 107, 108, 109};
#elif LOCATION == SICS
static uint8_t schedule[] = {1, 5, 8, 13, 2, 6, 9, 14, 3, 7, 10, 15, 4, 20, 11, 16, 22, 21, 12, 17, 23, 19, 18};
#endif /* LOCATION */
#define N_NODES sizeof(schedule)
static uint8_t leds[] = {1, 3, 7, 6, 4, 6, 7, 3, 1};

// Interface variables
static glossy_config_struct glossy_config_data;
static glossy_report_struct glossy_report_data;
static volatile struct config new_cfg;
static uint16_t data_rate;
int period_skew = 0;
// Will be changed by relcollect
uint8_t parent_id = 0;
static report_struct report[N_NODES];

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
  LPP_NEW_DEFAULT_LISTEN_TIME,
  LPP_NEW_DEFAULT_OFF_TIME,
  DEFAULT_MAX_NRTX
};
#endif
#define SCALE                     1000
#define IS_SINK()                 (rimeaddr_node_addr.u8[0] == SINK_ID && rimeaddr_node_addr.u8[1] == 0)
#define GLOSSY_IS_BOOTSTRAPPING() (skew_estimated < GLOSSY_BOOTSTRAP_PERIODS)

// Reference to the adaptation process
PROCESS_NAME(adaptation_process);

// Local variables
static struct rtimer rt;
static struct pt pt;
static volatile uint8_t config_seq_no = 0;
static rtimer_clock_t t_ref_l_old = 0;
static uint8_t skew_estimated = 0;
static uint8_t config_received = 0;
static uint8_t sync_missed = 0;
static uint8_t report_idx = 0;
static rtimer_clock_t t_start = 0;

PROCESS(glossy_print_report_process, "Glossy print report process");
PROCESS_THREAD(glossy_print_report_process, ev, data)
{
	PROCESS_BEGIN();

	while(1) {
		PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
		for (report_idx = 0; report_idx < N_NODES; report_idx++) {
			// print only if the node has already a parent and it is actually the node we were waiting for
			if (report[report_idx].glossy_report.parent_id &&
					report[report_idx].glossy_report.node_id == schedule[report_idx]) {
				PROCESS_PAUSE();
				if (report[report_idx].received) {
				    unsigned long t_l_to_print = (unsigned long)current_cfg.t_l * SCALE * 10 / RTIMER_SECOND;
				    unsigned long t_s_to_print = (unsigned long)current_cfg.t_s * SCALE * 10 / RTIMER_SECOND;
				    if (t_l_to_print % 10 > 4) {
				    	t_l_to_print = t_l_to_print / 10 + 1;
				    } else {
				    	t_l_to_print = t_l_to_print / 10;
				    }
				    if (t_s_to_print % 10 > 4) {
				    	t_s_to_print = t_s_to_print / 10 + 1;
				    } else {
				    	t_s_to_print = t_s_to_print / 10;
				    }
					printf("G o=%u p=%u pr=%u prr=%u tl=%u ts=%u n=%u\n",
							report[report_idx].glossy_report.node_id,
							report[report_idx].glossy_report.parent_id,
							report[report_idx].glossy_report.pkt_rate,
							report[report_idx].glossy_report.prr,
							(rtimer_clock_t)t_l_to_print, (rtimer_clock_t)t_s_to_print, current_cfg.n);
				} else {
					printf("node_id=%u rx_cnt=0\n", report[report_idx].glossy_report.node_id);
				}
			}
		}
		printf("F\n");
		if (MAC_PROTOCOL == LPP_NEW) {
			rime_mac->on();
		}
	}

	PROCESS_END();
}


PROCESS(glossy_print_process, "Glossy print process");
PROCESS_THREAD(glossy_print_process, ev, data)
{
	PROCESS_BEGIN();

	while(1) {
		PROCESS_YIELD_UNTIL(ev == PROCESS_EVENT_POLL);
		if (!GLOSSY_IS_BOOTSTRAPPING()) {
			if (config_received) {
				printf("Glossy received %u time(s): time_to_rx %u, seq_no %u, config %u %u %u\n",
						config_received, get_time_to_first_rx(), glossy_config_data.seq_no,
						glossy_config_data.t_l, glossy_config_data.t_s, glossy_config_data.n);
			} else {
				printf("Glossy NOT received: seq_no %u\n", glossy_config_data.seq_no);
			}
			printf("period_skew = %d\n", period_skew);
		}
	}

	PROCESS_END();
}

static inline void stop_mac_energest(void) {
	if (energest_current_mode[ENERGEST_TYPE_LISTEN]) {
		ENERGEST_OFF(ENERGEST_TYPE_LISTEN);
	}
	if (energest_current_mode[ENERGEST_TYPE_TRANSMIT]) {
		ENERGEST_OFF(ENERGEST_TYPE_TRANSMIT);
	}
	if (energest_current_mode[ENERGEST_TYPE_CPU]) {
		ENERGEST_OFF(ENERGEST_TYPE_CPU);
	}
	if (energest_current_mode[ENERGEST_TYPE_LPM]) {
		ENERGEST_OFF(ENERGEST_TYPE_LPM);
	}
	ENERGEST_GLOSSY_ON(ENERGEST_TYPE_CPU);
}

static inline void start_mac_energest(void) {
	ENERGEST_GLOSSY_OFF(ENERGEST_TYPE_CPU);
	ENERGEST_ON(ENERGEST_TYPE_CPU);
}

static inline void estimate_period_skew(void) {
	if (GLOSSY_IS_SYNCED()) {
		period_skew = get_t_ref_l() - (t_ref_l_old + (rtimer_clock_t)GLOSSY_PERIOD);
		t_ref_l_old = get_t_ref_l();
		if (GLOSSY_IS_BOOTSTRAPPING()) {
			skew_estimated++;
			if (!GLOSSY_IS_BOOTSTRAPPING()) {
				// Glossy bootstrap is over
				leds_off(LEDS_RED);
			}
		}
	}
}

char glossy_scheduler(struct rtimer *t, void *ptr) {
	PT_BEGIN(&pt);

	if (IS_SINK()) {
		// sink
		while (1) {
			rime_mac->off(0);
			stop_mac_energest();

			leds_on(LEDS_GREEN);
			glossy_disable_other_interrupts();
			// the sink is always the initiator during the config propagation phase
			glossy_start((uint8_t *)&glossy_config_data, GLOSSY_CONFIG_LEN,
					GLOSSY_INITIATOR, GLOSSY_SYNC, GLOSSY_N, 1);
			t_start = RTIMER_TIME(t);
			rtimer_set(t, t_start + GLOSSY_DURATION, 1, (rtimer_callback_t)glossy_scheduler, ptr);
			PT_YIELD(&pt);

			leds_off(LEDS_GREEN);
			config_received = glossy_stop();
			if (!GLOSSY_IS_BOOTSTRAPPING()) {
				// Glossy has successfully bootstrapped
				if (!GLOSSY_IS_SYNCED()) {
					// the reference time was not updated: increment reference time by GLOSSY_PERIOD
					set_t_ref_l(GLOSSY_REFERENCE_TIME + GLOSSY_PERIOD);
					set_t_ref_l_updated(1);
				}
				// start the round-robin reporting phase
				for (report_idx = 0; report_idx < N_NODES; report_idx++) {
					leds_off(LEDS_ALL);
					rtimer_set(t, GLOSSY_REFERENCE_TIME +
							(report_idx + 1) * (GLOSSY_DURATION + GLOSSY_GAP), 1,
							(rtimer_callback_t)glossy_scheduler, ptr);
					PT_YIELD(&pt);
					leds_on(leds[(report_idx + 1) % sizeof(leds)]);
					// the sink is always a receiver during the round-robin reporting phase
					glossy_start((uint8_t *)&report[report_idx].glossy_report, GLOSSY_REPORT_LEN,
							GLOSSY_RECEIVER, GLOSSY_NO_SYNC, GLOSSY_N, 1);
					rtimer_set(t, GLOSSY_REFERENCE_TIME +
							(report_idx + 1) * (GLOSSY_DURATION + GLOSSY_GAP) + GLOSSY_DURATION, 1,
							(rtimer_callback_t)glossy_scheduler, ptr);
					PT_YIELD(&pt);
					report[report_idx].received = glossy_stop();
				}
				leds_off(LEDS_ALL);
			}
			rtimer_set_long(t, t_start, GLOSSY_PERIOD, 1, (rtimer_callback_t)glossy_scheduler, ptr);
			estimate_period_skew();
			if (glossy_config_data.seq_no != config_seq_no) {
				// New configuration received by Glossy
				config_seq_no = glossy_config_data.seq_no;
				new_cfg.t_l = glossy_config_data.t_l;
				new_cfg.t_s = glossy_config_data.t_s;
				new_cfg.n = glossy_config_data.n;
				process_poll(&adaptation_process);
			}
			process_poll(&glossy_print_report_process);
			glossy_enable_other_interrupts();
			start_mac_energest();
			if (MAC_PROTOCOL != LPP_NEW) {
				rime_mac->on();
			}

			PT_YIELD(&pt);
		}
	} else {
		// not sink
		while (1) {
			rime_mac->off(0);
			stop_mac_energest();

			leds_on(LEDS_GREEN);
			glossy_disable_other_interrupts();
			glossy_start((uint8_t *)&glossy_config_data, GLOSSY_CONFIG_LEN,
					GLOSSY_RECEIVER, GLOSSY_SYNC, GLOSSY_N, 1);
			if (GLOSSY_IS_BOOTSTRAPPING()) {
				// Glossy is still bootstrapping
				rtimer_set(t, RTIMER_TIME(t) + GLOSSY_INIT_DURATION, 1,
					(rtimer_callback_t)glossy_scheduler, ptr);
			} else {
				// Glossy has successfully bootstrapped
				rtimer_set(t, RTIMER_TIME(t) + GLOSSY_GUARD_TIME * (1 + sync_missed) + GLOSSY_DURATION, 1,
					(rtimer_callback_t)glossy_scheduler, ptr);
			}
			PT_YIELD(&pt);

			leds_off(LEDS_GREEN);
			config_received = glossy_stop();
			if (GLOSSY_IS_BOOTSTRAPPING()) {
				// Glossy is still bootstrapping
				if (!GLOSSY_IS_SYNCED()) {
					// the reference time was not updated: reset skew_estimated
					skew_estimated = 0;
				}
			} else {
				// Glossy has successfully bootstrapped
				if (!GLOSSY_IS_SYNCED()) {
					// the reference time was not updated: increment reference time by GLOSSY_PERIOD
					set_t_ref_l(GLOSSY_REFERENCE_TIME + GLOSSY_PERIOD + period_skew);
					set_t_ref_l_updated(1);
					sync_missed++;
				} else {
					sync_missed = 0;
				}
				// start the round-robin reporting phase
				for (report_idx = 0; report_idx < N_NODES; report_idx++) {
					leds_off(LEDS_ALL);
					rtimer_set(t, GLOSSY_REFERENCE_TIME +
							(report_idx + 1) * (GLOSSY_DURATION + GLOSSY_GAP), 1,
							(rtimer_callback_t)glossy_scheduler, ptr);
					PT_YIELD(&pt);
					leds_on(leds[(report_idx + 1) % sizeof(leds)]);
					if (rimeaddr_node_addr.u8[0] == schedule[report_idx]) {
						// initiator for the current round: set the report fields
						glossy_report_data.node_id = rimeaddr_node_addr.u8[0];
						glossy_report_data.parent_id = parent_id;
						glossy_report_data.pkt_rate = data_rate;
						glossy_report_data.prr = MAC_GET_PRR();
						glossy_start((uint8_t *)&glossy_report_data, GLOSSY_REPORT_LEN,
								GLOSSY_INITIATOR, GLOSSY_NO_SYNC, GLOSSY_N, 1);
					} else {
						// receiver for the current round
						glossy_start((uint8_t *)&glossy_report_data, GLOSSY_REPORT_LEN,
								GLOSSY_RECEIVER, GLOSSY_NO_SYNC, GLOSSY_N, 1);
					}
					rtimer_set(t, GLOSSY_REFERENCE_TIME +
							(report_idx + 1) * (GLOSSY_DURATION + GLOSSY_GAP) + GLOSSY_DURATION, 1,
							(rtimer_callback_t)glossy_scheduler, ptr);
					PT_YIELD(&pt);
					glossy_stop();
				}
				leds_off(LEDS_ALL);
			}
			estimate_period_skew();
			if (GLOSSY_IS_BOOTSTRAPPING()) {
				if (skew_estimated == 0) {
					rtimer_set(t, RTIMER_TIME(t) + GLOSSY_INIT_PERIOD - GLOSSY_INIT_DURATION, 1,
							(rtimer_callback_t)glossy_scheduler, ptr);
				} else {
					rtimer_set_long(t, GLOSSY_REFERENCE_TIME,
							GLOSSY_PERIOD - GLOSSY_INIT_GUARD_TIME, 1,
							(rtimer_callback_t)glossy_scheduler, ptr);
				}
			} else {
				rtimer_set_long(t, GLOSSY_REFERENCE_TIME,
						GLOSSY_PERIOD + period_skew - GLOSSY_GUARD_TIME * (1 + sync_missed), 1,
						(rtimer_callback_t)glossy_scheduler, ptr);
			}
			if (glossy_config_data.seq_no != config_seq_no) {
				// New configuration received by Glossy
				config_seq_no = glossy_config_data.seq_no;
				new_cfg.t_l = glossy_config_data.t_l;
				new_cfg.t_s = glossy_config_data.t_s;
				new_cfg.n = glossy_config_data.n;
				process_poll(&adaptation_process);
			}
//			process_poll(&glossy_print_process);
			glossy_enable_other_interrupts();
			start_mac_energest();
			rime_mac->on();

			PT_YIELD(&pt);
		}
	}

	PT_END(&pt);
}

void glossy_init(void) {
	leds_on(LEDS_RED);
	skew_estimated = 0;
	// initialize Glossy config data
	glossy_config_data.seq_no = 0;
	glossy_config_data.t_l = current_cfg.t_l;
	glossy_config_data.t_s = current_cfg.t_s;
	glossy_config_data.n = current_cfg.n;
	// start print processes
	if (IS_SINK()) {
		process_start(&glossy_print_report_process, NULL);
	} else {
		process_start(&glossy_print_process, NULL);
	}
	process_start(&glossy_process, NULL);
	// start Glossy experiment
	rtimer_set(&rt, RTIMER_NOW() + RTIMER_SECOND, 1, (rtimer_callback_t)glossy_scheduler, NULL);
}

#endif /* GLOSSY_INTERFACE_H_ */

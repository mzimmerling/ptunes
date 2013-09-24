/*
 * Copyright (c) 2011, ETH Zurich.
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
 * Author: Federico Ferrari <ferrari@tik.ee.ethz.ch>
 *
 */

/**
 * \file
 *         Glossy core, header file from early 2011. A more recent version
 *         can be found at http://www.tik.ee.ethz.ch/~ferrarif/sw/glossy/index.html.
 * \author
 *         Federico Ferrari <ferrari@tik.ee.ethz.ch>
 */

#ifndef GLOSSY_H_
#define GLOSSY_H_

#include "contiki.h"
#include "dev/watchdog.h"
#include "dev/cc2420_const.h"
#include "dev/leds.h"
#include "dev/spi.h"
#include <stdio.h>
#include <legacymsp430.h>
#include <stdlib.h>

#if MAC_PROTOCOL == XMAC
#include "net/mac/xmac.h"
#elif MAC_PROTOCOL == LPP_NEW
#include "net/mac/lpp_new.h"
#endif /* MAC_PROTOCOL */

#define COMPENSATE_IRQ_DELAY          1
#if COOJA
#define CPU_RESYNC_DCO                0
#else
#define CPU_RESYNC_DCO                1
#endif
#define GLOSSY_SYNC_WINDOW            64

// ratio between the frequencies of the high- and low-frequency clocks
#if COOJA
#define CLOCK_PHI                     (4194304uL / RTIMER_SECOND)
#else
#define CLOCK_PHI                     (F_CPU / RTIMER_SECOND)
#endif

#define GLOSSY_HEADER                 0xfe
#define GLOSSY_HEADER_LEN             sizeof(uint8_t)
#define FOOTER_LEN                    2
#define FOOTER1_CRC_OK                0x80
#define FOOTER1_CORRELATION           0x7f

#define GLOSSY_LEN_FIELD              packet[0]
#define GLOSSY_HEADER_FIELD           packet[1]
#define GLOSSY_DATA_FIELD             packet[2]
#define GLOSSY_SLOT_FIELD             packet[packet_len - FOOTER_LEN]
#define GLOSSY_RSSI_FIELD             packet[packet_len - 1]
#define GLOSSY_CRC_FIELD              packet[packet_len]

typedef uint8_t slot_t;
#define GLOSSY_SLOT_LEN               sizeof(slot_t)
#define GLOSSY_IS_ON()                (get_glossy_status() != GLOSSY_STATUS_OFF)
#define GLOSSY_INITIATOR_TIMEOUT      (rtimer_clock_t)(F_CPU / 400)

enum {
	GLOSSY_INITIATOR = 1,
	GLOSSY_RECEIVER = 0
};

enum {
	GLOSSY_SYNC = 1,
	GLOSSY_NO_SYNC = 0
};

enum glossy_glossy_status {
	GLOSSY_STATUS_OFF,
	GLOSSY_STATUS_WAITING,
	GLOSSY_STATUS_RECEIVING,
	GLOSSY_STATUS_RECEIVED,
	GLOSSY_STATUS_TRANSMITTING,
	GLOSSY_STATUS_TRANSMITTED,
	GLOSSY_STATUS_ABORTED
};

PROCESS_NAME(glossy_process);

/* --------------------------- Main interface ----------------------- */
void glossy_start(uint8_t *data_, uint8_t data_len_, uint8_t initiator_,
		uint8_t sync_, uint8_t tx_max_, uint8_t turn_radio_on);
uint8_t glossy_stop(void);
inline void glossy_disable_other_interrupts(void);
inline void glossy_enable_other_interrupts(void);
slot_t get_relay_cnt(void);
rtimer_clock_t get_T_slot_h(void);
uint8_t is_t_ref_l_updated(void);
rtimer_clock_t get_t_ref_l(void);
void set_t_ref_l(rtimer_clock_t t);
void set_t_ref_l_updated(uint8_t updated);
uint8_t get_glossy_status(void);
rtimer_clock_t get_time_to_first_rx(void);

/* ------------------------------ Timeouts -------------------------- */
inline void glossy_schedule_rx_timeout(void);
inline void glossy_stop_rx_timeout(void);

/* ------------------------------- Radio ---------------------------- */
inline void radio_flush_tx(void);
inline uint8_t radio_glossy_status(void);
inline void radio_on(void);
inline void radio_off(void);
inline void radio_flush_rx(void);
inline void radio_abort_rx(void);
inline void radio_abort_tx(void);
inline void radio_start_tx(void);
inline void radio_write_tx(void);

/* ----------------------- Interrupt functions ---------------------- */
inline void glossy_begin_rx(void);
inline void glossy_end_rx(void);
inline void glossy_begin_tx(void);
inline void glossy_end_tx(void);

/* -------------------------- Clock Capture ------------------------- */
#define CAPTURE_NEXT_CLOCK_TICK(t_cap_h, t_cap_l) do {\
		/* Enable capture mode for timers B6 and A2 (ACLK) */\
		TBCCTL6 = CCIS0 | CM_POS | CAP | SCS; \
		TACCTL2 = CCIS0 | CM_POS | CAP | SCS; \
		/* Wait until both timers capture the next clock tick */\
		while (!((TBCCTL6 & CCIFG) && (TACCTL2 & CCIFG))); \
		/* Store the capture timer values */\
		t_cap_h = TBCCR6; \
		t_cap_l = TACCR2; \
		/* Disable capture mode */\
		TBCCTL6 = 0; \
		TACCTL2 = 0; \
} while (0)

/* -------------------------------- SFD ----------------------------- */
/* SFD capture for Timer B1 */
#define SFD_CAP_INIT(edge) do {\
	/* Need to select the special function! */\
	P4SEL |= BV(SFD);\
	/* Capture mode - capture on provided edges */\
	TBCCTL1 = edge | CAP | SCS;\
} while (0)

#define ENABLE_SFD_INT()		do { TBCCTL1 |= CCIE; } while (0)
#define DISABLE_SFD_INT()		do { TBCCTL1 &= ~CCIE; } while (0)
#define CLEAR_SFD_INT()			do { TBCCTL1 &= ~CCIFG; } while (0)
#define IS_ENABLED_SFD_INT()    !!(TBCCTL1 & CCIE)

#endif /* GLOSSY_H_ */

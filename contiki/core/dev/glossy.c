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
 *         Glossy core, source file from early 2011. A more recent version
 *         can be found at http://www.tik.ee.ethz.ch/~ferrarif/sw/glossy/index.html.
 * \author
 *         Federico Ferrari <ferrari@tik.ee.ethz.ch>
 */

#include "glossy.h"

static uint8_t initiator, sync, rx_cnt, tx_cnt, tx_max;
static uint8_t *data, *packet;
static uint8_t data_len, packet_len;
static uint8_t bytes_read, tx_slot_last;
static volatile uint8_t glossy_status;
static rtimer_clock_t t_rx_start, t_rx_stop, t_tx_start, t_tx_stop;
static rtimer_clock_t t_rx_timeout;
static rtimer_clock_t T_irq;
static unsigned short ie1, ie2, p1ie, p2ie, tbiv;

static rtimer_clock_t T_slot_h;
static rtimer_clock_t T_rx_h, T_w_rt_h, T_tx_h, T_w_tr_h;
#if GLOSSY_SYNC_WINDOW
static unsigned long T_slot_h_sum;
static uint8_t win_cnt;
#endif
static rtimer_clock_t t_ref_l, T_offset_h;
static slot_t slot;
static uint8_t t_ref_l_updated;
static volatile rtimer_clock_t t_first_rx, time_to_first_rx, min_time_to_first_rx;

/* --------------------------- SFD interrupt ------------------------ */
interrupt(TIMERB1_VECTOR)
timerb1_interrupt(void)
{
	T_irq = ((RTIMER_NOW_DCO() - TBCCR1) - 21) << 1;

	if (glossy_status == GLOSSY_STATUS_RECEIVING && !SFD_IS_1) {
		// T_irq in [0,...,8]
		if (T_irq <= 8) {
#if COMPENSATE_IRQ_DELAY
			asm volatile("add %[d], r0" : : [d] "m" (T_irq));
			asm volatile("nop");						// irq_delay = 0
			asm volatile("nop");						// irq_delay = 2
			asm volatile("nop");						// irq_delay = 4
			asm volatile("nop");						// irq_delay = 6
			asm volatile("nop");						// irq_delay = 8
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
			asm volatile("nop");
#endif
			radio_start_tx();
			/* always read TBIV to clear IFG */
			tbiv = TBIV;
			glossy_end_rx();
		} else {
			radio_flush_rx();
			glossy_status = GLOSSY_STATUS_WAITING;
			/* always read TBIV to clear IFG */
			tbiv = TBIV;
		}
	} else {
		tbiv = TBIV;
		if (glossy_status == GLOSSY_STATUS_WAITING && SFD_IS_1) {
			glossy_begin_rx();
		} else {
			if (glossy_status == GLOSSY_STATUS_RECEIVED && SFD_IS_1) {
				glossy_begin_tx();
			} else {
				if (glossy_status == GLOSSY_STATUS_TRANSMITTING && !SFD_IS_1) {
					glossy_end_tx();
				} else {
					if (glossy_status == GLOSSY_STATUS_ABORTED) {
						glossy_status = GLOSSY_STATUS_WAITING;
					} else {
						if (glossy_status == GLOSSY_STATUS_WAITING && tbiv == TBIV_CCR4) {
							// initiator timeout
							if (rx_cnt == 0) {
								tx_cnt = 0;
								// set the packet length field to the appropriate value
								GLOSSY_LEN_FIELD = packet_len;
								// set the header field
								GLOSSY_HEADER_FIELD = GLOSSY_HEADER;
								if (sync) {
									GLOSSY_SLOT_FIELD = 200;
								}
								// copy the application data to the data field
								memcpy(&GLOSSY_DATA_FIELD, data, data_len);
								// set Glossy glossy_status
								glossy_status = GLOSSY_STATUS_RECEIVED;
								radio_write_tx();
								radio_start_tx();
								TBCCR4 = RTIMER_NOW_DCO() + GLOSSY_INITIATOR_TIMEOUT;
							} else {
								TBCCTL4 = 0;
							}
						} else {
							if (glossy_status != GLOSSY_STATUS_OFF) {
								radio_flush_rx();
								glossy_status = GLOSSY_STATUS_WAITING;
							} else {
								// not Glossy
#if MAC_PROTOCOL == LPP_NEW
							if (tbiv == TBIV_CCR2) {
							  // LPP_NEW send timeout
							  TBCCTL2 = 0;
							  process_poll(&send_timeout_process);
							  LPM4_EXIT;
							} else {
								if (tbiv == TBIV_CCR3) {
								  // LPP_NEW dutycycle
								  dutycycle();
								  LPM4_EXIT;
								}
							}
#elif MAC_PROTOCOL == XMAC
							if (tbiv == TBIV_CCR2) {
								// XMAC timeout
								TBCCTL2 = 0;
								timeout();
								LPM4_EXIT;
							} else {
								if (tbiv == TBIV_CCR3) {
									// XMAC dutycycle
									powercycle();
									LPM4_EXIT;
								}
							}
#endif /* MAC_PROTOCOL */
							}
						}
					}
				}
			}
		}
	}
}

/* --------------------------- Glossy process ----------------------- */
PROCESS(glossy_process, "Glossy busy waiting process");
PROCESS_THREAD(glossy_process, ev, data) {
	PROCESS_BEGIN();

	while (1) {
		PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);
		// prevent the main cycle to enter the LPM mode or
		// any other process to run while Glossy is running
		while (GLOSSY_IS_ON());
	}

	PROCESS_END();
}

inline void glossy_disable_other_interrupts(void) {
    int s = splhigh();
	ie1 = IE1;
	ie2 = IE2;
	p1ie = P1IE;
	p2ie = P2IE;
	IE1 = 0;
	IE2 = 0;
	P1IE = 0;
	P2IE = 0;
	CACTL1 &= ~CAIE;
	DMA0CTL &= ~DMAIE;
	DMA1CTL &= ~DMAIE;
	DMA2CTL &= ~DMAIE;
	// disable etimer interrupts
	TACCTL1 &= ~CCIE;
	TBCCTL0 = 0;
	TBCCTL2 = 0;
	TBCCTL3 = 0;
	TBCCTL4 = 0;
	DISABLE_FIFOP_INT();
	CLEAR_FIFOP_INT();
	SFD_CAP_INIT(CM_BOTH);
	ENABLE_SFD_INT();
	// stop Timer B
	TBCTL = 0;
	// Timer B sourced by the DCO
	TBCTL = TBSSEL1;
	// start Timer B
	TBCTL |= MC1;
    splx(s);
    watchdog_stop();
}

inline void glossy_enable_other_interrupts(void) {
	int s = splhigh();
	IE1 = ie1;
	IE2 = ie2;
	P1IE = p1ie;
	P2IE = p2ie;
	// enable etimer interrupts
	TACCTL1 |= CCIE;
	TBCCTL0 = 0;
	TBCCTL2 = 0;
	TBCCTL3 = 0;
#if COOJA
	if (TACCTL1 & CCIFG) {
		etimer_interrupt();
	}
#endif
	DISABLE_SFD_INT();
	CLEAR_SFD_INT();
	FIFOP_INT_INIT();
	ENABLE_FIFOP_INT();
	// stop Timer B
	TBCTL = 0;
	// Timer B sourced by the 32 kHz
	TBCTL = TBSSEL0;
	// start Timer B
	TBCTL |= MC1;
    splx(s);
    watchdog_start();
}

void glossy_start(uint8_t *data_, uint8_t data_len_, uint8_t initiator_,
		uint8_t sync_, uint8_t tx_max_, uint8_t turn_radio_on) {
	// copy function arguments to the respective Glossy variables
	data = data_;
	data_len = data_len_;
	initiator = initiator_;
	sync = sync_;
	tx_max = tx_max_;
	// initialize Glossy variables
	tx_cnt = 0;
	rx_cnt = 0;

	// set Glossy packet length, depending on the sync flag value (with or without slot field)
	packet_len = (sync) ?
			data_len + FOOTER_LEN + GLOSSY_SLOT_LEN + GLOSSY_HEADER_LEN :
			data_len + FOOTER_LEN + GLOSSY_HEADER_LEN;
	// allocate memory for the packet buffer
	packet = (uint8_t *) malloc(packet_len + 1);
	// set the packet length field to the appropriate value
	GLOSSY_LEN_FIELD = packet_len;
	// set the header field
	GLOSSY_HEADER_FIELD = GLOSSY_HEADER;
	if (initiator) {
		// initiator: copy the application data to the data field
		memcpy(&GLOSSY_DATA_FIELD, data, data_len);
		// set Glossy glossy_status
		glossy_status = GLOSSY_STATUS_RECEIVED;
	} else {
		// receiver: set Glossy glossy_status
		glossy_status = GLOSSY_STATUS_WAITING;
	}
	if (sync) {
		// set the slot field to 0
		GLOSSY_SLOT_FIELD = 0;
		if (turn_radio_on) {
			t_ref_l_updated = 0;
		}
	}

	if (turn_radio_on) {
#if !COOJA
		// resynchronize the DCO
		msp430_sync_dco();
#endif
	}

	// flush radio buffers
	radio_flush_rx();
	radio_flush_tx();
	if (initiator) {
		// write the packet to the TXFIFO
		radio_write_tx();
		if (turn_radio_on) {
			// start the first transmission
			radio_start_tx();
			TBCCR4 = RTIMER_NOW_DCO() + GLOSSY_INITIATOR_TIMEOUT;
			TBCCTL4 = CCIE;
		}
	} else {
		if (turn_radio_on) {
			if (sync) {
				time_to_first_rx = RTIMER_NOW();
			}
			// turn on the radio
			radio_on();
		}
	}
	process_poll(&glossy_process);
}

uint8_t glossy_stop(void) {
	TBCCTL4 = 0;
	// turn off the radio
	radio_off();

	// flush radio buffers
	radio_flush_rx();
	radio_flush_tx();

//	printf("%u\n", T_irq);
	glossy_status = GLOSSY_STATUS_OFF;
	// deallocate memory for the packet buffer
	free(packet);
	return rx_cnt;
}

inline void glossy_estimate_slot_lengths(rtimer_clock_t t_rx_stop_tmp) {
	// estimate lengths if rx_cnt > 1
	// and we have received a packet immediately after our last transmission
	if ((rx_cnt > 1) && (GLOSSY_SLOT_FIELD == (tx_slot_last + 2))) {
		T_w_rt_h = t_tx_start - t_rx_stop;
		T_tx_h = t_tx_stop - t_tx_start;
		T_w_tr_h = t_rx_start - t_tx_stop;
		T_rx_h = t_rx_stop_tmp - t_rx_start;
		rtimer_clock_t T_slot_h_tmp = (T_tx_h + T_w_tr_h + T_rx_h + T_w_rt_h) / 2;
#if GLOSSY_SYNC_WINDOW
		T_slot_h_sum += T_slot_h_tmp;
		if ((++win_cnt) == GLOSSY_SYNC_WINDOW) {
			T_slot_h = T_slot_h_sum / GLOSSY_SYNC_WINDOW;
			T_slot_h_sum /= 2;
			win_cnt /= 2;
		} else {
			if (win_cnt == 1) {
				T_slot_h = T_slot_h_tmp;
			}
		}
#else
		T_slot_h = T_slot_h_tmp;
#endif
	}
}

inline void glossy_compute_sync_reference_point(void) {
#if COOJA
	rtimer_clock_t t_cap_l = RTIMER_NOW();
	rtimer_clock_t t_cap_h = RTIMER_NOW_DCO();
#else
	// capture the next low-frequency clock tick
	rtimer_clock_t t_cap_h, t_cap_l;
	CAPTURE_NEXT_CLOCK_TICK(t_cap_h, t_cap_l);
#endif
	rtimer_clock_t T_rx_to_cap_h = t_cap_h - t_rx_start;
	unsigned long T_ref_to_rx_h = (GLOSSY_SLOT_FIELD - 1) * (unsigned long)T_slot_h;
	unsigned long T_ref_to_cap_h = T_ref_to_rx_h + (unsigned long)T_rx_to_cap_h;
	rtimer_clock_t T_ref_to_cap_l = 1 + T_ref_to_cap_h / CLOCK_PHI;
	T_offset_h = (CLOCK_PHI - 1) - (T_ref_to_cap_h % CLOCK_PHI);
	t_ref_l = t_cap_l - T_ref_to_cap_l;
	slot = GLOSSY_SLOT_FIELD - 1;
	t_ref_l_updated = 1;
}

inline void glossy_begin_rx(void) {
	t_rx_start = TBCCR1;
	if (sync && rx_cnt == 0) {
		t_first_rx = RTIMER_NOW();
	}
	glossy_status = GLOSSY_STATUS_RECEIVING;
	t_rx_timeout = RTIMER_NOW() + 5 + (((rtimer_clock_t)packet_len * 135) / 128);
	// wait until the FIFO pin is 1 (until the first byte is received)
	while (!FIFO_IS_1) {
		if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t_rx_timeout)) {
			radio_abort_rx();
			return;
		}
	}
	// read the first byte (i.e., the len field) from the RXFIFO
	FASTSPI_READ_FIFO_BYTE(GLOSSY_LEN_FIELD);
	// keep receiving only if it has the right length
	if (GLOSSY_LEN_FIELD != packet_len) {
		// packet with a wrong length: abort reception
		radio_abort_rx();
		return;
	}

	bytes_read = 1;
#if !COOJA
	if (packet_len > 8) {
		// read all bytes but the last 8
		while (bytes_read <= packet_len - 8) {
			// wait until the FIFO pin is 1 (until one more byte is received)
			while (!FIFO_IS_1) {
				if (!RTIMER_CLOCK_LT(RTIMER_NOW(), t_rx_timeout)) {
					radio_abort_rx();
					return;
				}
			}
			// read another byte from the RXFIFO
			FASTSPI_READ_FIFO_BYTE(packet[bytes_read]);
			bytes_read++;
			if ((bytes_read == 2) && (GLOSSY_HEADER_FIELD != GLOSSY_HEADER)) {
				radio_abort_rx();
				return;
			}
		}
	}
#endif
	glossy_schedule_rx_timeout();
}

inline void glossy_end_rx(void) {
	rtimer_clock_t t_rx_stop_tmp = TBCCR1;
	// read the remaining bytes from the RXFIFO
	FASTSPI_READ_FIFO_NO_WAIT(&packet[bytes_read], packet_len - bytes_read + 1);
	bytes_read = packet_len + 1;
#if COOJA
	if ((GLOSSY_CRC_FIELD & FOOTER1_CRC_OK) && (GLOSSY_HEADER_FIELD == GLOSSY_HEADER)) {
#else
	if (GLOSSY_CRC_FIELD & FOOTER1_CRC_OK) {
#endif
		// packet correctly received
		if (sync) {
			// increment slot field
			GLOSSY_SLOT_FIELD++;
		}
		if (tx_cnt == tx_max) {
			// no more Tx to perform: stop Glossy
			radio_off();
			glossy_status = GLOSSY_STATUS_OFF;
		} else {
			// write Glossy packet to the TXFIFO
			radio_write_tx();
			glossy_status = GLOSSY_STATUS_RECEIVED;
		}
		if (sync && rx_cnt == 0) {
			time_to_first_rx = t_first_rx - time_to_first_rx;
		}
		rx_cnt++;
		if (sync && (GLOSSY_SLOT_FIELD < 200)) {
			glossy_estimate_slot_lengths(t_rx_stop_tmp);
		}
		t_rx_stop = t_rx_stop_tmp;
		if (initiator) {
			TBCCTL4 = 0;
		}
	} else {
		// packet corrupted, abort the transmission before it starts
		radio_abort_tx();
		glossy_status = GLOSSY_STATUS_WAITING;
	}
}

inline void glossy_begin_tx(void) {
	t_tx_start = TBCCR1;
	glossy_status = GLOSSY_STATUS_TRANSMITTING;
	tx_slot_last = GLOSSY_SLOT_FIELD;
	if ((!initiator) && (rx_cnt == 1)) {
		// copy the application data from the data field
		memcpy(data, &GLOSSY_DATA_FIELD, data_len);
	}
	if ((sync) && (T_slot_h) && (!t_ref_l_updated) && (rx_cnt) && (GLOSSY_SLOT_FIELD < 200)) {
		// compute the sync reference point after the first reception (higher accuracy)
		glossy_compute_sync_reference_point();
	}
}

inline void glossy_end_tx(void) {
	ENERGEST_GLOSSY_OFF(ENERGEST_TYPE_TRANSMIT);
	ENERGEST_GLOSSY_ON(ENERGEST_TYPE_LISTEN);
	t_tx_stop = TBCCR1;
	// stop Glossy if tx_cnt reached tx_max (and tx_max > 1 at the initiator, if sync is enabled)
	if ((++tx_cnt == tx_max) && ((!sync) || ((tx_max - initiator) > 0))) {
		radio_off();
		glossy_status = GLOSSY_STATUS_OFF;
	} else {
		glossy_status = GLOSSY_STATUS_WAITING;
	}
	radio_flush_tx();
}

/* ------------------------------ Timeouts -------------------------- */
inline void glossy_schedule_rx_timeout(void) {
	TACCR2 = t_rx_timeout;
	TACCTL2 = CCIE;
}
inline void glossy_stop_rx_timeout(void) {
	TACCTL2 = 0;
}

/* ------------------------------- Radio ---------------------------- */
inline void radio_flush_tx(void) {
	FASTSPI_STROBE(CC2420_SFLUSHTX);
}
inline uint8_t radio_glossy_status(void) {
	uint8_t glossy_status;
	FASTSPI_UPD_STATUS(glossy_status);
	return glossy_status;
}
inline void radio_on(void) {
	FASTSPI_STROBE(CC2420_SRXON);
	while(!(radio_glossy_status() & (BV(CC2420_XOSC16M_STABLE))));
	ENERGEST_GLOSSY_ON(ENERGEST_TYPE_LISTEN);
}
inline void radio_off(void) {
	if (energest_glossy_current_mode[ENERGEST_TYPE_TRANSMIT]) {
		ENERGEST_GLOSSY_OFF(ENERGEST_TYPE_TRANSMIT);
	}
	if (energest_glossy_current_mode[ENERGEST_TYPE_LISTEN]) {
		ENERGEST_GLOSSY_OFF(ENERGEST_TYPE_LISTEN);
	}
	FASTSPI_STROBE(CC2420_SRFOFF);
}
inline void radio_flush_rx(void) {
	uint8_t dummy;
	FASTSPI_READ_FIFO_BYTE(dummy);
	FASTSPI_STROBE(CC2420_SFLUSHRX);
	FASTSPI_STROBE(CC2420_SFLUSHRX);
}
inline void radio_abort_rx(void) {
	if (glossy_status == GLOSSY_STATUS_RECEIVING) {
		glossy_status = GLOSSY_STATUS_ABORTED;
		radio_flush_rx();
	}
	glossy_stop_rx_timeout();
}
inline void radio_abort_tx(void) {
	FASTSPI_STROBE(CC2420_SRXON);
	if (energest_glossy_current_mode[ENERGEST_TYPE_TRANSMIT]) {
		ENERGEST_GLOSSY_OFF(ENERGEST_TYPE_TRANSMIT);
		ENERGEST_GLOSSY_ON(ENERGEST_TYPE_LISTEN);
	}
	radio_flush_rx();
}
inline void radio_start_tx(void) {
	FASTSPI_STROBE(CC2420_STXON);
	ENERGEST_GLOSSY_OFF(ENERGEST_TYPE_LISTEN);
	ENERGEST_GLOSSY_ON(ENERGEST_TYPE_TRANSMIT);
}
inline void radio_write_tx(void) {
	FASTSPI_WRITE_FIFO(packet, packet_len - 1);
}

slot_t get_relay_cnt(void) {
	return slot;
}

rtimer_clock_t get_T_slot_h(void) {
	return T_slot_h;
}

uint8_t is_t_ref_l_updated(void) {
	return t_ref_l_updated;
}

rtimer_clock_t get_t_ref_l(void) {
	return t_ref_l;
}

void set_t_ref_l(rtimer_clock_t t) {
	t_ref_l = t;
}

void set_t_ref_l_updated(uint8_t updated) {
	t_ref_l_updated = updated;
}

uint8_t get_glossy_status(void) {
	return glossy_status;
}

rtimer_clock_t get_time_to_first_rx(void) {
	return time_to_first_rx;
}

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

#ifndef LPP_NEW_H_
#define LPP_NEW_H_

#include "sys/rtimer.h"
#include "net/mac/mac.h"
#include "net/rime/relunicast.h"
#include "dev/radio.h"
#include <signal.h>

struct lpp_new_config {
  rtimer_clock_t on_time;
  rtimer_clock_t off_time;
  rtimer_clock_t tx_timeout;
};

extern const struct mac_driver lpp_new_driver;

const struct mac_driver *lpp_new_init(const struct radio_driver *d);

const struct lpp_new_config *get_lpp_new_config();

void set_lpp_new_config(const struct lpp_new_config *config);

uint16_t lpp_new_get_prr();

void lpp_new_reset_prr();

#if GLOSSY
extern struct process send_timeout_process;
char dutycycle(void);
#endif /* GLOSSY */

#endif /* LPP_NEW_H_ */

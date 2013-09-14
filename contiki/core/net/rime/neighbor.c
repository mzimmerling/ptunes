/**
 * \addtogroup rimeneighbor
 * @{
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
 * $Id: neighbor.c,v 1.18 2009/04/06 21:19:00 adamdunkels Exp $
 */

/**
 * \file
 *         Radio neighborhood management
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include <limits.h>
#include <stdio.h>

#include "contiki.h"
#include "lib/memb.h"
#include "lib/list.h"
#include "net/rime/neighbor.h"
#include "net/rime/ctimer.h"
#include "net/rime/relcollect.h"

#define MAX_NEIGHBORS 16

#define RTMETRIC_MAX RELCOLLECT_MAX_DEPTH

MEMB(neighbors_mem, struct neighbor, MAX_NEIGHBORS);
LIST(neighbors_list);

/*static struct neighbor neighbors[MAX_NEIGHBORS];*/

static struct neighbor * last_best = NULL;

static struct ctimer t;

static int max_time = 500;

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif


/*---------------------------------------------------------------------------*/
static void
periodic(void *ptr)
{  
  struct neighbor *n, *next;

  /* Go through all neighbors and remove old ones. */
  for(n = list_head(neighbors_list); n != NULL; n = next) {
    next = NULL;
    /*  for(i = 0; i < MAX_NEIGHBORS; ++i) {*/
    if(!rimeaddr_cmp(&n->addr, &rimeaddr_null) &&
       n->time < max_time) {
      n->time++;
      if(n->time == max_time) {
	n->rtmetric = RTMETRIC_MAX;
	PRINTF("%d.%d: periodic: removing old neighbor %d.%d\n",
	       rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
	       n->addr.u8[0], n->addr.u8[1]);
	rimeaddr_copy(&n->addr, &rimeaddr_null);
	next = n->next;
	list_remove(neighbors_list, n);
	memb_free(&neighbors_mem, n);
      }
    }
    if(next == NULL) {
      next = n->next;
    }
  }
  /*  PRINTF("neighbor periodic\n");*/
#if !STATIC
  ctimer_set(&t, CLOCK_SECOND, periodic, NULL);
#endif /* !STATIC */
}
/*---------------------------------------------------------------------------*/
void
neighbor_init(void)
{
  
  memb_init(&neighbors_mem);
  list_init(neighbors_list);
  /*  for(i = 0; i < MAX_NEIGHBORS; ++i) {
    rimeaddr_copy(&neighbors[i].addr, &rimeaddr_null);
    }*/
#if !STATIC
  ctimer_set(&t, CLOCK_SECOND, periodic, NULL);
#endif /* !STATIC */
}
/*---------------------------------------------------------------------------*/
struct neighbor *
neighbor_find(rimeaddr_t *addr)
{
  struct neighbor *n;
  for(n = list_head(neighbors_list); n != NULL; n = n->next) {
    if(rimeaddr_cmp(&n->addr, addr)) {
      return n;
    }
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
void
neighbor_update(struct neighbor *n, uint8_t rtmetric)
{
  if(n != NULL) {
    n->rtmetric = rtmetric;
    n->time = 0;
  }
}
/*---------------------------------------------------------------------------*/
void
neighbor_timedout_etx(struct neighbor *n, uint8_t etx)
{
  if(n != NULL) {
    n->etxs[n->etxptr] += etx;
    n->etxptr = (n->etxptr + 1) % NEIGHBOR_NUM_ETXS;
  }
}
/*---------------------------------------------------------------------------*/
void
neighbor_update_etx(struct neighbor *n, uint8_t etx)
{
  if(n != NULL) {
    n->etxs[n->etxptr] = etx;
    n->etxptr = (n->etxptr + 1) % NEIGHBOR_NUM_ETXS;
    n->time = 0;
  }
}
/*---------------------------------------------------------------------------*/
uint8_t
neighbor_etx(struct neighbor *n)
{
  int i, etx;

  etx = 0;
  for(i = 0; i < NEIGHBOR_NUM_ETXS; ++i) {
    etx += n->etxs[i];
  }
  return NEIGHBOR_ETX_SCALE * etx / NEIGHBOR_NUM_ETXS;
}
/*---------------------------------------------------------------------------*/
void
neighbor_add(rimeaddr_t *addr, uint8_t nrtmetric, uint8_t netx)
{
  uint16_t rtmetric;
  uint16_t etx;
  struct neighbor *n, *max;
  int i;

  PRINTF("%d.%d: neighbor_add: adding %d.%d\n", rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1], addr->u8[0], addr->u8[1]);
  
  /* Check if the neighbor is already on the list. */
  for(n = list_head(neighbors_list); n != NULL; n = n->next) {
    if(rimeaddr_cmp(&n->addr, &rimeaddr_null) ||
       rimeaddr_cmp(&n->addr, addr)) {
      PRINTF("%d.%d: neighbor_add: already on list %d.%d\n", rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1], addr->u8[0], addr->u8[1]);
      break;
    }
  }

  /* If the neighbor was not on the list, we try to allocate memory
     for it. */
  if(n == NULL) {
    PRINTF("%d.%d: neighbor_add: not on list, allocating %d.%d\n", rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1], addr->u8[0], addr->u8[1]);
    n = memb_alloc(&neighbors_mem);
    if(n != NULL) {
      list_add(neighbors_list, n);
    }
  }
  
  /* If we could not allocate memory, we try to recycle an old
     neighbor */
  if(n == NULL) {
    PRINTF("%d.%d: neighbor_add: not on list, not allocated, recycling %d.%d\n", rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1], addr->u8[0], addr->u8[1]);
   /* Find the first unused entry or the used entry with the highest
     rtmetric and highest etx. */
    rtmetric = 0;
    etx = 0;
    max = NULL;

    for(n = list_head(neighbors_list); n != NULL; n = n->next) {
      if(!rimeaddr_cmp(&n->addr, &rimeaddr_null) && !rimeaddr_cmp(&n->addr, &last_best->addr)) {
	if(n->rtmetric > rtmetric) {
	  rtmetric = n->rtmetric;
	  etx = neighbor_etx(n);
	  max = n;
	} else if(n->rtmetric == rtmetric) {
	  if(neighbor_etx(n) > etx) {
	    rtmetric = n->rtmetric;
	    etx = neighbor_etx(n);
	    max = n;
	    /*	PRINTF("%d: found worst neighbor %d with rtmetric %d, signal %d\n",
		node_id, neighbors[n].nodeid, rtmetric, signal);*/
	  }
	}
      }
    }
    n = max;
  }


  /*  PRINTF("%d: adding neighbor %d with rtmetric %d, signal %d at %d\n",
      node_id, neighbors[n].nodeid, rtmetric, signal, n);*/
  if(n != NULL) {
    n->time = 0;
    rimeaddr_copy(&n->addr, addr);
    n->rtmetric = nrtmetric;
    for(i = 0; i < NEIGHBOR_NUM_ETXS; ++i) {
      n->etxs[i] = netx;
    }
    n->etxptr = 0;
  }
}
/*---------------------------------------------------------------------------*/
void
neighbor_remove(rimeaddr_t *addr)
{
  struct neighbor *n;

  for(n = list_head(neighbors_list); n != NULL; n = n->next) {
    if(rimeaddr_cmp(&n->addr, addr)) {
      PRINTF("%d.%d: neighbor_remove: removing %d.%d\n",
	     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	     addr->u8[0], addr->u8[1]);
      /* Check if we are going to remove the last best node. */
      if (last_best != NULL) {
    	  if (rimeaddr_cmp(&n->addr, &last_best)) {
    		  last_best = NULL;
    	  }
      }
      rimeaddr_copy(&n->addr, &rimeaddr_null);
      n->rtmetric = RTMETRIC_MAX;
      list_remove(neighbors_list, n);
      memb_free(&neighbors_mem, n);
      return;
    }
  }
  /*  int i;

  for(i = 0; i < MAX_NEIGHBORS; ++i) {
    if(rimeaddr_cmp(&neighbors[i].addr, addr)) {
      PRINTF("%d: removing %d @ %d\n", rimeaddr_node_addr.u16[0], addr->u16[0], i);
      rimeaddr_copy(&neighbors[i].addr, &rimeaddr_null);
      neighbors[i].rtmetric = RTMETRIC_MAX;
      return;
    }
    }*/
}
/*---------------------------------------------------------------------------*/
struct neighbor *
neighbor_best(void)
{
  struct neighbor *n = NULL;
  struct neighbor *best = NULL;
  uint16_t rtmetric = RTMETRIC_MAX;

  /* Find lowest rtmetric + etx. */
  for(n = list_head(neighbors_list); n != NULL; n = n->next) {
	  PRINTF("%u.%u: neighbor_best: neighbor %u.%u with rtmetric %u and etx %u\n",
			  rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
			  n->addr.u8[0], n->addr.u8[1],
			  n->rtmetric,
			  neighbor_etx(n));
    if(  !rimeaddr_cmp(&n->addr, &rimeaddr_null)
      && rtmetric > n->rtmetric + neighbor_etx(n)) {
      rtmetric = n->rtmetric + neighbor_etx(n);
      best = n;
    }
  }

  if (last_best == NULL) {
	  // printf("%d: first parent %d (rtmetric %u)\n", rimeaddr_node_addr.u8[0], best->addr.u8[0], rtmetric);
	  last_best = best;
  } else {
	  if (! (rtmetric + 2 < last_best->rtmetric + neighbor_etx(last_best))) {
		  /* No improvement in rtmetric, so we keep the current best. */
		  best = last_best;
	  } else {
		  /* Improvement in rtmetric, so we switch to new best. */
		  // printf("%d: new parent %d (rtmetric %u), old parent %d (rtmetric %u)\n", rimeaddr_node_addr.u8[0], best->addr.u8[0], rtmetric, last_best->addr.u8[0], last_best->rtmetric + neighbor_etx(last_best));
		  last_best = best;
	  }
  }

  return best;
}
/*---------------------------------------------------------------------------*/
void
neighbor_set_lifetime(int seconds)
{
  max_time = seconds;
}
/*---------------------------------------------------------------------------*/
int
neighbor_num(void)
{
  PRINTF("neighbor_num %d\n", list_length(neighbors_list));
  return list_length(neighbors_list);
}
/*---------------------------------------------------------------------------*/
struct neighbor *
neighbor_get(int num)
{
  int i;
  struct neighbor *n;

  PRINTF("neighbor_get %d\n", num);
  
  i = 0;
  for(n = list_head(neighbors_list); n != NULL; n = n->next) {
    if(i == num) {
      PRINTF("neighbor_get found %d.%d\n", n->addr.u8[0], n->addr.u8[1]);
      return n;
    }
    i++;
  }
  return NULL;
}
/*---------------------------------------------------------------------------*/
/** @} */

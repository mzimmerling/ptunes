#include "net/rime/relunicast.h"
#include "net/rime/neighbor.h"
#include "net/rime.h"
#if MAC_PROTOCOL == XMAC
 #include "net/mac/xmac.h"
#elif MAC_PROTOCOL == LPP
 #include "net/mac/lpp.h"
#elif MAC_PROTOCOL == LPP_NEW
 #include "net/mac/lpp_new.h"
#endif /* MAC_PROTOCOL */
#include "random.h"
#include <string.h>

#if WITH_FTSP
#include "net/rime/ftsp.h"
#endif /* WITH_FTSP */

#define RELUNICAST_PACKET_ID_BITS 4

#if MAC_PROTOCOL == XMAC
#define T_DUTY ((get_xmac_config()->off_time + get_xmac_config()->on_time)/(RTIMER_ARCH_SECOND/CLOCK_SECOND))
#elif MAC_PROTOCOL == LPP
#define T_DUTY (get_lpp_config()->off_time + get_lpp_config()->on_time)
static struct relunicast_conn *c_send;
#elif MAC_PROTOCOL == LPP_NEW
#define T_DUTY ((get_lpp_new_config()->off_time + get_lpp_new_config()->on_time)/(RTIMER_ARCH_SECOND/CLOCK_SECOND))
static struct relunicast_conn *c_send;
#endif /* MAC_PROTOCOL */
#define RANDOM_DELAY (random_rand() % T_DUTY)
#define REXMIT_TIME (T_DUTY + RANDOM_DELAY)

static const struct packetbuf_attrlist attributes[] = { RELUNICAST_ATTRIBUTES PACKETBUF_ATTR_LAST };

#include <stdio.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#if GLOSSY
#include "glossy.h"
extern int period_skew;
void schedule_send(struct relunicast_conn *c, clock_time_t scheduled_time);
#endif

/*---------------------------------------------------------------------------*/
static void recv_from_unicast(struct unicast_conn *uc, rimeaddr_t *from) {
	struct relunicast_conn *c = (struct relunicast_conn *) uc;

	PRINTF("%d.%d: relunicast: recv_from_unicast: received packet %u from %d.%d\n",
			rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
			packetbuf_attr(PACKETBUF_ATTR_PACKET_ID),
			from->u8[0], from->u8[1]);

	if (c->u->recv != NULL) {
		c->u->recv(c, from, (uint8_t) packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));
	}
}
/*---------------------------------------------------------------------------*/
static const struct unicast_callbacks relunicast = { recv_from_unicast };
/*---------------------------------------------------------------------------*/
void relunicast_open(struct relunicast_conn *c, uint16_t channel, const struct relunicast_callbacks *u) {
	unicast_open(&c->c, channel, &relunicast);
	channel_set_attributes(channel, attributes);
	c->u = u;
	c->is_tx = 0;
	c->rxmit = 0;
	c->sndnxt = 0;
}
/*---------------------------------------------------------------------------*/
void relunicast_close(struct relunicast_conn *c) {
	unicast_close(&c->c);
	ctimer_stop(&c->t);
	if (c->buf != NULL) {
		queuebuf_free(c->buf);
	}
}
/*---------------------------------------------------------------------------*/
uint8_t relunicast_is_transmitting(struct relunicast_conn *c) {
	return c->is_tx;
}
/*---------------------------------------------------------------------------*/
rimeaddr_t * relunicast_receiver(struct relunicast_conn *c) {
	return &c->receiver;
}
/*---------------------------------------------------------------------------*/
static void send(void *ptr) {
	struct relunicast_conn *c = ptr;

	if (c->rxmit > c->max_rxmit) {
		PRINTF("%d.%d: relunicast: send: packet %d to %d.%d timed out\n",
				rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
				c->sndnxt,
				c->receiver.u8[0], c->receiver.u8[1]);
		c->sndnxt = (c->sndnxt + 1) % (1 << RELUNICAST_PACKET_ID_BITS);
		c->is_tx = 0;
		c->rxmit--;
		ctimer_stop(&c->t);
		if(c->u->timedout) {
	    	c->u->timedout(c, &c->receiver, c->rxmit);
	    }
	} else {
		PRINTF("%d.%d: relunicast: send: (re-)sending packet %d to %d.%d\n",
				rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
				c->sndnxt,
				c->receiver.u8[0], c->receiver.u8[1]);
		c->rxmit++;
		queuebuf_to_packetbuf(c->buf);
		if (rimeaddr_cmp(&c->esender, &rimeaddr_node_addr)) {
#if WITH_FTSP || GLOSSY
			packetbuf_set_attr(PACKETBUF_ATTR_TIME_HIGH, c->time_high);
			packetbuf_set_attr(PACKETBUF_ATTR_TIME_LOW, c->time_low);
#endif /* WITH_FTSP */
		}
#if MAC_PROTOCOL == LPP || MAC_PROTOCOL == LPP_NEW
		c_send = c;
#endif /* MAC_PROTOCOL */
		unicast_send(&c->c, &c->receiver);
#if MAC_PROTOCOL == XMAC
		if (xmac_got_data_ack()) {
			PRINTF("%d.%d: relunicast: send: successfully sent packet %d to %d.%d\n",
					rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
					c->sndnxt,
					c->receiver.u8[0], c->receiver.u8[1]);
			c->sndnxt = (c->sndnxt + 1) % (1 << RELUNICAST_PACKET_ID_BITS);
			c->is_tx = 0;
			c->rxmit--;
			ctimer_stop(&c->t);
			if (c->u->sent != NULL) {
				c->u->sent(c, &c->receiver, c->rxmit);
			}
		} else {
#if GLOSSY
			schedule_send(c, (clock_time_t) REXMIT_TIME);
#else
			ctimer_set(&c->t, (clock_time_t) REXMIT_TIME, send, c);
#endif /*GLOSSY */
		}
#endif /* MAC_PROTOCOL */
	}
}
/*---------------------------------------------------------------------------*/
#if MAC_PROTOCOL == LPP || MAC_PROTOCOL == LPP_NEW
void post_send(int acknowledged)
{
	if (acknowledged) {
		PRINTF("%d.%d: relunicast: post_send: successfully sent packet %d to %d.%d\n",
				rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
				c_send->sndnxt,
				c_send->receiver.u8[0], c_send->receiver.u8[1]);
		c_send->sndnxt = (c_send->sndnxt + 1) % (1 << RELUNICAST_PACKET_ID_BITS);
		c_send->is_tx = 0;
		c_send->rxmit--;
		ctimer_stop(&c_send->t);
		if (c_send->u->sent != NULL) {
			c_send->u->sent(c_send, &c_send->receiver, c_send->rxmit);
		}
	} else {
		PRINTF("%d.%d: relunicast: post_send: NOT successfully sent packet %d to %d.%d\n",
				rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
				c_send->sndnxt,
				c_send->receiver.u8[0], c_send->receiver.u8[1]);
#if GLOSSY
		schedule_send(c_send, (clock_time_t) REXMIT_TIME);
#else
		ctimer_set(&c_send->t, (clock_time_t) REXMIT_TIME, send, c_send);
#endif
	}
}
#endif /* MAC_PROTOCOL */
/*---------------------------------------------------------------------------*/
int relunicast_send(struct relunicast_conn *c, rimeaddr_t *receiver,
		uint8_t max_retransmissions, rimeaddr_t *esender) {
	if (relunicast_is_transmitting(c)) {
		PRINTF("%d.%d: relunicast: already transmitting\n",
				rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1]);
		return 0;
	}

	if (c->buf != NULL) {
		queuebuf_free(c->buf);
	}
	packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, c->sndnxt);
	c->buf = queuebuf_new_from_packetbuf();
	if (c->buf == NULL) {
		return 0;
	}
	c->max_rxmit = max_retransmissions;
	c->rxmit = 0;
	c->is_tx = 1;
	rimeaddr_copy(&c->esender, esender);
	rimeaddr_copy(&c->receiver, receiver);
#if WITH_FTSP
	// set the time attribute only if we are the esender (and this is the first transmission attempt)
	if (rimeaddr_cmp(&c->esender, &rimeaddr_node_addr)) {
		rtimer_long_clock_t time = get_global_time();
		c->time_high = (rtimer_clock_t) (time >> LOW_SHIFT_RIGHT);
		c->time_low = (rtimer_clock_t) (time & 0xffff);
	}
#endif /* WITH_FTSP */
#if GLOSSY
	// set the time attribute only if we are the esender (and this is the first transmission attempt)
	if (rimeaddr_cmp(&c->esender, &rimeaddr_node_addr)) {
		unsigned long time = TIME_FROM_GLOSSY;
		c->time_high = (rtimer_clock_t) (time >> 16);
		c->time_low = (rtimer_clock_t) (time & 0xffff);
	}
	schedule_send(c, (clock_time_t) RANDOM_DELAY);
#else
	ctimer_set(&c->t, (clock_time_t) RANDOM_DELAY, send, c);
#endif /* GLOSSY */

	return 1;
}
/*---------------------------------------------------------------------------*/
#if GLOSSY
void schedule_send(struct relunicast_conn *c, clock_time_t scheduled_time) {
	int time_to_glossy = TIME_TO_GLOSSY/(RTIMER_ARCH_SECOND/CLOCK_SECOND);
	int glossy_duration = GLOSSY_DURATION/(RTIMER_ARCH_SECOND/CLOCK_SECOND);
	int send_buffer = time_to_glossy - scheduled_time - SLACK_BEFORE_GLOSSY;
	if (send_buffer >= 0) {
		/* There is enough time before Glossy starts */
		ctimer_set(&c->t, scheduled_time, send, c);
		PRINTF("schedule normally before Glossy (send buffer %d)\n", send_buffer);
	} else {
		int time_after_glossy = ((int) scheduled_time) - time_to_glossy - glossy_duration - SLACK_AFTER_GLOSSY;
		if (time_after_glossy >= 0) {
			/* The send is scheduled after Glossy */
			ctimer_set(&c->t, scheduled_time, send, c);
			PRINTF("schedule normally after Glossy (time after glossy %d)\n", time_after_glossy);
		} else {
			/* The send is scheduled too close to the start of Glossy or even during
			   Glossy, so we re-schedule it immediately after Glossy finishes */
			ctimer_set(&c->t, (clock_time_t) (time_to_glossy + glossy_duration + SLACK_AFTER_GLOSSY), send, c);
			PRINTF("schedule adjusted (time after glossy %d)\n", time_after_glossy);
		}
	}
}
/*---------------------------------------------------------------------------*/
#endif /*GLOSSY */

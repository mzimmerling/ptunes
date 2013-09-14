#ifndef __RELUNICAST_H__
#define __RELUNICAST_H__

#include "net/rime/unicast.h"
#include "net/rime/ctimer.h"
#include "net/rime/queuebuf.h"
#include "sys/rtimer.h"

struct relunicast_conn;

#if WITH_FTSP || GLOSSY
#define RELUNICAST_ATTRIBUTES  { PACKETBUF_ATTR_PACKET_ID, PACKETBUF_ATTR_BIT * 8 }, \
                               { PACKETBUF_ATTR_TIME_HIGH, PACKETBUF_ATTR_BYTE* 4 }, \
                               { PACKETBUF_ATTR_TIME_LOW,  PACKETBUF_ATTR_BYTE* 4 }, \
                                 UNICAST_ATTRIBUTES
#else
#define RELUNICAST_ATTRIBUTES  { PACKETBUF_ATTR_PACKET_ID, PACKETBUF_ATTR_BIT * 8 }, \
                                 UNICAST_ATTRIBUTES
#endif /* WITH_FTSP */

struct relunicast_callbacks {
  void (* recv)(struct relunicast_conn *c, rimeaddr_t *from, uint8_t seqno);
  void (* sent)(struct relunicast_conn *c, rimeaddr_t *to, uint8_t retransmissions);
  void (* timedout)(struct relunicast_conn *c, rimeaddr_t *to, uint8_t retransmissions);
};

#if WITH_FTSP || GLOSSY
struct relunicast_conn {
  struct unicast_conn c;
  struct ctimer t;
  struct queuebuf *buf;
  const struct relunicast_callbacks *u;
  rimeaddr_t receiver;
  uint8_t sndnxt;
  uint8_t is_tx;
  uint8_t rxmit;
  uint8_t max_rxmit;
  rimeaddr_t esender;
  rtimer_clock_t time_high;
  rtimer_clock_t time_low;
};
#else
struct relunicast_conn {
  struct unicast_conn c;
  struct ctimer t;
  struct queuebuf *buf;
  const struct relunicast_callbacks *u;
  rimeaddr_t receiver;
  uint8_t sndnxt;
  uint8_t is_tx;
  uint8_t rxmit;
  uint8_t max_rxmit;
  rimeaddr_t esender;
};
#endif /* WITH_FTSP */

void relunicast_open(struct relunicast_conn *c, uint16_t channel, const struct relunicast_callbacks *u);
void relunicast_close(struct relunicast_conn *c);
int relunicast_send(struct relunicast_conn *c, rimeaddr_t *receiver, uint8_t max_retransmissions,
		rimeaddr_t *esender);
uint8_t relunicast_is_transmitting(struct relunicast_conn *c);
rimeaddr_t *relunicast_receiver(struct relunicast_conn *c);
void post_send(int acknowledged);

#endif /* __RELUNICAST_H__ */

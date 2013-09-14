/* -*- C -*- */
/* @(#)$Id: contiki-conf.h,v 1.59 2009/09/07 11:31:26 nifi Exp $ */

#ifndef CONTIKI_CONF_H
#define CONTIKI_CONF_H

#define WITH_NULLMAC 0

#define XMAC         0
#define LPP          1
#define LPP_NEW      2

/* Specifies the MAC protocol, either XMAC or LPP_NEW. */
#define MAC_PROTOCOL XMAC
#define MAC_CONF     4

// pre-defined MAC configurations
#if MAC_PROTOCOL == XMAC
#if MAC_CONF == 1
 #define DEFAULT_MAX_NRTX 8
 #define XMAC_DEFAULT_ON_TIME    524 //  16 ms
 #define XMAC_DEFAULT_OFF_TIME  3277 // 100 ms
#elif MAC_CONF == 2
 #define DEFAULT_MAX_NRTX 5
 #define XMAC_DEFAULT_ON_TIME    360 //  11 ms
 #define XMAC_DEFAULT_OFF_TIME  8192 // 250 ms
#elif MAC_CONF == 3
 #define DEFAULT_MAX_NRTX 2
 #define XMAC_DEFAULT_ON_TIME    197 //   6 ms
 #define XMAC_DEFAULT_OFF_TIME 16384 // 500 ms
#elif MAC_CONF == 4 // quasi-optimal @ 30 s
 #define DEFAULT_MAX_NRTX 3
 #define XMAC_DEFAULT_ON_TIME    197 //   6 ms
 #define XMAC_DEFAULT_OFF_TIME  3277 // 100 ms
#elif MAC_CONF == 5 // quasi-optimal @ 300 s
 #define DEFAULT_MAX_NRTX 2
 #define XMAC_DEFAULT_ON_TIME    360 //  11 ms
 #define XMAC_DEFAULT_OFF_TIME 11469 // 350 ms
#endif /* MAC_CONF */
#elif MAC_PROTOCOL == LPP_NEW
#define LPP_NEW_DEFAULT_ON_TIME        256 // 7.8 ms
#if MAC_CONF == 1
 #define DEFAULT_MAX_NRTX 8
 #define LPP_NEW_DEFAULT_OFF_TIME     3277 // 100 ms
 #define LPP_NEW_DEFAULT_LISTEN_TIME  3801 // 116 ms (k = 1)
#elif MAC_CONF == 2
 #define DEFAULT_MAX_NRTX 5
 #define LPP_NEW_DEFAULT_OFF_TIME     8192 // 250 ms
 #define LPP_NEW_DEFAULT_LISTEN_TIME  8716 // 266 ms (k = 1)
#elif MAC_CONF == 3
 #define DEFAULT_MAX_NRTX 2
 #define LPP_NEW_DEFAULT_OFF_TIME    16384 // 500 ms
 #define LPP_NEW_DEFAULT_LISTEN_TIME 16908 // 516 ms (k = 1)
#endif /* MAC_CONF */
#endif /* MAC_PROTOCOL */

/* GPIO pins */
#define GIO2_ON()        do { P2OUT |=  BV(3); } while (0)
#define GIO2_OFF()       do { P2OUT &= ~BV(3); } while (0)
#define GIO2_IS_ON()     (P2OUT & BV(3))
#define GIO2_TOGGLE()    do { if (GIO2_IS_ON()) GIO2_OFF(); else GIO2_ON(); } while (0)
#define GIO2_BLINK()     do { GIO2_ON(); clock_delay(100); GIO2_OFF(); } while (0)
#define GIO2_INIT()      do {\
		/* select GPIO pin GIO2 on port P2.3 */\
		P2SEL &= ~BV(3); \
		/* select OUT direction */\
		P2DIR |= BV(3); \
} while (0)
#define GIO3_ON()        do { P2OUT |=  BV(6); } while (0)
#define GIO3_OFF()       do { P2OUT &= ~BV(6); } while (0)
#define GIO3_IS_ON()     (P2OUT & BV(6))
#define GIO3_TOGGLE()    do { if (GIO3_IS_ON()) GIO3_OFF(); else GIO3_ON(); } while (0)
#define GIO3_BLINK()     do { GIO3_ON(); clock_delay(100); GIO3_OFF(); } while (0)
#define GIO3_INIT()      do {\
		/* select GPIO pin GIO3 on port P2.6 */\
		P2SEL &= ~BV(6); \
		/* select OUT direction */\
		P2DIR |= BV(6); \
} while (0)

#if MAC_PROTOCOL == XMAC
 #define MAC_CONF_DRIVER xmac_driver
 #define MAC_GET_PRR() xmac_get_prr()
 #define XMAC_CONF_COMPOWER 0
 #define XMAC_CONF_ANNOUNCEMENTS 0
 #define HANDSHAKES_RESET_PERIOD 20
#elif MAC_PROTOCOL == LPP
 #define MAC_GET_PRR() lpp_get_prr()
 #define MAC_CONF_DRIVER lpp_driver
 #define WITH_LPP_ANNOUNCEMENTS 0
 #define LPP_DEFAULT_ON_TIME CLOCK_SECOND/128	// 7.8 ms
 #define LPP_DEFAULT_OFF_TIME CLOCK_SECOND/8	// 125 ms
 #define HANDSHAKES_RESET_PERIOD 20
#elif MAC_PROTOCOL == LPP_NEW
 #define MAC_GET_PRR() lpp_new_get_prr()
 #define MAC_CONF_DRIVER lpp_new_driver
 #define HANDSHAKES_RESET_PERIOD 10
 #define LPP_NEW_MAX_RANDOM_OFF_TIME RTIMER_SECOND/64 // 15.6 ms
#endif /* MAC_PROTOCOL */

/* Adaptive MAC settings */
#define GLOSSY 1
#define EXCLUDE_TRICKLE_ENERGY 1
#define QUEUING_STATS 1
#if QUEUING_STATS
#define QUEUING_DELAY_RESET_PERIOD 10
#endif /* QUEUING_STATS */

#if GLOSSY
// Set COOJA to 1 to simulate Glossy in Cooja
#define COOJA 0
// Disable FTSP and Trickle if Glossy is enabled
#define WITH_FTSP 0
#define TRICKLE_ON 0
// Constants for Glossy timing
#define GLOSSY_INIT_PERIOD      (GLOSSY_INIT_DURATION + RTIMER_SECOND / 100)
#define GLOSSY_INIT_DURATION    (GLOSSY_DURATION - GLOSSY_GUARD_TIME + GLOSSY_INIT_GUARD_TIME)
#define GLOSSY_INIT_GUARD_TIME  (RTIMER_SECOND / 20)   // / 20
#define GLOSSY_PERIOD           ((unsigned long)RTIMER_SECOND * 60)
#if COOJA
#define GLOSSY_DURATION         (RTIMER_SECOND / 60)
#define GLOSSY_GUARD_TIME       (RTIMER_SECOND / 1000)
#else
#define GLOSSY_DURATION         (RTIMER_SECOND / 150)  // / 60
#define GLOSSY_GUARD_TIME       (RTIMER_SECOND / 1900) // / 1000
#endif /* COOJA */
#define GLOSSY_GAP              (RTIMER_SECOND / 200)  // / 100
#define GLOSSY_N                3
#define GLOSSY_BOOTSTRAP_PERIODS 3
// Macros useful for managing Glossy timing
#define TIME_TO_GLOSSY          (rtimer_time_to_expire())
#define TIME_FROM_GLOSSY        (GLOSSY_PERIOD + period_skew - TIME_TO_GLOSSY + ((rtimer_clock_t)(GLOSSY_REFERENCE_TIME + GLOSSY_PERIOD + period_skew) - TACCR0))
#define GLOSSY_IS_SYNCED()      (is_t_ref_l_updated())
#define GLOSSY_REFERENCE_TIME   (get_t_ref_l())
#define GLOSSY_LAST_PERIOD      (get_last_period_l())
#define GLOSSY_CONFIG_LEN       (sizeof(glossy_config_struct))
#define GLOSSY_REPORT_LEN       (sizeof(glossy_report_struct))
#if MAC_PROTOCOL == XMAC
#define SLACK_BEFORE_GLOSSY ((int) (((get_xmac_config()->off_time + 2*get_xmac_config()->on_time)/(RTIMER_SECOND/CLOCK_SECOND)) + 2))
#define SLACK_AFTER_GLOSSY  ((int) 2)
#elif MAC_PROTOCOL == LPP_NEW
#define SLACK_BEFORE_GLOSSY ((int) (((get_lpp_new_config()->tx_timeout)/(RTIMER_SECOND/CLOCK_SECOND)) + 2))
#define SLACK_AFTER_GLOSSY  ((int) 2)
#endif /* MAC_PROTOCOL */
#else
#define WITH_FTSP 1
#define TRICKLE_ON 1
#define FTSP_RATE 60
#endif /* GLOSSY */

#define PACKETBUF_CONF_ATTRS_INLINE 1

#define QUEUEBUF_CONF_NUM          22

#define IEEE802154_CONF_PANID       0xABCD

#define SHELL_VARS_CONF_RAM_BEGIN 0x1100
#define SHELL_VARS_CONF_RAM_END 0x2000

/* DCO speed resynchronization for more robust UART, etc. */
#define DCOSYNCH_CONF_ENABLED 0
#define DCOSYNCH_CONF_PERIOD 30

#ifndef WITH_UIP6
#define TIMESYNCH_CONF_ENABLED 0
#define CC2420_CONF_TIMESTAMPS 0
#define CC2420_CONF_CHECKSUM   0
#define RIME_CONF_NO_POLITE_ANNOUCEMENTS 0
#else
#define RIME_CONF_NO_POLITE_ANNOUCEMENTS 1
#endif /* !WITH_UIP6 */

#define CFS_CONF_OFFSET_TYPE	long

#define PROFILE_CONF_ON 0
#define ENERGEST_CONF_ON 1

#define HAVE_STDINT_H
#define MSP430_MEMCPY_WORKAROUND 1
#include "msp430def.h"

/* ETHZ and SICS specific settings */
#define ETHZ 0
#define SICS 1
#define LOCATION ETHZ

/* ID of the sink, depending on location */
#if LOCATION == ETHZ
#define SINK_ID 200
#elif LOCATION == SICS
#define SINK_ID 200
#endif /* LOCATION */

/* wireless channel, depending on location */
#ifndef RF_CHANNEL
#if LOCATION == ETHZ
  #define RF_CHANNEL              26
#elif LOCATION == SICS
  #define RF_CHANNEL              23
#endif /* LOCATION */
#endif /* RF_CHANNEL */

#define ELFLOADER_CONF_TEXT_IN_ROM 0
#define ELFLOADER_CONF_DATAMEMORY_SIZE 0x400
#define ELFLOADER_CONF_TEXTMEMORY_SIZE 0x800

#define IRQ_PORT1 0x01
#define IRQ_PORT2 0x02
#define IRQ_ADC   0x03

#define CCIF
#define CLIF

#define CC_CONF_INLINE inline

#define AODV_COMPLIANCE
#define AODV_NUM_RT_ENTRIES 32

#define TMOTE_SKY 1
#define WITH_ASCII 1

#define PROCESS_CONF_NUMEVENTS 8
#define PROCESS_CONF_STATS 1
/*#define PROCESS_CONF_FASTPOLL    4*/

/* CPU target speed in Hz */
#if GLOSSY && !COOJA
#define F_CPU 4194304uL /*2457600uL*/
#else
#define F_CPU 3900000uL /*2457600uL*/
#endif /* GLOSSY && !COOJA  */

/* Our clock resolution, this is the same as Unix HZ. */
#define CLOCK_CONF_SECOND 128

#define BAUD2UBR(baud) ((F_CPU/baud))

#ifdef WITH_UIP6

#define RIMEADDR_CONF_SIZE              8

#define UIP_CONF_LL_802154              1
#define UIP_CONF_LLH_LEN                0

#ifndef UIP_CONF_ROUTER
#define UIP_CONF_ROUTER			0
#endif

#define UIP_CONF_IPV6                   1
#define UIP_CONF_IPV6_QUEUE_PKT         1
#define UIP_CONF_IPV6_CHECKS            1
#define UIP_CONF_IPV6_REASSEMBLY        0
#define UIP_CONF_NETIF_MAX_ADDRESSES    3
#define UIP_CONF_ND6_MAX_PREFIXES       3
#define UIP_CONF_ND6_MAX_NEIGHBORS      4
#define UIP_CONF_ND6_MAX_DEFROUTERS     2
#define UIP_CONF_IP_FORWARD             0
#define UIP_CONF_BUFFER_SIZE		256

#define SICSLOWPAN_CONF_COMPRESSION_IPV6        0
#define SICSLOWPAN_CONF_COMPRESSION_HC1         1
#define SICSLOWPAN_CONF_COMPRESSION_HC01        2
#define SICSLOWPAN_CONF_COMPRESSION             SICSLOWPAN_CONF_COMPRESSION_HC01
#define SICSLOWPAN_CONF_FRAG                    0
#define SICSLOWPAN_CONF_CONVENTIONAL_MAC	1
#define SICSLOWPAN_CONF_MAX_ADDR_CONTEXTS       2
#else
#define UIP_CONF_IP_FORWARD      1
#define UIP_CONF_BUFFER_SIZE     108
#endif /* WITH_UIP6 */

#define UIP_CONF_ICMP_DEST_UNREACH 1

#define UIP_CONF_DHCP_LIGHT
#define UIP_CONF_LLH_LEN         0
#define UIP_CONF_RECEIVE_WINDOW  60
#define UIP_CONF_TCP_MSS         60
#define UIP_CONF_MAX_CONNECTIONS 4
#define UIP_CONF_MAX_LISTENPORTS 8
#define UIP_CONF_UDP_CONNS       12
#define UIP_CONF_FWCACHE_SIZE    30
#define UIP_CONF_BROADCAST       1
#define UIP_ARCH_IPCHKSUM        1
#define UIP_CONF_UDP             1
#define UIP_CONF_UDP_CHECKSUMS   1
#define UIP_CONF_PINGADDRCONF    0
#define UIP_CONF_LOGGING         0

#define UIP_CONF_TCP_SPLIT       0

/*
 * Definitions below are dictated by the hardware and not really
 * changeable!
 */

/* LED ports */
#define LEDS_PxDIR P5DIR
#define LEDS_PxOUT P5OUT
#define LEDS_CONF_RED    0x10
#define LEDS_CONF_GREEN  0x20
#define LEDS_CONF_YELLOW 0x40

/* Button sensors. */
#define IRQ_PORT2 0x02

typedef unsigned short uip_stats_t;
typedef unsigned short clock_time_t;

typedef unsigned long off_t;
#define ROM_ERASE_UNIT_SIZE  512
#define XMEM_ERASE_UNIT_SIZE (64*1024L)

/* Use the first 64k of external flash for node configuration */
#define NODE_ID_XMEM_OFFSET     (0 * XMEM_ERASE_UNIT_SIZE)

/* Use the second 64k of external flash for codeprop. */
#define EEPROMFS_ADDR_CODEPROP  (1 * XMEM_ERASE_UNIT_SIZE)

#define CFS_XMEM_CONF_OFFSET    (2 * XMEM_ERASE_UNIT_SIZE)
#define CFS_XMEM_CONF_SIZE      (1 * XMEM_ERASE_UNIT_SIZE)

#define CFS_RAM_CONF_SIZE 4096

/*
 * SPI bus configuration for the TMote Sky.
 */

/* SPI input/output registers. */
#define SPI_TXBUF U0TXBUF
#define SPI_RXBUF U0RXBUF

				/* USART0 Tx ready? */
#define	SPI_WAITFOREOTx() while ((U0TCTL & TXEPT) == 0)
				/* USART0 Rx ready? */
#define	SPI_WAITFOREORx() while ((IFG1 & URXIFG0) == 0)
				/* USART0 Tx buffer ready? */
#define SPI_WAITFORTxREADY() while ((IFG1 & UTXIFG0) == 0)

#define SCK            1  /* P3.1 - Output: SPI Serial Clock (SCLK) */
#define MOSI           2  /* P3.2 - Output: SPI Master out - slave in (MOSI) */
#define MISO           3  /* P3.3 - Input:  SPI Master in - slave out (MISO) */

/*
 * SPI bus - M25P80 external flash configuration.
 */

#define FLASH_PWR	3	/* P4.3 Output */
#define FLASH_CS	4	/* P4.4 Output */
#define FLASH_HOLD	7	/* P4.7 Output */

/* Enable/disable flash access to the SPI bus (active low). */

#define SPI_FLASH_ENABLE()  ( P4OUT &= ~BV(FLASH_CS) )
#define SPI_FLASH_DISABLE() ( P4OUT |=  BV(FLASH_CS) )

#define SPI_FLASH_HOLD()		( P4OUT &= ~BV(FLASH_HOLD) )
#define SPI_FLASH_UNHOLD()		( P4OUT |=  BV(FLASH_HOLD) )

/*
 * SPI bus - CC2420 pin configuration.
 */

#define FIFO_P         0  /* P1.0 - Input: FIFOP from CC2420 */
#define FIFO           3  /* P1.3 - Input: FIFO from CC2420 */
#define CCA            4  /* P1.4 - Input: CCA from CC2420 */

#define SFD            1  /* P4.1 - Input:  SFD from CC2420 */
#define CSN            2  /* P4.2 - Output: SPI Chip Select (CS_N) */
#define VREG_EN        5  /* P4.5 - Output: VREG_EN to CC2420 */
#define RESET_N        6  /* P4.6 - Output: RESET_N to CC2420 */

/* Pin status. */

#define FIFO_IS_1       (!!(P1IN & BV(FIFO)))
#define CCA_IS_1        (!!(P1IN & BV(CCA) ))
#define RESET_IS_1      (!!(P4IN & BV(RESET_N)))
#define VREG_IS_1       (!!(P4IN & BV(VREG_EN)))
#define FIFOP_IS_1      (!!(P1IN & BV(FIFO_P)))
#define SFD_IS_1        (!!(P4IN & BV(SFD)))

/* The CC2420 reset pin. */
#define SET_RESET_INACTIVE()    ( P4OUT |=  BV(RESET_N) )
#define SET_RESET_ACTIVE()      ( P4OUT &= ~BV(RESET_N) )

/* CC2420 voltage regulator enable pin. */
#define SET_VREG_ACTIVE()       ( P4OUT |=  BV(VREG_EN) )
#define SET_VREG_INACTIVE()     ( P4OUT &= ~BV(VREG_EN) )

/* CC2420 rising edge trigger for external interrupt 0 (FIFOP). */
#define FIFOP_INT_INIT() do {\
  P1IES &= ~BV(FIFO_P);\
  CLEAR_FIFOP_INT();\
} while (0)

/* FIFOP on external interrupt 0. */
#define ENABLE_FIFOP_INT()          do { P1IE |= BV(FIFO_P); } while (0)
#define DISABLE_FIFOP_INT()         do { P1IE &= ~BV(FIFO_P); } while (0)
#define CLEAR_FIFOP_INT()           do { P1IFG &= ~BV(FIFO_P); } while (0)

/* Enables/disables CC2420 access to the SPI bus (not the bus).
 *
 * These guys should really be renamed but are compatible with the
 * original Chipcon naming.
 *
 * SPI_CC2420_ENABLE/SPI_CC2420_DISABLE???
 * CC2420_ENABLE_SPI/CC2420_DISABLE_SPI???
 */

#include <signal.h>
#define SPI_ENABLE()    do { P4OUT &= ~BV(CSN); } while (0) /* ENABLE CSn (active low) */
#define SPI_DISABLE()   do { P4OUT |=  BV(CSN); } while (0) /* DISABLE CSn (active low) */

#ifdef PROJECT_CONF_H
#include PROJECT_CONF_H
#endif /* PROJECT_CONF_H */



#endif /* CONTIKI_CONF_H */

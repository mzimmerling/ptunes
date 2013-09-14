/*
 * ftsp.c
 *
 *  Created on: Feb 23, 2010
 *      Author: Federico Ferrari
 */

#include "net/rime/ftsp.h"
#include "net/rime.h"
#include "node-id.h"
#include "dev/cc2420.h"
#include "dev/leds.h"
#include "random.h"
#include <stdio.h>

#if WITH_FTSP

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#if CONTIKI_TARGET_NETSIM
#include "ether.h"
#endif

static const struct packetbuf_attrlist attributes[] =
  {
    FTSP_ATTRIBUTES PACKETBUF_ATTR_LAST
  };

static table_item 				table[MAX_ENTRIES];
static uint8_t 					state, mode, num_entries, table_entries, heart_beats, num_errors, seq_num;
static float 					skew;
static rtimer_long_clock_t 		local_average, age, oldest_time, approx_local_time;
static long						offset_average, time_error, a, b;
static struct ftsp_message 		msg;
static struct ftsp_message_rcv 	msg_rcv;
static struct etimer			et, et_process_msg;
static process_event_t			msg_ready;
static process_event_t			send_ready;
static int8_t					i, free_item, oldest_item;
static long long				local_sum, offset_sum;

/*---------------------------------------------------------------------------*/
PROCESS(ftsp_process, "FTSP");
PROCESS(ftsp_msg_process, "FTSP Message Process");
PROCESS(ftsp_send_process, "FTSP Send Process");
/*---------------------------------------------------------------------------*/

rtimer_long_clock_t get_global_time(void)
{
	return local_to_global(get_local_time());
}

int is_synced(void)
{
	if ((num_entries >= ENTRY_VALID_LIMIT) || (msg.root_id == node_id))
		return FTSP_OK;
	else
		return FTSP_ERR;
}

rtimer_long_clock_t local_to_global(rtimer_long_clock_t time)
{
	approx_local_time = time + offset_average;
	return (approx_local_time +
		(long)(skew * (long)(approx_local_time - local_average)));
}
	
rtimer_long_clock_t global_to_local(rtimer_long_clock_t time)
{
	approx_local_time = time - offset_average;
	return (approx_local_time -
		(long)(skew * (long)(approx_local_time - local_average)));
}

void calculate_conversion(void)
{
	for(i=0; (i < MAX_ENTRIES) && (table[i].state != ENTRY_FULL); ++i);
	
	if (i >= MAX_ENTRIES) return;	// table is empty
	
	local_average = table[i].local_time;
	offset_average = table[i].time_offset;
	
	local_sum = 0;
	offset_sum = 0;
	
	while (++i < MAX_ENTRIES)
	{
		if (table[i].state == ENTRY_FULL)
		{
			local_sum +=
				(long)(table[i].local_time - local_average) / table_entries;
			offset_sum +=
				(long)(table[i].time_offset - offset_average) / table_entries;
		}
	}
	
	local_average += local_sum;
	offset_average += offset_sum;
	
	local_sum = offset_sum = 0;
	for (i=0; i < MAX_ENTRIES; ++i)
	{
		if (table[i].state == ENTRY_FULL)
		{
			a = table[i].local_time - local_average;
			b = table[i].time_offset - offset_average;
			
			local_sum += (long long)a * a;
			offset_sum += (long long)a * b;
		}
	}
	
	if (local_sum != 0)
		skew = (float)offset_sum / (float)local_sum;

	num_entries = table_entries;
	
	PRINTF("FTSP conversion calculated: num_entries=%u, is_synced=%u\n",
	       num_entries, is_synced());
}	

void clear_table(void)
{
	for (i=0; i < MAX_ENTRIES; ++i)
		table[i].state = ENTRY_EMPTY;
	
	num_entries = 0;
}

void add_new_entry(void)
{
	free_item = -1;
	oldest_item = 0;
	age = 0;
	oldest_time = 0;
	
	table_entries = 0;
	
	time_error = (long)(local_to_global(msg_rcv.local_time)
			- msg_rcv.global_time);
	if (is_synced() == FTSP_OK)
	{
		PRINTF("FTSP synced, error %ld\n", time_error);
		if ((time_error > ENTRY_THROWOUT_LIMIT) ||
				(-time_error > ENTRY_THROWOUT_LIMIT))
		{
			PRINTF("(big)\n");
			if (++num_errors > 3)
			{
				PRINTF("FTSP: num_errors > 3 => clear_table()\n");
				clear_table();
			}
		}
		else
		{
			PRINTF("(small)\n");
			num_errors = 0;
		}
	}
	else
	{
		PRINTF("FTSP not synced\n");
	}
	
	for (i=0; i < MAX_ENTRIES; ++i)
	{
		age = msg_rcv.local_time - table[i].local_time;
		
		if (age >= 0x7FFFFFFFL)
			table[i].state = ENTRY_EMPTY;
		
		if (table[i].state == ENTRY_EMPTY)
			free_item = i;
		else
			++table_entries;
		
		if (age >= oldest_time)
		{
			oldest_time = age;
			oldest_item = i;
		}
	}
	
	if (free_item < 0)
		free_item = oldest_item;
	else
		++table_entries;
	
	table[free_item].state = ENTRY_FULL;
	table[free_item].local_time = msg_rcv.local_time;
	table[free_item].time_offset = msg_rcv.global_time - msg_rcv.local_time;
}

void process_msg(void)
{
	if ((msg_rcv.root_id < msg.root_id) &&
		~((heart_beats < IGNORE_ROOT_MSG) && (msg.root_id == node_id)))
	{
		msg.root_id = msg_rcv.root_id;
		seq_num = msg_rcv.seq_num;
	}
	else
	{
		if ((msg.root_id == msg_rcv.root_id) &&
			((int8_t)(msg_rcv.seq_num - seq_num) > 0))
		{
			seq_num = msg_rcv.seq_num;
		}
		else
		{
			goto exit;
		}
	}
	
	if (msg.root_id < node_id)
		heart_beats = 0;
	
	add_new_entry();
	calculate_conversion();

//	process_post(PROCESS_BROADCAST, ftsp_table_updated, (process_data_t)NULL);

	leds_blink();
	
exit:
	state &= ~STATE_PROCESSING;
}

void ftsp_init(void)
{
	msg.node_id = node_id;
	if (node_id == SINK_ID) {
		msg.root_id = node_id;
	} else {
		msg.root_id = 0xFFFF;
	}

	skew = 0.0;
	local_average = 0;
	offset_average = 0;
	clear_table();
	state = STATE_INIT;
	mode = TS_TIMER_MODE;
	heart_beats = 0;
	num_errors = 0;
	
	process_start(&ftsp_process, NULL);
	process_start(&ftsp_msg_process, NULL);
	process_start(&ftsp_send_process, NULL);
}

void msg_send(void)
{
//	if ((msg.root_id == 0xFFFF) && (++heart_beats >= ROOT_TIMEOUT))
//	{
//		PRINTF("FTSP root timeout, declaring myself the root\n");
//		seq_num = 0;
//		msg.root_id = node_id;
//	}
//
	if ((msg.root_id != 0xFFFF) && ((state & STATE_SENDING) == 0))
	{
		state |= STATE_SENDING;
		rtimer_long_clock_t local_time, global_time;
		local_time = get_local_time();
		global_time = local_to_global(local_time);
		
		if (msg.root_id == node_id)
		{
			if ((long)(local_time - local_average) >= 0x20000000)
			{
				local_average = local_time;
				offset_average = global_time - local_time;
			}
		}
//		else
//		{
//			if (heart_beats >= ROOT_TIMEOUT)
//			{
//				PRINTF("FTSP: root timeout, declaring myself the root\n");
//				heart_beats = 0;
//				msg.root_id = node_id;
//				++seq_num;
//			}
//		}
		
		if ((num_entries < ENTRY_SEND_LIMIT) && (msg.root_id != node_id))
		{
			++heart_beats;
			state &= ~STATE_SENDING;
		}
		else
		{
			process_post(&ftsp_send_process, send_ready, NULL);
		}
	}
}

void ftsp_send(void)
{
	if (mode == TS_USER_MODE)
		msg_send();
}

void ftsp_stop(void)
{
	etimer_stop(&et);
}

uint8_t ftsp_get_mode(void)
{
	return mode;
}

void ftsp_set_mode(uint8_t mode_)
{
	if (mode_ == mode)
		return;
	
	if (mode_ == TS_TIMER_MODE)
		etimer_set(&et, BEACON_RATE * CLOCK_SECOND + random_rand() % (BEACON_RATE/2 * CLOCK_SECOND));
	else
		ftsp_stop();
	
	mode = mode_;
}

static void
ftsp_recv(struct broadcast_conn *bc, rimeaddr_t *from)
{
	packetbuf_copyto(&msg_rcv);	
	if (packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP))	
	{
		if ((state & STATE_PROCESSING) == 0)
		{
			msg_rcv.seq_num = packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID);
			msg_rcv.local_time = ftsp_arrival_time;
			state |= STATE_PROCESSING;
			// remove the TIMESTAMP attribute
			packetbuf_set_attr(PACKETBUF_ATTR_TIMESTAMP, 0);
			process_post(&ftsp_msg_process, msg_ready, NULL);
			
			PRINTF("FTSP message rcvd: node_id=%u, root_id=%u, seq_num=%u\n",
			       msg_rcv.node_id, msg_rcv.root_id, msg_rcv.seq_num);
			PRINTF("                   global_time=%lu, local_time=%lu\n",
			       msg_rcv.global_time, msg_rcv.local_time);
			PRINTF("                   local_to_global(local_time)=%lu, diff=%ld\n",
					local_to_global(msg_rcv.local_time),
					local_to_global(msg_rcv.local_time) - msg_rcv.global_time);
		}
		else
		{
			PRINTF("FTSP message from %u ignored, still processing\n",
			       msg_rcv.node_id);
		}
	}
	else
	{
		PRINTF("FTSP message from %u with timestamp attribute %d ignored\n",
		       msg_rcv.node_id, packetbuf_attr(PACKETBUF_ATTR_TIMESTAMP));
	}
}

const static struct broadcast_callbacks ftsp_call = { ftsp_recv };
static struct broadcast_conn ftsp_conn;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ftsp_process, ev, data)
{

	PROCESS_EXITHANDLER(broadcast_close(&ftsp_conn);)

	PROCESS_BEGIN();

	PRINTF("Node id %d\n", node_id);

	broadcast_open(&ftsp_conn, FTSP_CHANNEL, &ftsp_call);
	channel_set_attributes(FTSP_CHANNEL, attributes);

	etimer_set(&et, BEACON_RATE * CLOCK_SECOND + random_rand() % (BEACON_RATE * CLOCK_SECOND));
  
	while (1)
	{
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
		
		if (mode == TS_TIMER_MODE)
			msg_send();
		else
			ftsp_stop();

		etimer_reset(&et);
	}
	
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ftsp_msg_process, ev, data)
{
	PROCESS_BEGIN();
	
	while(1)
	{
		PROCESS_WAIT_EVENT_UNTIL(ev == msg_ready);
		// delay the message processing by e.g. 15 ms:
		// less likely to interfere with a duty-cycled MAC protocol
		etimer_set(&et_process_msg, CLOCK_SECOND / 64);
		PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et_process_msg));
		// disable interrupts while we process the received message:
		// avoid errors due to interrupts during the processing
		dint();
		process_msg();
		eint();
	}
	
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(ftsp_send_process, ev, data)
{
	PROCESS_BEGIN();
	
	ftsp_msg_sent = process_alloc_event();
	ftsp_table_updated = process_alloc_event();

	while(1)
	{
		PROCESS_WAIT_EVENT_UNTIL(ev == send_ready);
		
		packetbuf_copyfrom(&msg, sizeof(struct ftsp_message));
		packetbuf_set_attr(PACKETBUF_ATTR_TIMESTAMP, 1);
		packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, seq_num);
		
		if (broadcast_send(&ftsp_conn))
		{
			PRINTF("FTSP message sent: node_id=%u, root_id=%u, seq_num=%u\n",
			       msg.node_id, msg.root_id, seq_num);

//			process_post(PROCESS_BROADCAST, ftsp_msg_sent, (process_data_t)NULL);

			++heart_beats;
			
			if (msg.root_id == node_id)
				++seq_num;
		}
		else
		{
			PRINTF("FTSP error sending message: node_id=%u, root_id=%u, seq_num=%u\n",
			       msg.node_id, msg.root_id, seq_num);
		}
 		state &= ~STATE_SENDING;
	}
	
	PROCESS_END();
}

#endif /* WITH_FTSP */

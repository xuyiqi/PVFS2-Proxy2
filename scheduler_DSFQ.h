/*
 * scheduler_DSFQ.h
 *
 *  Created on: Dec 13, 2010
 *      Author: yiqi
 */

#ifndef SCHEDULER_DSFQ_H_
#define SCHEDULER_DSFQ_H_
#define __STATIC_SCHEDULER_DSFQ__ 1
#include "llist.h"
#include "scheduler_main.h"
#define DSFQ_SCHEDULER "DSFQ"
#define DEFAULT_DSFQ_PROXY_PORT 3336
extern int dsfq_depth;
extern int dsfq_num_apps;
struct proxy_message
{
	//int msg_index;		//message sequence number
	int app_index;		//application identifier
	int period_count;	//periodical sum of bytes transfered for this application in the last period
};

struct proxy_queue
{
	PINT_llist_p messages;
	int size;
};

struct dsfq_statistics
{
	int request_receive_queue;
	int dsfq_delay_values;
	int dsfq_delay_value_bitmap;
	int dsfq_last_applied_delay;
	int dsfq_last_applied_item_ids;
	int dsfq_app_dispatch;
};



int list_pmsg_print_all(void * item);

extern struct heap *dsfq_heap_queue;
struct dsfq_queue_item
{
	int request_socket;
	int start_tag;
	int finish_tag;
	int delay_value;
	int weight;
	int last_finish_tag;
	int virtual_time;
	int delay_applied;
	int data_port; //internal forwarding parameter
	char* data_ip;
	int data_socket;
	int socket_tag; //internal forwarding parameter (tag used by pvfs)
	int app_index;//differentiates streams, now differentiated by ip
	int stream_id;//for identifying previously finished time.
	int task_size;
	int complete_size;
	int got_size;
	char* buffer; //request message buffer
	int io_type;
	int buffer_size;
	int unlock_index;

};

struct generic_queue_item * dsfq_dequeue(struct dequeue_reason r);
#endif /* SCHEDULER_DSFQ_H_ */

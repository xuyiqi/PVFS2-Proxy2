/*
 * scheduler_vDSFQ.h
 *
 *  Created on: Dec 13, 2010
 *      Author: yiqi
 */

#ifndef SCHEDULER_VDSFQ2_H_
#define SCHEDULER_VDSFQ2_H_
#define __STATIC_SCHEDULER_VDSFQ2__ 1
#include "llist.h"
#include "scheduler_main.h"
#define VDSFQ2_SCHEDULER "VDSFQ2"
#define DEFAULT_VDSFQ2_PROXY_PORT 3336
extern int vdsfq2_depth;
extern int vdsfq2_num_apps;

struct vproxy_message2
{
	//int msg_index;		//message sequence number
	int app_index;		//application identifier
	int period_count;	//periodical sum of bytes transfered for this application in the last period
};

struct vproxy_queue2
{
	PINT_llist_p messages;
	int size;
};


struct vdsfq2_statistics
{
	int request_receive_queue;
	int vdsfq_delay_values;
	int vdsfq_delay_value_bitmap;
	int vdsfq_last_applied_delay;
	int vdsfq_last_applied_item_ids;
	int vdsfq_app_dispatch;
};



int list_pmsg_print_all(void * item);

extern struct heap *vdsfq2_heap_queue;

struct vdsfq2_queue_item
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

struct generic_queue_item * vdsfq2_dequeue(struct dequeue_reason r);
#endif /* SCHEDULER_vDSFQ_H_ */

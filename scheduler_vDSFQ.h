/* vPFS: Virtualized Parallel File System:
 * Performance Virtualization of Parallel File Systems

 * Copyright (C) 2009-2012 Yiqi Xu Florida International University
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * See COPYING in top-level directory.
 */

#ifndef SCHEDULER_VDSFQ_H_
#define SCHEDULER_VDSFQ_H_
#define __STATIC_SCHEDULER_VDSFQ__ 1
#include "llist.h"
#include "scheduler_main.h"
#define VDSFQ_SCHEDULER "VDSFQ"
#define DEFAULT_VDSFQ_PROXY_PORT 3336
extern int vdsfq_depth;
extern int vdsfq_num_apps;

struct vproxy_message
{
	//int msg_index;		//message sequence number
	int app_index;		//application identifier
	int period_count;	//periodical sum of bytes transfered for this application in the last period
};

struct vproxy_queue
{
	PINT_llist_p messages;
	int size;
};


struct vdsfq_statistics
{
	int request_receive_queue;
	int vdsfq_delay_values;
	int vdsfq_delay_value_bitmap;
	int vdsfq_last_applied_delay;
	int vdsfq_last_applied_item_ids;
	int vdsfq_app_dispatch;
};



int list_pmsg_print_all(void * item);

extern struct heap *vdsfq_heap_queue;

struct vdsfq_queue_item
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

struct generic_queue_item * vdsfq_dequeue(struct dequeue_reason r);
#endif /* SCHEDULER_vDSFQ_H_ */

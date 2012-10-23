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

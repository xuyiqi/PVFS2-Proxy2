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

#ifndef SCHEDULER_SFQD2_H_
#define SCHEDULER_SFQD2_H_
#include "heap.h"
#include "scheduler_main.h"
#define SFQD2_SCHEDULER "SFQD2"
#define __STATIC_SCHEDULER_SFQD2__ 1
/* trial of a variation of local SFQD who uses different queues for small I/Os versus large I/Os */
extern char** app_names;
extern int sfqd2_depth;
extern int sfqd2_purecost;
struct sfqd2_queue_item
{
	int request_socket;
	int start_tag;
	int finish_tag;
	int data_port; //internal forwarding parameter
	char* data_ip;
	int data_socket;
	int request_socket_index;
	int data_file_size;
	int strip_size;
	int server_nr;
	long long file_offset;
	long long aggregate_size;
	int server_count;
	int data_socket_index;
	int socket_tag; //internal forwarding parameter (tag used by pvfs)
	int app_index;//differentiates streams, now differentiated by ip
	int stream_id;//for identifying previously finished time.
	int task_size;
	int over_count;
	int complete_size;
	int got_size;
	char* buffer; //request message buffer
	int io_type;
	int buffer_size;
	int unlock_index;

};

extern FILE* sfqd2_depthtrack;
extern char* config_s;
extern int* weights;
extern int num_apps;
extern int depth;
extern int purecost;
//void initialize_hashtable();
int sfqd2_packet_cmp(struct heap_node* _a, struct heap_node* _b);
extern struct heap *sfqd2_heap_queue;
struct generic_queue_item* sfqd2_get_next_request(struct dequeue_reason r);
extern int sfqd2_virtual_time;
int sfqd2_add_request(int r_socket_index,int d_socket_index, int length,
		int tag, int io_type, char* request, int request_size);
int sfqd2_enqueue(struct socket_info * si, struct pvfs_info* pi);
struct generic_queue_item * sfqd2_dequeue(struct dequeue_reason r);
void sfqd2_get_scheduler_info();
int sfqd2_update_on_request_completion(void* arg);
int sfqd2_load_data_from_config (dictionary * dict);
int sfqd2_init();
int sfqd2_is_idle();
int sfqd2_current_size(struct request_state * original_rs, long long actual_data_file_size);

extern int current_depth_large;
extern int current_depth_small;
extern char* clients[];
extern int client_app[];
extern int* apps;
extern int default_weight;
extern int* sfqd2_stream_ids;
extern int* sfqd2_last_finish_tags;
#endif /* SCHEDULER_SFQD2_H_ */

/*
 * scheduler.h
 *
 *  Created on: May 20, 2010
 *      Author: yiqi
 */

#ifndef SCHEDULER_SFQD3_H_
#define SCHEDULER_SFQD3_H_
#include "heap.h"
#include "scheduler_main.h"
#define SFQD3_SCHEDULER "SFQD3"
#define __STATIC_SCHEDULER_SFQD3__ 1
/* local SFQD who does throttling (non-work-conserving) */
extern char** app_names;
extern int sfqd3_depth;
extern int sfqd3_purecost;
struct sfqd3_queue_item
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

extern FILE* sfqd3_depthtrack;
extern char* config_s;
extern int* weights;
extern int num_apps;
extern int depth;
extern int purecost;
//void initialize_hashtable();
int sfqd3_packet_cmp(struct heap_node* _a, struct heap_node* _b);
extern struct heap *sfqd3_heap_queue;
struct generic_queue_item* sfqd3_get_next_request(struct dequeue_reason r);
extern int sfqd3_virtual_time;
int sfqd3_add_request(int r_socket_index,int d_socket_index, int length,
		int tag, int io_type, char* request, int request_size);
int sfqd3_enqueue(struct socket_info * si, struct pvfs_info* pi);
struct generic_queue_item * sfqd3_dequeue(struct dequeue_reason r);
void sfqd3_get_scheduler_info();
int sfqd3_update_on_request_completion(void* arg);
int sfqd3_load_data_from_config (dictionary * dict);
int sfqd3_init();
int sfqd3_is_idle();
int sfqd3_current_size(struct request_state * original_rs, long long actual_data_file_size);

extern int current_depth_large;
extern int current_depth_small;
extern char* clients[];
extern int client_app[];
extern int* apps;
extern int default_weight;
extern int* sfqd3_stream_ids;
extern int* sfqd3_last_finish_tags;
#endif /* SCHEDULER_SFQD2_H_ */

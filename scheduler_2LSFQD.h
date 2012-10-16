/*
 * scheduler.h
 *
 *  Created on: May 20, 2010
 *      Author: yiqi
 */

#ifndef SCHEDULER_2LSFQD_H_
#define SCHEDULER_2LSFQD_H_
#include "heap.h"
#include "scheduler_main.h"
#define TWOLSFQD_SCHEDULER "TWOLSFQD"
#define __STATIC_SCHEDULER_2LSFQD__ 1

extern char** app_names;
extern int twolsfqd_depth;
extern int twolsfqd_purecost;
struct resp_time
{
	int resp_time;
};
struct twolsfqd_queue_item
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
	struct timeval deadline;	//for edf queue
	struct timeval queuedtime;	//for waiting time calculation
	struct timeval dispatchtime; // for response time feedback
	int deadline_in_current_timewindow;
	int dispatched;
	int missed;
	int miss_start_timewindow;
	int miss_end_timewindow;
	int timewindow;
	int deadline_same_timewindow;
};



extern char* config_s;
extern int* weights;
extern int num_apps;

extern int purecost;
//void initialize_hashtable();

int twolsfqd_edf_complete(void * arg);
void* twolsfqd_time_replenish(void * arg);
void* twolsfqd_time_window(void * arg);
int twolsfqd_packet_cmp(struct heap_node* _a, struct heap_node* _b);
extern struct heap *twolsfqd_heap_queue;
struct generic_queue_item* twolsfqd_get_next_request(struct dequeue_reason r);
extern int twolsfqd_virtual_time;
int twolsfqd_add_request(int r_socket_index,int d_socket_index, int length,
		int tag, int io_type, char* request, int request_size);
struct generic_queue_item* twolsfqd_edf_dequeue();
int twolsfqd_enqueue(struct socket_info * si, struct pvfs_info* pi);
struct generic_queue_item * twolsfqd_dequeue();
void twolsfqd_get_scheduler_info();
int twolsfqd_update_on_request_completion(void* arg);
int twolsfqd_load_data_from_config (dictionary * dict);
int twolsfqd_init();
int twolsfqd_is_idle();
int twolsfqd_edf_dequeue_all(int enqueued_socket);
int twolsfqd_current_size(struct request_state * original_rs, long long actual_data_file_size);
void twolsfqd_update_spareness();
extern int twolsfqd_virtual_time;
extern int twolsfqd_current_depth;
extern char* clients[];
extern int client_app[];
extern int* apps;
extern int default_weight;
extern int* stream_ids;
extern int* twolsfqd_last_finish_tags;
#endif

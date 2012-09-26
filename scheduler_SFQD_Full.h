/*
 * scheduler.h
 *
 *  Created on: May 20, 2010
 *      Author: yiqi
 */

#ifndef SCHEDULER_SFQD_FULL_H_
#define SCHEDULER_SFQD_FULL_H_
#include "heap.h"
#include "scheduler_main.h"
#include "config.h"
#define SFQD_FULL_SCHEDULER "SFQD_FULL"
#define __STATIC_SCHEDULER_SFQD_FULL_ 1

extern char** app_names;
extern int sfqdfull_depth;
extern int sfqdfull_purecost;
struct sfqdfull_queue_item
{
	int request_socket;
	int start_tag;
	int finish_tag;
	int data_port; //internal forwarding parameter
	char* data_ip;
	int data_socket;
	int request_port;
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



};

extern FILE* depthtrack;
extern char* config_s;
extern int* weights;
extern int num_apps;
extern int depth;
extern int purecost;
//void initialize_hashtable();
int sfqdfull_packet_cmp(struct heap_node* _a, struct heap_node* _b);
extern struct heap *sfqdfull_heap_queue;
struct generic_queue_item* sfqdfull_get_next_request(struct dequeue_reason r);
extern int sfqdfull_virtual_time;
int sfqdfull_add_request(int r_socket_index,int d_socket_index, int length,
		int tag, int io_type, char* request, int request_size);
int set_start_end(char* range, char *splitter, struct ip_application* ip_range);
int set_IP_numbers(char* ip_0, short* i1,short* i2, short* i3, short* i4);
int sfqdfull_enqueue(struct socket_info * si, struct pvfs_info* pi);
struct generic_queue_item * sfqdfull_dequeue(struct dequeue_reason r);
void sfqdfull_get_scheduler_info();
int sfqdfull_update_on_request_completion(void* arg);
int sfqdfull_load_data_from_config (dictionary * dict);
int sfqdfull_init();
int sfqdfull_is_idle();
int sfqdfull_current_size();
extern int virtual_time;
extern int sfqdfull_current_depth;
extern char* clients[];
extern int client_app[];
extern int* apps;
extern int default_weight;
extern int* sfqdfull_stream_ids;
extern int* sfqdfull_last_finish_tags;
#endif /* SCHEDULER_H_ */

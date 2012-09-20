/*
 * scheduler.h
 *
 *  Created on: May 20, 2010
 *      Author: yiqi
 */

#ifndef SCHEDULER_IEDF_H_
#define SCHEDULER_IEDF_H_
#include "heap.h"
#include "scheduler_main.h"
#define IEDF_SCHEDULER "SFQD"
#define __STATIC_SCHEDULER_IDF__ 1
enum range_type{IP,RANGE,HOSTNAME};
extern char** app_names;
extern int iedf_depth;
extern int iedf_purecost;
struct iedf_queue_item
{
	struct twolsfqd_queue_item;//from higher level
	struct time_val queue_time;
};


extern FILE* iedf_depthtrack;
extern char* config_s;
extern int* weights;
extern int num_apps;
extern int* iedf_deadlines;
extern int iedf_depth;
extern int iedf_purecost;
//void initialize_hashtable();

int iedf_enqueue(struct socket_info * si, struct pvfs_info* pi);

struct generic_queue_item * sfqd_dequeue(struct dequeue_reason r);

void iedf_get_scheduler_info();
int iedf_update_on_request_completion(void* arg);
int iedf_load_data_from_config (dictionary * dict);
int iedf_init();
int iedf_is_idle();
int iedf_current_size();

extern int current_depth;
extern char* clients[];
extern int client_app[];
extern int* apps;
extern int default_weight;
extern int* stream_ids;

#endif /* SCHEDULER_H_ */

/*
 * scheduler.c
 *
 *  Created on: Apr 15, 2010
 *      Author: yiqi
 */




#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "sockio.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <poll.h>
#include <sys/poll.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "logging.h"
#include "proxy2.h"
#include <unistd.h>
#include "scheduler_IEDF.h"
#include "heap.h"
#include "iniparser.h"
#include "llist.h"
#include "performance.h"
#include "scheduler_main.h"
#include "config.h"
#include "cost_model_history.h"



extern struct socket_pool  s_pool;
extern char* log_prefix;
#define REDUCER 1024

PINT_llist_p iedf_llist_queue; //the edf queue, ordered by deadline from this momemnt,e.g. -1ms, -0.5ms, 1ms. 10ms...
int iedf_size;
int iedf_timewindow;//in ms
extern long long total_throughput;

FILE* iedf_depthtrack;
int* iedf_deadlines;
int iedf_item_id=0;


int print_ip_app_item(void* item);

int iedf_add_ttl_tp(int this_amount, int app_index)
{
	total_throughput+=this_amount;
    app_stats[app_index].byte_counter=app_stats[app_index].byte_counter+this_amount;
    app_stats[app_index].app_throughput=app_stats[app_index].app_throughput+this_amount;
}

const struct scheduler_method sch_iedf = {
    .method_name = IEDF_SCHEDULER,
    .work_conserving = 1,
    .sch_initialize = iedf_init,
    .sch_finalize = NULL,
    .sch_enqueue = iedf_enqueue,
    .sch_dequeue = iedf_dequeue,
    .sch_load_data_from_config = iedf_load_data_from_config,
    .sch_update_on_request_completion = iedf_update_on_request_completion,
    .sch_get_scheduler_info = iedf_get_scheduler_info,
    .sch_is_idle = iedf_is_idle,
    .sch_current_size = iedf_current_size,
    .sch_add_ttl_throughput = iedf_add_ttl_tp
};



int iedf_is_idle()
{

	return iedf_size;

}

//int 2lsfqd_current_size(int sock_index, long long actual_data_file_size)
//this function will be kept in the upper level for bytes expected adjustment
//hook should still call the higher level callback function


int iedf_enqueue(struct generic_queue_item twolsfqd_item)
{

	//create my own queueu item
	//add deadline by queuing time+ app-deadline (e.g. 200ms)
	//insert into the tail of the queue
	//sort the queue

/*	char bptr[20];
    struct timeval tv;
    time_t tp;
    gettimeofday(&tv, 0);
    tp = tv.tv_sec;
    strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
    sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);
    s_pool.socket_state_list[r_socket_index].last_work_time=tv;*/

	int dispatched=0;
	if (iedf_current_depth<iedf_depth)//heap_empty(heap_queue))
	{
		dispatched=1;
		iedf_current_depth++;
		//this means the queue is empty before adding item to it.
	}
	if (dispatched)
	{

	}
	return dispatched;


}

struct generic_queue_item * iedf_dequeue(struct dequeue_reason r)
{

	/*
	 * Called after add_request to determine if it's dispatched immediately
	 *
	 * Update last_serviced_request time
	 * */

}

void iedf_get_scheduler_info()
{
	//fprintf(stderr,"depth remains at %i", current_depth);
	//fprintf(stderr," current queue has %i items\n",heap_queue->all_count);
}

//hook should call this one- the nearest to actual completion
int iedf_update_on_request_completion(void* arg)
{


	struct complete_message * complete = (struct complete_message *)arg;

	struct generic_queue_item * current_item = (complete->current_item);

	struct twolsfqd_queue_item * twolsfqd_item = (struct twolsfqd_queue_item * )(current_item->embedded_queue_item);
	twolsfqd_item->got_size+=(complete->complete_size);

	if (twolsfqd_item->task_size==twolsfqd_item->got_size)
	{

		return sfqd_item->got_size;


	}
	else if (twolsfqd_item->task_size < twolsfqd_item->got_size)
	{
		twolsfqd_item->over_count++;
		fprintf(stderr,"error from sock %i, item %i\n",sfqd_item->request_socket_index, sfqd_item->stream_id);
		fprintf(stderr, "error...got_size %i> task_size %i (completed %i), op is %i, error_count is %i\n",sfqd_item->got_size,sfqd_item->task_size, complete->complete_size, sfqd_item->io_type,sfqd_item->over_count);

		return -1;
		//return sfqd_item->got_size;
	}
	else
	{
		//fprintf(stderr, "inprogress...got_size %i< task_size %i (completed %i), op is %i, error_count is %i\n",sfqd_item->got_size,sfqd_item->task_size, complete->complete_size, sfqd_item->io_type,sfqd_item->over_count);
		return 0;
	}
}



int iedf_load_data_from_config (dictionary * dict)
{

	iedf_depth=iniparser_getint(dict, "IEDF:depth" ,iedf_depth);
	fprintf(stderr,"Two level EDF using depth:%i\n",iedf_depth);

	iedf_timewindow=iniparser_getint(dict, "IEDF:timewindow" ,iedf_timewindow);
	fprintf(stderr,"Two level EDF using timewindow:%i\n",iedf_timewindow);


}

void dispatcher(void * arg)
{

	//thread work function to detect time window

	//signaler or waiter?

}


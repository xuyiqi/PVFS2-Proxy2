/*
 * scheduler_2LSFQD.c
 *
 *  Created on: Nov 15, 2011
 *      Author: yiqi
 */
#include <limits.h>
#include <math.h>
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
#include <unistd.h>
#include <pthread.h>
#include "logging.h"
#include "proxy2.h"
#include <unistd.h>
#include "scheduler_2LSFQD.h"
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

#define TWOL_REPLENISH_TIME 200
#define TWOL_EDF_TIME 1000
#define TWOL_EDF_INFINITE 100
#define TWOL_REPLENISH_AMOUNT 100
int twolsfqd_ttl_rate=0;
int twolsfqd_default_weight=1;
int twolsfqd_depth=1;
int twolsfqd_current_depth=30;
int twolsfqd_virtual_time=0;
int* twolsfqd_last_finish_tags;
extern long long total_throughput;
int timewindow_id=0;
int twolsfqd_missed_storage=0;
int *twolsfqd_missed_storages=0;
int twolsfqd_missed_time = 0;

int *period_cumulative_request_size;
int *period_cumulative_request_count;
double *last_app_rate;
pthread_mutex_t twolsfqd_sfqd_queue_mutex;

int default_nr=8;
/*
 * aging statistics
 * expected_time - predicted response time of an I/O in storage utility...used to calculate the time for staying in the queue
 *
 * SPECIFIC FOR EDF QUEUE ONLY
 *
 * timewindow_arrival
 * timewindow_arrival_deadline
 * timewindow_completed
 * timewindow_edf_waiting
 * timewindow_max_outstanding
 * timewindow_queue_threshold
 * timewindow_curent_queue_length
 *
 * next_timewindow_X_lower
 * next_timewindow_X_upper
 * next_timewindow_RT
 * next_timewindow_queue_threshold
 *
 * */
int queue_index=0;

int* sarc_credits;

int timewindow_arrival;
int timewindow_arrival_deadline;
int timewindow_arrival_next_deadline;
int* timewindow_arrival_deadlines;
int* timewindow_previous_arrival_deadlines;

struct timeval* timewindow_edf_waiting;//for each class
int * class_dispatched;
//what if no items are queued at the edf?
//what if it's queued, but never dequeued?

int* timewindow_95_percentile; //for each class
int* last_timewindow_95_percentile; //for each class
int* current_outstanding;

int timewindow_max_outstanding = 0;
int timewindow_queue_threshold = TWOL_EDF_INFINITE;
int timewindow_current_queue_length;
int timewindow_total_resp;
int timewindow_current_outstanding;

struct timeval current_time_window_end, next_time_window_end;
int timewindow_interval;
int timewindow_ttl_dispatched;
int timewindow_ttl_completed;
int *timewindow_completed;
int *timewindow_dispatched;

float next_timewindow_X_lower;
float next_timewindow_X_upper;
int next_timewindow_X_lower_length;
int next_timewindow_X_upper_length;
float next_timewindow_RT;
float* next_timewindow_queue_thresholds;
float next_timewindow_queue_threshold;

int edf_queue_length;
PINT_llist_p iedf_llist_queue; //the edf queue, ordered by deadline from this momemnt,e.g. -1ms, -0.5ms, 1ms. 10ms...
extern int* iedf_deadlines;
extern struct timeval *iedf_deadlines_timeval;
PINT_llist_p* iedf_llist_resp;

int iedf_item_id=0;
int expected_time;

PINT_llist_p* sarc_credit_list;

//struct heap * twolsfqd_heap_queue;
int twolsfqd_item_id=0;

int twolsfqd_timewindow = TWOL_EDF_TIME;//in ms for collecting data
int twolsfqd_replenish_time = TWOL_REPLENISH_TIME;//in ms for replinishing spareness
struct timeval twolsfqd_timewindow_timeval;
int twolsfqd_edf_infinite = TWOL_EDF_INFINITE;
struct timeval last_replenish_time;
int twolsfqd_replenish_amount= TWOL_REPLENISH_AMOUNT;


//use twolsfqd_enqueue to enqueue //return false;
//use dummy when completed hook is raised - doesn't do anything

int print_ip_app_item(void* item);
int twolsfqd_add_ttl_tp(int this_amount, int app_index)
{
	total_throughput+=this_amount;
    app_stats[app_index].byte_counter=app_stats[app_index].byte_counter+this_amount;
    app_stats[app_index].app_throughput=app_stats[app_index].app_throughput+this_amount;
    return 0;
}

const struct scheduler_method sch_2lsfqd = {
    .method_name = TWOLSFQD_SCHEDULER,
    .work_conserving = 1,
    .sch_initialize = twolsfqd_init,
    .sch_finalize = NULL,
    .sch_enqueue = twolsfqd_enqueue,
    .sch_dequeue = twolsfqd_dequeue,//no use
    .sch_load_data_from_config = twolsfqd_load_data_from_config,
    .sch_update_on_request_completion = twolsfqd_edf_complete,
    .sch_get_scheduler_info = twolsfqd_get_scheduler_info,
    .sch_is_idle = twolsfqd_is_idle,
    .sch_current_size = twolsfqd_current_size,
    .sch_add_ttl_throughput = twolsfqd_add_ttl_tp,
    .sch_self_dispatch = 1
};
int twolsfqd_is_idle()
{
	return 0;
}
int twolsfqd_init()
{
	pthread_mutex_init(&twolsfqd_sfqd_queue_mutex, NULL);
	twolsfqd_timewindow_timeval.tv_sec  = twolsfqd_timewindow/1000;
	twolsfqd_timewindow_timeval.tv_usec = (twolsfqd_timewindow - twolsfqd_timewindow_timeval.tv_sec*1000)*1000;
	gettimeofday(&last_replenish_time, 0);
	//twolsfqd_heap_queue=(struct heap*)malloc(sizeof(struct heap));

	//heap_init(twolsfqd_heap_queue);
	iedf_llist_resp=(PINT_llist_p*)malloc(sizeof(PINT_llist_p));

	current_outstanding = (int*)malloc(num_apps*sizeof(int));


	period_cumulative_request_count=(int*)malloc(num_apps*sizeof(int));
	period_cumulative_request_size=(int*)malloc(num_apps*sizeof(int));
	last_app_rate=(double*)malloc(num_apps*sizeof(int));

	iedf_llist_queue=PINT_llist_new();

	sarc_credit_list=(PINT_llist_p*)malloc(num_apps*sizeof(PINT_llist_p));
	sarc_credits=(int*)malloc(num_apps*sizeof(int));

	timewindow_completed = (int*) malloc(num_apps*sizeof(int));

	timewindow_dispatched = (int*) malloc(num_apps*sizeof(int));

	twolsfqd_missed_storages = (int*) malloc(num_apps*sizeof(int));
	timewindow_edf_waiting = (struct timeval*) malloc(num_apps*sizeof(struct timeval));

	twolsfqd_last_finish_tags=(int*)malloc(num_apps*sizeof(int));
	timewindow_95_percentile=(int*)malloc(num_apps*sizeof(int));
	last_timewindow_95_percentile=(int*)malloc(num_apps*sizeof(int));

	timewindow_arrival_deadlines = (int*)malloc(num_apps*sizeof(int));
	timewindow_previous_arrival_deadlines = (int*)malloc(num_apps*sizeof(int));

	class_dispatched = (int*)malloc(num_apps*sizeof(int));
	next_timewindow_queue_thresholds = (float*)malloc(num_apps*sizeof(float));

	int i;
	struct timeval temp;
	temp.tv_sec=0;
	temp.tv_usec=0;
	timewindow_arrival=0;
	for (i=0;i<num_apps;i++)
	{
		sarc_credits[i]=app_stats[i].app_rate*1048576;
		//assumming that rate is in megabytes
		sarc_credit_list[i]=PINT_llist_new();
		last_app_rate[i]=0.0f;
		period_cumulative_request_count[i]=0;
		period_cumulative_request_size[i]=0;
		current_outstanding[i] = 0;
		iedf_llist_resp[i]=PINT_llist_new();
		class_dispatched[i]=0;
		timewindow_arrival_deadlines[i]=0;
		timewindow_previous_arrival_deadlines[i]=0;
		twolsfqd_last_finish_tags[i]=0;
		timewindow_95_percentile[i]=0;
		last_timewindow_95_percentile[i]=0;
		timewindow_edf_waiting[i].tv_sec=0;
		timewindow_edf_waiting[i].tv_usec=0;
		timewindow_completed[i]=0;
		timewindow_dispatched[i]=0;
		twolsfqd_missed_storages[i]=0;
		next_timewindow_queue_thresholds[i]=1;
	}

	char* deptht=(char*)malloc(sizeof(char)*40);
	snprintf(deptht, 40, "%s.latency_track.txt", log_prefix);
	depthtrack = fopen(deptht,"w");
	setbuf(depthtrack, (char*)NULL);
	//start threads
	int rc;
	pthread_t thread_replenish, thread_window;
	rc = pthread_create(&thread_replenish, NULL, twolsfqd_time_replenish, NULL);
	rc = pthread_create(&thread_window, NULL, twolsfqd_time_window, NULL);
	return 0;
}

//used by sf queue


int twolsfqd_packet_cmp(struct heap_node* _a, struct heap_node* _b)
{
	struct generic_queue_item *g_a, *g_b;
	struct twolsfqd_queue_item *a, *b;

	g_a = (struct generic_queue_item*) heap_node_value(_a);
	g_b = (struct generic_queue_item*) heap_node_value(_b);
	a = (struct twolsfqd_queue_item *)g_a->embedded_queue_item;
	b = (struct twolsfqd_queue_item *)g_b->embedded_queue_item;
	if (a->start_tag < b->start_tag)
	{
		return 1;
	}
	else if (a->start_tag == b->start_tag)
	{
		return g_a->item_id < g_b->item_id;//to ensure that the order in which the requests are received are dispatched the same way.
	}
	else
	{
		return 0;
	}

}

int twolsfqd_add_item(struct heap* heap, struct generic_queue_item* item)
{
	struct heap_node* hn = malloc(sizeof(struct heap_node));
	heap_node_init(hn, item);
	int dispatched=0;

	heap_insert(twolsfqd_packet_cmp, heap, hn);
	return dispatched;
}


int twolsfqd_current_size(struct request_state * original_rs, long long actual_data_file_size)
{

	struct generic_queue_item * current_item = original_rs->current_item;
	struct twolsfqd_queue_item * twolsfqd_item = (struct twolsfqd_queue_item * )(current_item->embedded_queue_item);

	twolsfqd_item->data_file_size=actual_data_file_size;

	int strip_size = twolsfqd_item->strip_size;
	int server_nr = twolsfqd_item->server_nr;
	int server_count= twolsfqd_item->server_count;

	long long offset = twolsfqd_item->file_offset;
	//update expected receivables for current item!
	long long ask_size = twolsfqd_item->aggregate_size;

    int my_shared_size = get_my_share(strip_size, server_count, offset, ask_size, server_nr);
    twolsfqd_item->task_size=my_shared_size;


    return my_shared_size;

}

//this is a hooked function, useful for moving current_size function to a single point of accountability/shared function
void twolsfqd_adjust_task_size(long long actual_data_file_size, int task_size, struct request_state * rs)
{
	struct generic_queue_item * current_item = rs->current_item;
	struct sfqd_queue_item * sfqd_item = (struct sfqd_queue_item * )(current_item->embedded_queue_item);

	sfqd_item->data_file_size=actual_data_file_size;
	sfqd_item->task_size=task_size;
}

int twolsfqd_enqueue(struct socket_info * si, struct pvfs_info* pi)
{
	//fprintf(stderr,"enqueueing.......................................\n");
	app_stats[si->app_index].received_requests+=1;
	int r_socket_index, d_socket_index, length, tag, io_type, req_size;
	char* request = si->buffer;
	r_socket_index = si->request_socket;
	d_socket_index = si->data_socket;

	length = pi->current_data_size;
	tag=  pi->tag;
	io_type= pi->io_type;
	req_size=pi->req_size;



	char* ip = s_pool.socket_state_list[d_socket_index].ip;

	int port= s_pool.socket_state_list[d_socket_index].port;
	int d_socket=s_pool.socket_state_list[d_socket_index].socket;
	int socket_tag = tag;

	int app_index= s_pool.socket_state_list[r_socket_index].app_index;



	int weight = s_pool.socket_state_list[r_socket_index].weight;

	int start_tag=MAX(twolsfqd_virtual_time, twolsfqd_last_finish_tags[app_index]);//work-conserving
	int cost;
	int reducer, finish_tag;

	//fprintf(stderr,"app %i, size %i\n",app_index, length);

	if (cost_model==COST_MODEL_NONE)
	{
		//reducer=REDUCER;
		cost=length;
		finish_tag = start_tag+cost/weight/REDUCER;
	}
	else
	{

		cost=length;
				get_expected_resp(app_index, r_socket_index, io_type);//length*(sfqd_current_depth+1);
				finish_tag = start_tag+cost/weight/REDUCER;
	}

	twolsfqd_last_finish_tags[app_index]=finish_tag;

	struct generic_queue_item * generic_item =  (struct generic_queue_item * )malloc(sizeof(struct generic_queue_item));
	struct twolsfqd_queue_item * item = (struct twolsfqd_queue_item *)(malloc(sizeof(struct twolsfqd_queue_item)));
	generic_item->embedded_queue_item=item;
	item->start_tag=start_tag;
	item->finish_tag=finish_tag;

	item->data_socket_index=d_socket_index;
	item->request_socket_index=r_socket_index;
	item->data_file_size=0;
	item->server_count=pi->total_server;
	item->server_nr=pi->current_server;
	item->file_offset=pi->req_offset;
	item->aggregate_size=pi->aggregate_size;
	item->strip_size=pi->strip_size;
	generic_item->item_id=twolsfqd_item_id++;
	fprintf(stderr,"iiiiiiiiiiiiiii item id is %i\n", generic_item->item_id);

	char bptr[20];
    struct timeval tv;
    gettimeofday(&tv, 0);
    get_time_string(&tv, bptr);

	item->data_port=port;
	item->data_ip=ip;
	item->task_size=length;

	fprintf(depthtrack, "%s %s offset %lli size %i %s:%i, %i\n", log_prefix, bptr, pi->req_offset, length,
			s_pool.socket_state_list[r_socket_index].ip,s_pool.socket_state_list[r_socket_index].port, io_type);

	item->data_socket=d_socket;
	item->socket_tag=socket_tag;
	item->app_index=app_index;
	item->stream_id=app_stats[app_index].stream_id++;
	item->got_size=0;
	item->request_socket=s_pool.socket_state_list[r_socket_index].socket;
	item->io_type=io_type;
	item->buffer=request;
	item->buffer_size=req_size;
	item->dispatched=-1;


	generic_item->socket_data=si;

	period_cumulative_request_size[app_index]+=item->aggregate_size;
	period_cumulative_request_count[app_index]+=1;
	app_stats[app_index].app_nr=item->server_count;

	fprintf (stderr,"new request on socket %i!!!!!!!!!!!!!!!!!!!!1\n", r_socket_index);
	pthread_mutex_lock(&twolsfqd_sfqd_queue_mutex);

	//Dprintf(D_CACHE, "[INITIALIZE]ip:%s port:%i tag:%i\n", item->data_ip, item->data_port, item->socket_tag);


	//twolsfqd_add_item(twolsfqd_heap_queue,generic_item);
	//re-write this function to add item to its own list, or, expand it here
	PINT_llist_add_to_tail(sarc_credit_list[app_index], generic_item);

	si->rs->current_item = generic_item;

	int dispatched=0;



	//current_depth now means current_credit
	if (sarc_credits[app_index]>=length)
	{
		//dispatched=1;
		fprintf(stderr,"enqueue\n");
		if (twolsfqd_dequeue_all(generic_item)<=0)
		{
			si->rs->locked = 1;
		}

	}
	else
	{
		fprintf(stderr,"app %i has not enough credits left ....", app_index);
		//check spareness
		twolsfqd_update_spareness();
		if (sarc_credits[app_index]>=length)
		{
			//dispatched=1;
			fprintf(stderr,"app %i found after update\n", app_index);
			if (twolsfqd_dequeue_all(generic_item)<=0)
			{
				//fprintf(stderr,"2LSFQD 2item delayed on socket %i\n",item->request_socket);
				si->rs->locked = 1;
			}
		}
		else
		{
			fprintf(stderr," still not enough credit left\n");
			//fprintf(stderr,"2LSFQD 3item delayed on socket %i\n",item->request_socket);
			si->rs->locked = 1;

		}

	}

	app_stats[app_index].req_come+=1;
	pthread_mutex_unlock(&twolsfqd_sfqd_queue_mutex);
	return 0;//never dispatch at this level

}



void twolsfqd_get_scheduler_info()
{
	//fprintf(stderr,"depth remains at %i", current_depth);
	//fprintf(stderr," current queue has %i items\n",heap_queue->all_count);
}


int twolsfqd_update_on_request_completion(void* arg)
{

	return 0;
}


int twolsfqd_dequeue_all(struct generic_queue_item * g_item)
{

	//credits are stored in twolsfqd_current_depth
	//decreasing it effectively drains the credits
	//fprintf(stderr, "Dequeueing all because of credit replenish\n");
	int dispatched = 0;
	int r_dispatched = 0;
	int i;

	struct generic_queue_item* citem = twolsfqd_dequeue();
	while (citem!=NULL)
	{
		citem=twolsfqd_dequeue();
	}

	if (g_item!=NULL)
	{
		struct twolsfqd_queue_item* q_item = (struct twolsfqd_queue_item*) (g_item->embedded_queue_item);
		return q_item->dispatched;
	}
	else
		return 0;
}

/* *
 * this dispatches to the edf queue, it is immediately called after a batch of sfq enqueue operations
 * to ensure fairness to the edf queue and create backlog when admitting requests.
 *
 * at each time window's beginning, check depth and try again up to the length of queue threshold
 * time window will call this function
 * */



struct generic_queue_item* twolsfqd_dequeue()
{

/* hand over the request to edf queue /enqueue
 *
 *
 * */

	struct generic_queue_item * next, *temp;
	//free up current item
	int dispatched = 0;

	int k=0;

	while (k<num_apps)//try at most all apps once
	{
		fprintf(stderr,"dispatching %i app xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n", queue_index);
		temp = (struct generic_queue_item *)PINT_llist_head(sarc_credit_list[queue_index]);
		if (temp!=NULL)
		{

			struct twolsfqd_queue_item *next_item = (struct twolsfqd_queue_item *)(temp->embedded_queue_item);
			if (sarc_credits[queue_index]>=next_item->task_size)
			{

				next = (struct generic_queue_item *)
						PINT_llist_rem(sarc_credit_list[queue_index], (void*)temp->item_id,  list_req_comp);
				//we are just trying to grab the same next_item
				fprintf(stderr,"item id is %i xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n", temp->item_id);
				assert(next!=NULL);

				next_item = (struct twolsfqd_queue_item *)(next->embedded_queue_item);
				sarc_credits[queue_index]-=next_item->task_size;
				dispatched=1;
				next_item->dispatched=0;
				twolsfqd_edf_enqueue(next);

				timewindow_dispatched[next_item->app_index]++;
				timewindow_ttl_dispatched++;
				current_outstanding[next_item->app_index]++;
				struct timeval now;
				gettimeofday(&now, 0);
				next_item->queuedtime=now;
				queue_index= (queue_index+1) % num_apps;
				return next;
			}
		}
		queue_index= (queue_index+1) % num_apps;
		k++;
	}

	if (dispatched==1)
	{
		assert(dispatched!=1);
	}
	else
	{
		return NULL;
	}
}



int twolsfqd_edf_enqueue(struct generic_queue_item* item)
{

	struct timeval now;
	gettimeofday(&now, 0);

	timewindow_arrival++;


	PINT_llist_add_to_tail(iedf_llist_queue, item);
	edf_queue_length++;



	struct twolsfqd_queue_item *next_item = (struct twolsfqd_queue_item *)(item->embedded_queue_item);

	timeradd(&now, &(iedf_deadlines_timeval[0]), &(next_item->deadline));
	next_item->timewindow = timewindow_id;
	if (timewindow_current_queue_length!=0)
	{
		PINT_llist_sort(iedf_llist_queue,list_edf_sort_comp);
	}



	fprintf(depthtrack,"current deadline: %i.%06i, current window end: %i.%06i\n",
			(int)next_item->deadline.tv_sec, (int)next_item->deadline.tv_usec,
			(int)current_time_window_end.tv_sec, (int)current_time_window_end.tv_usec);
	if (timercmp(&(next_item->deadline), &current_time_window_end,< ))
	{
		fprintf(depthtrack,"deadline in current window\n");
		timewindow_arrival_deadline++;
		timewindow_arrival_deadlines[next_item->app_index]++;
		next_item->deadline_in_current_timewindow=1;
	}
	next_item->queuedtime=now;

	//fprintf(stderr,"called in edf_enqueue\n");

	int dispatched = twolsfqd_edf_dequeue_all(next_item->request_socket);
	next_item->dispatched=dispatched;

	if (timewindow_current_queue_length>timewindow_max_outstanding)
	{
		timewindow_max_outstanding=timewindow_current_queue_length;
		fprintf(depthtrack,"max outstanding updated to %i\n",timewindow_current_queue_length);
	}


	return dispatched;
}

int twolsfqd_edf_dequeue_all(int enqueued_socket)
{
	//1. dequeue those who misses deadlines
	//2. if time window changes, allow all queue items to be dispatched until queue threshold is reached
	//fprintf(stderr,"trying to dispatch as many requests as possible in edf...\n");

	PINT_llist_p queue;
	queue= iedf_llist_queue->next;
	struct twolsfqd_queue_item * item;
	int item_count=0;
	struct timeval now;
	gettimeofday(&now, 0);
	//this is a sorted list, so first "if" dispatches deadline misses no matter what the queue threshold is
	//if the items are not missing deadlines, then it takes queue threshold into consideration.
	//last, if everything necessary is dispatched

	while (queue!=NULL)
	{

		item = (struct twolsfqd_queue_item*)(((struct generic_queue_item *)queue->item)->embedded_queue_item);

		fprintf(stderr,"deadline:%i.%i, now: %i.%i\n", item->deadline.tv_sec, item->deadline.tv_usec,
		now.tv_sec, now.tv_usec	);
		if ( timercmp(&(item->deadline), &now, <=))
		{
			fprintf(depthtrack,"missing dealine %i.%06i <= now %i.%06i\n",
					(int)item->deadline.tv_sec, (int)item->deadline.tv_usec,
					(int)now.tv_sec, (int)now.tv_usec);
			twolsfqd_missed_storage++;
			twolsfqd_missed_storages[item->app_index]++;
			item->missed=1;
			item->miss_start_timewindow=timewindow_id;
			item_count++;
		}
		else if (timewindow_current_queue_length+item_count < timewindow_queue_threshold)
		{
			item_count++;
			fprintf(depthtrack,"current %i + item %i < threshold %i\n",timewindow_current_queue_length, item_count, timewindow_queue_threshold);
		}
		else
		{
			fprintf(depthtrack,"current %i + item %i >= threshold %i\n",timewindow_current_queue_length, item_count, timewindow_queue_threshold);
			break;
		}
		queue= queue->next;
	}
	int i;
	int dispatched=0;
	for (i=0; i< item_count;i++)
	{
		//possible synchronization point! spareness thread could have depleted thisw

		struct generic_queue_item * item = twolsfqd_edf_dequeue();

		struct twolsfqd_queue_item * twolsfqd_item =  (struct twolsfqd_queue_item *)(item->embedded_queue_item);
		if (twolsfqd_item->request_socket==enqueued_socket)
		{
			dispatched=1;
		}
	}

	if (item_count>0)
	{
		fprintf(depthtrack,"*******************edf dispatched %i requests******************\n", item_count);
		twolsfqd_update_spareness();
		fprintf(stderr,"edf update\n");
		twolsfqd_dequeue_all(NULL);

	}
	else
	{
	}
	if (dispatched==1)
	{
		fprintf(depthtrack,"*******************service immediately!*********************\n");
	}

	return dispatched;

	//call twolsfqd_edf_dequeue() item_count times.

}

int all_requests=1;

struct generic_queue_item* twolsfqd_edf_dequeue()
{

	//find the item in the queue
	struct generic_queue_item * item  = (struct generic_queue_item *)PINT_llist_head(iedf_llist_queue);

	struct generic_queue_item * next_item = NULL;

	if (item!=NULL)
	{
		next_item = (struct generic_queue_item *)
				PINT_llist_rem(iedf_llist_queue, (void*)item->item_id,  list_req_comp);
	}
	else
	{
		return NULL;
	}

	if (next_item!=NULL)
	{
		fprintf(stderr,"$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ dispatching %i\n", item->item_id);
		struct twolsfqd_queue_item * twolsfqd_item = (struct twolsfqd_queue_item *)(next_item->embedded_queue_item);
		struct timeval dispatch_time, diff;
		gettimeofday(&dispatch_time, 0);
		twolsfqd_item->dispatchtime = dispatch_time;
		twolsfqd_item->dispatched=1;
		if (twolsfqd_item->timewindow==timewindow_id)
		{
			fprintf(depthtrack, "arrival decreased because edf is dispatched to outstanding\n");
			timewindow_arrival--;
			if (twolsfqd_item->deadline_in_current_timewindow==1)
			{
				timewindow_arrival_deadline--;
			}
		}

		get_time_diff(&twolsfqd_item->queuedtime, &diff);
		int app_index = twolsfqd_item->app_index;
		fprintf(depthtrack,"diff: %i.%06i\n", (int)diff.tv_sec, (int)diff.tv_usec);

		timeradd(&diff, timewindow_edf_waiting+app_index, timewindow_edf_waiting+app_index);

		//fprintf(stderr,"edf waiting of class %i updated to %i.%06i\n", app_index,
			//	(int)timewindow_edf_waiting[app_index].tv_sec, (int)timewindow_edf_waiting[app_index].tv_usec);

		twolsfqd_virtual_time=twolsfqd_item->start_tag;

		next_item->socket_data->rs->locked = 0;

		app_stats[app_index].req_go+=1;
		app_stats[app_index].dispatched_requests+=1;
		timewindow_current_queue_length++;
		edf_queue_length--;
		fprintf(depthtrack,"storage utility queue length: %i\n", timewindow_current_queue_length);
		return next_item;
	}

	return next_item;

}

int cumulativetime;
int timewindow_nintyfive;

void nintyfive_percentile()//calculate the history in the current window
{
	int i;

	int item_time=0;

	int j;
	for (j=0; j<num_apps; j++)
	{
		cumulativetime=0;
		if (timewindow_completed[j]!=0)
		{
			PINT_llist_p resp_list = PINT_llist_sort(iedf_llist_resp[j],list_resp_sort_comp);

			if (resp_list==NULL)
			{

				//timewindow_95_percentile[j]=0;
				//fprintf(stderr,"no resp records for class %i\n", j+1);
				continue;
			}
			resp_list= resp_list->next;

			if (resp_list==NULL)
			{
				//timewindow_95_percentile[j]=0;
				//fprintf(stderr,"no resp records for class %i\n", j+1);
				continue;
			}
			for (i=0; i<timewindow_completed[j]; i++)
			{
				if (resp_list==NULL)
				{
					//fprintf(stderr,"Went over the whole list for class %i\n", j+1);
					break;
				}
				item_time = ((struct resp_time *)resp_list->item)->resp_time;
				cumulativetime += item_time;
				//fprintf(stderr,"%i/%i ", item_time, cumulativetime);
				last_timewindow_95_percentile[j] = timewindow_95_percentile[j];
				timewindow_95_percentile[j]= item_time;
				if (cumulativetime>=timewindow_nintyfive)
				{
					fprintf(stderr,"exceeded %i\n",timewindow_nintyfive);
					//fprintf(stderr,"Found nintyfifth percentile before going over the whole list of class %i: %i\n", j+1, item_time);
					break;
				}
				resp_list= resp_list->next;

			}
			fprintf(stderr,"value calculated from response is %i\n",item_time );
		}
		else if (current_outstanding[j]!=0)
		{	int k=0;

			int found=0;
			PINT_llist_p jtime =PINT_llist_new();
			for (k=0;k<s_pool.pool_size;k++)
			{
				PINT_llist_p head = s_pool.socket_state_list[k].req_state_data->next;

				while (head!=NULL)
				{
					if (((struct request_state *)head->item)->current_item!=NULL)
					{
						struct twolsfqd_queue_item * twolsfqd_item =
								(struct twolsfqd_queue_item * )
								(((struct request_state *)head->item)->current_item->embedded_queue_item);

						if ( twolsfqd_item->dispatched==1 && twolsfqd_item->app_index==j)
						{
							//accumulate time values here
							//form a list
							struct resp_time * new_resp = (struct resp_time *) malloc(sizeof(struct resp_time));
							struct timeval diff;
							get_time_diff(&(twolsfqd_item->dispatchtime), &diff);
							new_resp->resp_time=diff.tv_sec*1000+diff.tv_usec/1000;
							PINT_llist_add_to_tail(jtime, (void*) (new_resp));
							PINT_llist_sort(jtime,list_resp_sort_comp);
							found++;
							if (found>=current_outstanding[j])
								break;
						}
					}
					head = head -> next;
				}
			}

			jtime= jtime->next;

			if (jtime==NULL)
			{
				//timewindow_95_percentile[j]=0;
				//fprintf(stderr,"no resp records for class %i\n", j+1);
				continue;
			}
			for (i=0; i<current_outstanding[j]; i++)
			{
				if (jtime==NULL)
				{
					//fprintf(stderr,"Went over the whole list for class %i\n", j+1);
					break;
				}
				item_time = ((struct resp_time *)jtime->item)->resp_time;
				cumulativetime += item_time;
				//fprintf(stderr,"%i/%i ", item_time, cumulativetime);
				last_timewindow_95_percentile[j] = timewindow_95_percentile[j];
				timewindow_95_percentile[j]= item_time;
				if (cumulativetime>=timewindow_nintyfive)
				{
					//fprintf(stderr,"Found nintyfifth percentile before going over the whole list of class %i: %i\n", j+1, item_time);
					fprintf(stderr,"exceeded %i\n",timewindow_nintyfive);
					break;
				}
				jtime = jtime->next;
			}
			fprintf(stderr,"value calculated from outstanding is %i\n",item_time );
		}
		else
		{
			fprintf(stderr,"nothing to calculate for app %i\n",j);
			timewindow_95_percentile[j]=0;
		}
		///now edf waiting time also needs the accumulative values from the queued time for long-stacked items
		///if they cross multiple timewindows
		int l;
		for (l=0;l<s_pool.pool_size;l++)
		{
			PINT_llist_p head = s_pool.socket_state_list[l].req_state_data->next;

			while (head!=NULL)
			{
				if (((struct request_state *)head->item)->current_item!=NULL)
				{
					struct twolsfqd_queue_item * twolsfqd_item =
							(struct twolsfqd_queue_item * )
							(((struct request_state *)head->item)->current_item->embedded_queue_item);

					if (twolsfqd_item->dispatched==0 && twolsfqd_item->app_index==j)
					{
						struct timeval diff;
						get_time_diff(&twolsfqd_item->queuedtime, &diff);
						int app_index = twolsfqd_item->app_index;
						fprintf(stderr,"queued item counts towards edf waiting: %i.%06i\n", (int)diff.tv_sec, (int)diff.tv_usec);
						timeradd(&diff, timewindow_edf_waiting+app_index, timewindow_edf_waiting+app_index);
						/**
						 * check if they're in the edf queue yet!
						 * */
					}
				}
				head = head -> next;
			}
			//same thing did as when the request was dispatched.
		}
	}
}


//hooked by the scheduler complete hook!
int twolsfqd_edf_complete(void * arg)
{
	pthread_mutex_lock(&twolsfqd_sfqd_queue_mutex);

	struct complete_message * complete = (struct complete_message *)arg;
	struct generic_queue_item * current_item;
	struct twolsfqd_queue_item * twolsfqd_item;



	current_item = (complete->current_item);
	twolsfqd_item = (struct twolsfqd_queue_item * )(current_item->embedded_queue_item);
	fprintf(stderr,"got from item %i on socket %i\n", current_item->item_id, complete->complete_size);
	if (complete->complete_size>0)
	{

		twolsfqd_item->got_size+=(complete->complete_size);
	}
	else
	{
		twolsfqd_item->got_size=twolsfqd_item->task_size;
	}


	//fprintf(stderr,"sock %i ^^^^^^^^^^^^^^^^^Got %i/%i ^^^^^^^^^^^^^^^^^^\n",
		//	s_pool.socket_state_list[twolsfqd_item->request_socket_index].socket,
			//twolsfqd_item->got_size, twolsfqd_item->task_size);
	if (twolsfqd_item->task_size==twolsfqd_item->got_size)
	{
		if (twolsfqd_item->timewindow==timewindow_id)
		{
			timewindow_arrival++;
			if (twolsfqd_item->deadline_in_current_timewindow==1)
			{
				timewindow_arrival_deadline++;
			}
		}
		int app_index = twolsfqd_item->app_index;
		app_stats[app_index].completed_requests+=1;
		//fprintf(stderr,"**************************************************\n");
		timewindow_ttl_completed++;
		timewindow_completed[app_index]++;
		current_outstanding[app_index]--;
		if (twolsfqd_item->missed==1)
		{
			twolsfqd_missed_storages[twolsfqd_item->app_index]--;
			twolsfqd_missed_storage--;
			int diff_timewindow = timewindow_id - twolsfqd_item->miss_end_timewindow;
			twolsfqd_missed_time -= (MAX(diff_timewindow-1, 0))*iedf_deadlines[twolsfqd_item->app_index]*2;
			//deduct from accumulated value
		}
		//add new timewindow_resp_time the list;
		struct resp_time * new_resp = (struct resp_time *) malloc(sizeof(struct resp_time));
		struct timeval diff,n;
		get_time_diff(&(twolsfqd_item->dispatchtime), &diff);
		gettimeofday(&n,0);
		new_resp->resp_time=diff.tv_sec*1000+diff.tv_usec/1000;//in ms
		timewindow_total_resp+=new_resp->resp_time;

		fprintf(depthtrack, "%i response time of class %i: %i ms on ip %s\n",
				n.tv_sec,twolsfqd_item->app_index, new_resp->resp_time, s_pool.socket_state_list[twolsfqd_item->request_socket_index].ip);
		//fprintf(stderr, "response time of class %i: %i ms on ip %s\n",
				//twolsfqd_item->app_index, new_resp->resp_time, s_pool.socket_state_list[twolsfqd_item->request_socket_index].ip);


		average_resp_time[twolsfqd_item->app_index]+=(diff.tv_sec*1000+diff.tv_usec/1000);

		PINT_llist_add_to_tail(iedf_llist_resp[twolsfqd_item->app_index], (void*)new_resp);

		timewindow_current_queue_length--;
		//fprintf(stderr,"EDf completed\n");
		twolsfqd_edf_dequeue_all(-1);


		//timewindow_current_queue_length try to dispatch
		//indication of spareness update?
		pthread_mutex_unlock(&twolsfqd_sfqd_queue_mutex);
		return twolsfqd_item->got_size;//prevent framework from dispatching
	}
	else if (twolsfqd_item->task_size < twolsfqd_item->got_size)
	{
		twolsfqd_item->over_count++;
		fprintf(stderr,"error from sock %i, item %i\n",twolsfqd_item->request_socket_index, twolsfqd_item->stream_id);
		fprintf(stderr, "error...got_size %i> task_size %i (completed %i), op is %i, error_count is %i\n",
				twolsfqd_item->got_size,twolsfqd_item->task_size, complete->complete_size, twolsfqd_item->io_type,twolsfqd_item->over_count);
		//getchar();
		pthread_mutex_unlock(&twolsfqd_sfqd_queue_mutex);
		return -1;
		//return sfqd_item->got_size;

	}
	else
	{
		pthread_mutex_unlock(&twolsfqd_sfqd_queue_mutex);
		//fprintf(stderr, "inprogress...got_size %i< task_size %i (completed %i), op is %i, error_count is %i\n",sfqd_item->got_size,sfqd_item->task_size, complete->complete_size, sfqd_item->io_type,sfqd_item->over_count);
		return 0;
	}

}

int twolsfqd_load_data_from_config (dictionary * dict)
{
	twolsfqd_timewindow=iniparser_getint(dict, "TWOLSFQD:EDF_timewindow" ,twolsfqd_timewindow);
	fprintf(stderr,"TWOLSFQD using EDF time window:%i\n",twolsfqd_timewindow);

	twolsfqd_edf_infinite=iniparser_getint(dict, "TWOLSFQD:EDF_infinite", twolsfqd_edf_infinite);
	fprintf(stderr,"TWOLSFQD using edf infinite:%i\n",twolsfqd_edf_infinite);

	twolsfqd_replenish_time=iniparser_getint(dict, "TWOLSFQD:SARC_timewindow" ,twolsfqd_replenish_time);
	fprintf(stderr,"TWOLSFQD using SARC time window:%i\n",twolsfqd_replenish_time);

	twolsfqd_replenish_amount=iniparser_getint(dict, "TWOLSFQD:replenish_amount" ,twolsfqd_replenish_amount);
	fprintf(stderr,"TWOLSFQD using SARC replenish amount:%i\n",twolsfqd_replenish_amount);

	int i;
	timewindow_queue_threshold = twolsfqd_edf_infinite/2;

	for (i=0;i<num_apps;i++)
	{

		char l[6];
		sprintf(l,"app%i",i+1);
		char* l2 = (char*)malloc(sizeof(char)*(strlen(l)+11));
		sprintf(l2, "latencies:%s", l);
		char* l3 = (char*)malloc(sizeof(char)*(strlen(l)+7));
		sprintf (l3, "rates:%s", l);
		app_stats[i].app_rate = iniparser_getint(dict, l3 ,100);
		iedf_deadlines[i]=iniparser_getint(dict, l2 ,2000);
		iedf_deadlines_timeval[i].tv_sec=iedf_deadlines[i]/1000;
		iedf_deadlines_timeval[i].tv_usec=(iedf_deadlines[i]-iedf_deadlines_timeval[i].tv_sec*1000)*1000;
		fprintf(stderr,"TWOLSFQD using latency:%i.%06i\n",
				(int)iedf_deadlines_timeval[i].tv_sec,
				(int)iedf_deadlines_timeval[i].tv_usec);
		fprintf(stderr,"TWOLSFQD using rate:%i\n", app_stats[i].app_rate);
		twolsfqd_ttl_rate+=app_stats[i].app_rate;
		app_stats[i].app_nr=default_nr;
	}
	return 0;
}

void twolsfqd_edf_dispacher(void * arg)
{

	//this function can be hooked in normal queueing operations
}

void free_resp_item (void * arg)
{
	free((struct resp_item * )arg);
}

void* twolsfqd_time_window(void * arg)
{
	//pthread_mutex_unlock(&twolsfqd_sfqd_queue_mutex);
	/* waiting time span over two time window problem
	 */

	/* leave untouched queue threshold values
	 * completed/arrivals/arrival_deadline
	 * max outstanding length
	 * average waiting time:timewindow_edf_waiting[i].seconds = 0
	 * auto clear: 95 percentile : use list free function: PINT_llist_free for each class
	 */

	int i;

	while (1)
	{

		usleep(twolsfqd_timewindow*1000);
		pthread_mutex_lock(&twolsfqd_sfqd_queue_mutex);
		timewindow_id++;

		fprintf(stderr, "%s Time window working...after sleeping %i micro seconds\n", log_prefix, twolsfqd_timewindow*1000);
		next_timewindow_queue_threshold = twolsfqd_edf_infinite;

		timewindow_nintyfive = 0.95 * timewindow_total_resp;

		/* this value should bear some outstanding working time too if no work completed in last time window!
		 * otherwise, 95 percentile may take outstanding continuous working time if it does not finish.
		 * but total resp *.95 as a threshold may be 0...thus the smallest waiting time in the outstanding queue
		 * may be used as a 95th percentile response time.
		 * */


		fprintf(stderr,"total resp time is  %i, threshold is %i\n", timewindow_total_resp, timewindow_nintyfive);
		nintyfive_percentile();
		//clear resp list

		PINT_llist_p queue= iedf_llist_queue->next;
		struct twolsfqd_queue_item * item;
		int item_count=0;
		int* item_counts = (int *)malloc(num_apps*sizeof(int));
		memset(item_counts, 0, num_apps*sizeof(int));

		//look for the items in the edf that are missing today's deadline
		while (queue!=NULL)
		{
			item = (struct twolsfqd_queue_item*)(((struct generic_queue_item *)queue->item)->embedded_queue_item);

			fprintf(depthtrack,"current deadline: %i.%06i, next window end: %i.%06i\n",
					(int)item->deadline.tv_sec, (int)item->deadline.tv_usec,
					(int)next_time_window_end.tv_sec,(int)next_time_window_end.tv_usec );
			if (timercmp(&(item->deadline), &current_time_window_end, <))
			{
				fprintf(depthtrack,"item in current window end\n");
				item_counts[item->app_index]++;
			}
			else if (timercmp(&(item->deadline), &next_time_window_end, <))
			{
				fprintf(depthtrack,"item in next window end\n");
				item_count++;
			}
			else
			{
				break;
			}
			queue= queue->next;
		}

		fprintf(depthtrack, "current working: %i next deadline: %i arrival_deadline: %i, ttl completed: %i, edf length: %i arrival: %i\n",
				timewindow_current_queue_length, item_count, timewindow_arrival_deadline, timewindow_ttl_completed, edf_queue_length, timewindow_arrival);

		next_timewindow_X_lower_length = (timewindow_current_queue_length + item_count + timewindow_arrival_deadline);


		///////////////////////


		if (timewindow_ttl_completed == 0)
		{
			//
			timewindow_ttl_completed = 1;//((float)(timewindow_queue_threshold)); //keep estimation
		}
		next_timewindow_X_lower = (float)((float)next_timewindow_X_lower_length * (float)timewindow_queue_threshold) / (float)timewindow_ttl_completed;
		next_timewindow_X_upper_length = (timewindow_current_queue_length + edf_queue_length + timewindow_arrival);
		next_timewindow_X_upper = (float)((float)next_timewindow_X_upper_length * (float)timewindow_queue_threshold) / (float)timewindow_ttl_completed;

		fprintf(depthtrack,"x_upper=%i+%i+%i\n",timewindow_current_queue_length, edf_queue_length, timewindow_arrival);
		fprintf(depthtrack, "x_under:%f = %i * %i / %i x~bar:%f = %i * %i / %i\n",
				next_timewindow_X_lower,  next_timewindow_X_lower_length, timewindow_queue_threshold, timewindow_ttl_completed,
				next_timewindow_X_upper, next_timewindow_X_upper_length, timewindow_queue_threshold, timewindow_ttl_completed);
		fprintf(stderr, "x_under:%f = %i * %i / %i x~bar:%f = %i * %i / %i\n",
				next_timewindow_X_lower,  next_timewindow_X_lower_length, timewindow_queue_threshold, timewindow_ttl_completed,
				next_timewindow_X_upper, next_timewindow_X_upper_length, timewindow_queue_threshold, timewindow_ttl_completed);


		int overload=1;

/*		twolsfqd_missed_time = (twolsfqd_missed_storage)*iedf_deadlines[i]
		                               + twolsfqd_missed_time*2;*/

		for (i=0; i<num_apps; i++)
		{

			//////////////////////
			int timewindow_dispatched_i = timewindow_dispatched[i];

			int class_i_edf_waiting;
			if (timewindow_dispatched_i == 0)
			{
				class_i_edf_waiting = 0;
			}
			else
			{
				class_i_edf_waiting =	(timewindow_edf_waiting[i].tv_sec*1000
											+timewindow_edf_waiting[i].tv_usec/1000)
											/timewindow_dispatched_i;
			}
			fprintf(depthtrack, "response time class %i edf waiting: %i ms = %i/%i, 95 is %i\n",i+1, class_i_edf_waiting,
					(int)(timewindow_edf_waiting[i].tv_sec*1000 +timewindow_edf_waiting[i].tv_usec/1000), timewindow_completed[i],
					timewindow_95_percentile[i]);

			int timewindow_95_percentile_i = timewindow_95_percentile[i];//timewindow_completed[i];
			//////////////////////

			float e_i;

			//////////////////////

			e_i= ((float)(iedf_deadlines[i]-class_i_edf_waiting))/timewindow_95_percentile_i;

			fprintf(depthtrack,"e_i calc first: %f = (%i-%i)/%i\n",
					e_i,iedf_deadlines[i],class_i_edf_waiting,timewindow_95_percentile_i);

			if (e_i>1000)
			{
				e_i= ((float)(iedf_deadlines[i]-class_i_edf_waiting))/iedf_deadlines[i];
				fprintf(depthtrack,"e_i calc infinity: %f = (%i-%i)/%i\n",
						e_i, iedf_deadlines[i],class_i_edf_waiting,iedf_deadlines[i]);

			}
			if (e_i<-1000)
			{

				e_i=1.1;
				fprintf(depthtrack,"e_i calc negative: %f\n", e_i);

			}

			if (e_i<0.001)
			{
				fprintf(stderr,"e_i calc error! %f = 95: %i, class %i deadline: %i, edf waiting: %i\n",
						e_i, timewindow_95_percentile_i, i+1, iedf_deadlines[i], class_i_edf_waiting);
				fprintf(depthtrack,"e_i calc error! %f = 95: %i, class %i deadline: %i, edf waiting: %i\n",
						e_i, timewindow_95_percentile_i, i+1, iedf_deadlines[i], class_i_edf_waiting);
				e_i=0.001;
				fprintf(stderr,"e_i calc second: %f\n", e_i);
			}

			fprintf(depthtrack,"APP %i 95: %i edf waiting: %i\n",
					i+1, timewindow_95_percentile_i, class_i_edf_waiting);

			fprintf(depthtrack,"APP %i RT: %f X_: %f X~: %f EI: %f\n",
					i+1, next_timewindow_RT, next_timewindow_X_lower, next_timewindow_X_upper, e_i);
			//x_upper available

			if (next_timewindow_queue_thresholds[i]<twolsfqd_edf_infinite)
			{
				//underloaded case
				next_timewindow_RT =e_i*timewindow_queue_threshold;
				if (next_timewindow_RT<0.01)
				{
					next_timewindow_RT=0.01;
				}

				if (next_timewindow_RT < next_timewindow_X_lower)
				{

					fprintf(depthtrack, "case 1 of class %i RT = %f = (ei*L = %f * %i ) < X_ = %f\n",
							i, next_timewindow_RT, e_i, timewindow_queue_threshold, next_timewindow_X_lower);
					fprintf(stderr, "case 1 of class %i RT = %f = (ei*L = %f * %i ) < X_ = %f\n",
							i, next_timewindow_RT, e_i, timewindow_queue_threshold, next_timewindow_X_lower);
					next_timewindow_queue_thresholds[i] = twolsfqd_edf_infinite;

				}
				else if (next_timewindow_RT > next_timewindow_X_upper)
				{
					fprintf(depthtrack, "case 2 of class %i RT = %f = (ei*L = %f * %i ) > X~ = %f\n",
							i, next_timewindow_RT, e_i, timewindow_queue_threshold, next_timewindow_X_upper);
					fprintf(stderr, "case 2 of class %i RT = %f = (ei*L = %f * %i ) > X~ = %f\n",
							i, next_timewindow_RT, e_i, timewindow_queue_threshold, next_timewindow_X_upper);

					if (next_timewindow_X_upper<0.01)
						next_timewindow_X_upper=0.01;
					next_timewindow_queue_thresholds[i] = next_timewindow_X_upper;
					overload=0;
				}
				else if (next_timewindow_RT < timewindow_queue_threshold || timewindow_max_outstanding >= timewindow_queue_threshold)
				{
					fprintf(depthtrack, "case 3 class %i RT = %f = (ei*L = %f * %i ) < L = %i || MAXO = %i >>= L\n",
							i, next_timewindow_RT, e_i, timewindow_queue_threshold, timewindow_queue_threshold, timewindow_max_outstanding);
					fprintf(stderr, "case 3 class %i RT = %f = (ei*L = %f * %i ) < L = %i || MAXO = %i >>= L\n",
							i, next_timewindow_RT, e_i, timewindow_queue_threshold, timewindow_queue_threshold, timewindow_max_outstanding);

					next_timewindow_queue_thresholds[i] = next_timewindow_RT;
					overload=0;
				}
				else if (next_timewindow_RT >= timewindow_queue_threshold && timewindow_max_outstanding < timewindow_queue_threshold)
				{
					fprintf(depthtrack, "case 4 of class %i RT = %f = (ei*L = %f * %i ) >= L = %i && MAXO = %i < L\n",
							i, next_timewindow_RT, e_i, timewindow_queue_threshold, timewindow_queue_threshold, timewindow_max_outstanding);
					fprintf(stderr, "case 4 of class %i RT = %f = (ei*L = %f * %i ) >= L = %i && MAXO = %i < L\n",
							i, next_timewindow_RT, e_i, timewindow_queue_threshold, timewindow_queue_threshold, timewindow_max_outstanding);

					//next_timewindow_queue_thresholds[i] = next_timewindow_queue_thresholds[i];
					overload=0;
				}
				else
				{

					fprintf(stderr,"cannot decide 1\n");
					fprintf(stderr,"next_timewindow_RT %f\n",next_timewindow_RT);
					fprintf(stderr,"next_timewindow_X_lower %f\n",next_timewindow_X_lower);
					fprintf(stderr,"next_timewindow_X_upper %f\n",next_timewindow_X_upper);
					fprintf(stderr,"timewindow_queue_threshold %i\n",timewindow_queue_threshold);
					fprintf(stderr,"timewindow_max_outstanding %i\n", timewindow_max_outstanding);
					exit(-3);
				}

			}
			else
			{
				//overloaded case
				next_timewindow_RT = e_i*timewindow_max_outstanding;

				if (next_timewindow_RT<0.01)
				{
					next_timewindow_RT=0.01;
				}

				if (next_timewindow_X_lower_length < MAX(timewindow_ttl_completed * 0.9,1))//guard against tt_completed=0
				{
					if (next_timewindow_X_lower < 0.01)
					{
						next_timewindow_X_lower = 0.01;
					}
					fprintf(depthtrack, "case 5 of class %i X_ = %i < X*0.9 = %i; RT = %f (e_i*MAXO = %f * %i)\n",
							i, next_timewindow_X_lower_length ,timewindow_ttl_completed, next_timewindow_RT, e_i, timewindow_max_outstanding);
					fprintf(stderr, "case 5 of class %i X_ = %i < X*0.9 = %i; RT = %f (e_i*MAXO = %f * %i)\n",
							i, next_timewindow_X_lower_length ,timewindow_ttl_completed, next_timewindow_RT, e_i, timewindow_max_outstanding);

					next_timewindow_queue_thresholds[i]=MAX(next_timewindow_RT, next_timewindow_X_lower);
					overload=0;
				}
				else if (next_timewindow_X_upper_length >= timewindow_ttl_completed * 0.9)
				{
					fprintf(depthtrack, "case 6 of class %i X~ = %i > X*0.9 = %i; RT = %f (e_i*MAXO = %f * %i)\n",
							i, next_timewindow_X_upper_length, timewindow_ttl_completed, next_timewindow_RT, e_i, timewindow_max_outstanding);
					fprintf(stderr, "case 6 of class %i X~ = %i > X*0.9 = %i; RT = %f (e_i*MAXO = %f * %i)\n",
							i, next_timewindow_X_upper_length, timewindow_ttl_completed, next_timewindow_RT, e_i, timewindow_max_outstanding);

					next_timewindow_queue_thresholds[i]=twolsfqd_edf_infinite;

				}
				else
				{
					fprintf(stderr,"cannot decide 2\n");
					fprintf(stderr,"next_timewindow_X_lower_length: %i \n",next_timewindow_X_lower_length);
					fprintf(stderr,"timewindow_ttl_completed, %i \n", timewindow_ttl_completed);
					fprintf(stderr,"next_timewindow_X_upper_length %i\n", next_timewindow_X_upper_length);
					exit(-4);
				}

			}
			next_timewindow_queue_threshold = MIN(next_timewindow_queue_threshold, next_timewindow_queue_thresholds[i]);
			//find the smallest/most stringent queue length


		}

		//do a ceiling here from float to integer

		fprintf(depthtrack,"case~ The next timewindow threshold is found: %f->%i\n", next_timewindow_queue_threshold, (int)ceil(next_timewindow_queue_threshold));
		fprintf(stderr,"case~ The next timewindow threshold is found: %f->%i\n", next_timewindow_queue_threshold, (int)ceil(next_timewindow_queue_threshold));
		timewindow_queue_threshold = ceil(next_timewindow_queue_threshold);
		//============begins next time window==============


		struct timeval now;
		gettimeofday(&now, 0);
		timeradd(&now, &twolsfqd_timewindow_timeval,&current_time_window_end);
		timeradd(&current_time_window_end, &twolsfqd_timewindow_timeval,&next_time_window_end);


		timewindow_max_outstanding=0;
		timewindow_ttl_completed=0;
		timewindow_arrival=0;
		timewindow_arrival_deadline=0;
		timewindow_total_resp=0;

		for (i=0; i< num_apps; i++)
		{
			timewindow_edf_waiting[i].tv_sec = 0;
			timewindow_edf_waiting[i].tv_usec = 0;
			//timewindow_95_percentile[i] = 0;
			timewindow_completed[i]=0;
			PINT_llist_free(iedf_llist_resp[i], free_resp_item);
			iedf_llist_resp[i]=PINT_llist_new();
			timewindow_previous_arrival_deadlines[i]=timewindow_arrival_deadlines[i];
			timewindow_arrival_deadlines[i]=0;
		}

		if (overload==1)
		{
			fprintf(depthtrack,"The next timewindow is overloaded\n");
			fprintf(stderr,"The next timewindow is overloaded\n");
			twolsfqd_update_spareness();

			twolsfqd_dequeue_all(NULL);
		}
		else
		{
			fprintf(depthtrack,"The next timewindow is underloaded\n");

		}

		//if next window is overloaded

		//twolsfqd_update_spareness();
		//fprintf(stderr,"calling in time window\n");
		twolsfqd_edf_dequeue_all(-1);
		pthread_mutex_unlock(&twolsfqd_sfqd_queue_mutex);
	}

}

void twolsfqd_replenish()
{
	//it is replensih to FULL CREDITS whether its a timeout or a spareness change
	int i;
	for (i=0;i<num_apps;i++)
	{
		fprintf(depthtrack,"replenishing app %i which has %i left, with ", i, sarc_credits[i]);
		sarc_credits[i]=app_stats[i].app_rate * 1048576 /1000 * twolsfqd_replenish_time ;
		fprintf(depthtrack,"app %i is being replenished with %i credits\n",i, sarc_credits[i]);
		//how many slots are for this app

	}


	//twolsfqd_current_depth=(int)amt;

	//fprintf(depthtrack,"replenishing a total of %i\n",(int)amt);
	//twolsfqd_replenish_amount;
	char timestring[20];
	struct timeval now;
	gettimeofday(&now, 0);
	get_time_string(&now, timestring);
	last_replenish_time = now;
	//fprintf(stderr,"%s replenishing\n", timestring);

}


void* twolsfqd_time_replenish(void * arg)
{

	int ureplenishtime=twolsfqd_replenish_time*1000;//milli*1000=micro
	struct timeval diff;
	struct timeval time_already_passed;
	get_time_diff(&last_replenish_time, &time_already_passed);

	int useconds_to_sleep;
	while (1)
	{
		pthread_mutex_lock(&twolsfqd_sfqd_queue_mutex);
		//first sleep for at least replenish time amount
		//then check if during the sleep other events triggered replenishment and advanced last replenish time
		useconds_to_sleep = ureplenishtime- time_already_passed.tv_sec*1000000 - time_already_passed.tv_usec;
		if (useconds_to_sleep<=0)
		{
			twolsfqd_replenish();
			fprintf(stderr,"replenish\n");
			twolsfqd_dequeue_all(NULL);
			pthread_mutex_unlock(&twolsfqd_sfqd_queue_mutex);
		}
		else
		{
			pthread_mutex_unlock(&twolsfqd_sfqd_queue_mutex);
			//fprintf(stderr,"%s replenish Sleeping %i microseconds\n", log_prefix, useconds_to_sleep);
			usleep(useconds_to_sleep);
		}

		get_time_diff(&last_replenish_time, &time_already_passed);

	}


}

void twolsfqd_update_spareness()
{
	//fprintf(stderr,"updating spareness...\n");
	if (timewindow_queue_threshold*0.9> timewindow_current_queue_length)
	{
		//fprintf(stderr,"Lcurr= %i < (Lo= %i)*0.9\n", timewindow_current_queue_length,timewindow_queue_threshold);
		twolsfqd_replenish();
	}
}

//this is an invisible scheduler called by the rate regulator (SFQ)
//the upper level SFQ will be exposed to the system.

//the polling timeout will be the deadline-passed time since the start of the current time window
//for SARC elapse event - dispatching to lower level, drain the depth for the new time window
//IEDF means internal edf

/* a dispatch/replenish happens
 * 1. upon a time period Tsarc having elapsed since last replenishment
 * 2. upon a new arrival at the fifo queue with no available credits,
 *    while the spareness status indicatres that the storage utility has spare bandwidth available
 * 3. upon AVATAR changing the spareness status and indicating that spare bandwidth has become available
 *
 * */

/* for SFQ regulator:
 * 1. credits are merged credits for all classes
 * 2. the higher level is always open to accept new connections (no idling without accepting new I/Os)
 * 3. creidts determine the total number of requests dispatched to the EDF queue
 *
 * */


/* spareness changes (and reported to SFQ) only when:
 * 1. when requests are dispatched to the storage
 * 2. when the requests depart from the storage utility
 * 3. at the beginning of an overloaded time window
 *
 * */

/* dispatcher (EDF):
 * 1. when new requests arrive
 * 2. when requests depart from the storage utility upon completion
 * 3. at the beginning of each time window
 * this means we need a thread
 * The dispatching is not tagged with an outside event any more
 * So we should receive the request message into the buffer, and set the size to 0 for delaying the forward
 * in the meantime, the receiving end will not be polling any more data.
 * on the othe end, we can control when to dispatch, and the polling loop goes on again.
 * This 2-level behavior needs to muffle the ordinary completion event, because we don't have a fixed depth now,
 * although the completion event does mean something, it should call the edf's dispatcher instead of SFQ's
 *
 */

/* SFQ's queueing behavior queue based on credit
 * disptach will call edf's queueing method
 * SFQ's dispatch will also depend on timeout/threading
 *
 * */


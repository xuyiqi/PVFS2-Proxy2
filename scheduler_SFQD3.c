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
#include "scheduler_SFQD3.h"
#include "heap.h"
#include "iniparser.h"
#include "llist.h"
#include "performance.h"
#include "scheduler_main.h"
#include "config.h"
#include "cost_model_history.h"
extern int total_weight;
extern long long total_throughput;
extern struct socket_pool  s_pool;
extern char* log_prefix;
#define REDUCER 1
int sfqd3_default_weight=1;
extern long long small_io_size;
int* sfqd3_depths;
int* sfqd3_current_depths;
int* sfqd3_virtual_times;
int* sfqd3_last_finish_tags;

FILE* sfqd3_depthtrack;
struct heap ** sfqd3_heap_queues;
int sfqd3_item_id=0;
extern dictionary * dict;
extern struct app_statistics * app_stats;
int print_ip_app_item(void* item);
int sfqd3_add_ttl_tp(int this_amount, int app_index)
{
	total_throughput+=this_amount;
    app_stats[app_index].byte_counter=app_stats[app_index].byte_counter+this_amount;
    app_stats[app_index].app_throughput=app_stats[app_index].app_throughput+this_amount;
}

int sfqd3_calculate_diff(int app_index)
{
	return app_stats[app_index].app_throughput-total_throughput*app_stats[app_index].app_weight/total_weight;
}

const struct scheduler_method sch_sfqd3 = {
    .method_name = SFQD3_SCHEDULER,
    .work_conserving = 0,
    .sch_initialize = sfqd3_init,
    .sch_finalize = NULL,
    .sch_enqueue = sfqd3_enqueue,
    .sch_dequeue = sfqd3_dequeue,
    .sch_load_data_from_config = sfqd3_load_data_from_config,
    .sch_update_on_request_completion = sfqd3_update_on_request_completion,
    .sch_get_scheduler_info = sfqd3_get_scheduler_info,
    .sch_is_idle = sfqd3_is_idle,
    .sch_current_size = sfqd3_current_size,
    .sch_add_ttl_throughput = sfqd3_add_ttl_tp,
    .sch_calculate_diff = sfqd3_calculate_diff,
    .sch_self_dispatch = 0
};


int sfqd3_init()
{
	int i;
	sfqd3_heap_queues=(struct heap**)malloc(num_apps*sizeof(struct heap*));
	sfqd3_last_finish_tags=(int*)malloc(num_apps*sizeof(int));

	sfqd3_current_depths=(int*)malloc(num_apps*sizeof(int));
	sfqd3_virtual_times=(int*)malloc(num_apps*sizeof(int));


	for (i=0;i<num_apps;i++)
	{
		sfqd3_heap_queues[i]=(struct heap *)malloc(sizeof(struct heap));
		heap_init(sfqd3_heap_queues[i]);
		sfqd3_last_finish_tags[i]=0;
		sfqd3_current_depths[i]=0;
		sfqd3_virtual_times[i]=0;

	}

	char* deptht=(char*)malloc(sizeof(char)*40);
	snprintf(deptht, 40, "%s.depthtrack.txt", log_prefix);
	sfqd3_depthtrack = fopen(deptht,"w");

}

//weight_mapping-ip-app mapping
//app-weight mapping
int sfqd3_packet_cmp(struct heap_node* _a, struct heap_node* _b)
{
	struct generic_queue_item *g_a, *g_b;
	struct sfqd3_queue_item *a, *b;

	g_a = (struct generic_queue_item*) heap_node_value(_a);
	g_b = (struct generic_queue_item*) heap_node_value(_b);
	a = (struct sfqd3_queue_item *)g_a->embedded_queue_item;
	b = (struct sfqd3_queue_item *)g_b->embedded_queue_item;
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

	/*#
	 * a->socket_info
	 *
	 *
	 * #*/
}

int sfqd3_add_item(struct heap* heap, struct generic_queue_item* item)
{
	struct heap_node* hn = malloc(sizeof(struct heap_node));
	heap_node_init(hn, item);
	int dispatched=0;


	heap_insert(sfqd3_packet_cmp, heap, hn);
	return dispatched;
}
int sfqd3_is_idle()
{

	int i;
	int items=0;
	for (i=0;i<num_apps;i++)
	{
		items+=sfqd3_heap_queues[i]->all_count;
	}
	return items;
}



//heap_empty(heap_queue);
	//obsolete for sfqd2!


int sfqd3_current_size(struct request_state * original_rs, long long actual_data_file_size)
{

	struct generic_queue_item * current_item = original_rs->current_item;


	struct sfqd3_queue_item * sfqd_item = (struct sfqd3_queue_item * )(current_item->embedded_queue_item);

	sfqd_item->data_file_size=actual_data_file_size;

	int strip_size=sfqd_item->strip_size;
	int server_nr = sfqd_item->server_nr;
	int server_count= sfqd_item->server_count;

	long long offset=sfqd_item->file_offset;
	//update expected receivables for current item!
	long long ask_size= sfqd_item->aggregate_size;

	int my_shared_size = get_my_share(strip_size, server_count, offset, ask_size, server_nr);
    //fprintf(stderr,"current item adjusted to %i\n",my_shared_size);
    sfqd_item->task_size=my_shared_size;

    return my_shared_size;

}


int sfqd3_enqueue(struct socket_info * si, struct pvfs_info* pi)
{
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
	//fprintf(stderr,"%s: [Jit Tracking] IP: %s:%i TAG %i\n",log_prefix, ip,port, tag);
	int d_socket=s_pool.socket_state_list[d_socket_index].socket;
	int socket_tag = tag;

	int app_index= s_pool.socket_state_list[r_socket_index].app_index;
	//Dprintf(D_CACHE, "app_index is %i for ip %s\n", app_index, ip);
	int weight = s_pool.socket_state_list[r_socket_index].weight;


	//Dprintf(D_CACHE,"weight got from request socket is %i,ip %s\n",weight,ip);
	int start_tag=MAX(sfqd3_virtual_times[app_index], sfqd3_last_finish_tags[app_index]);//work-conserving
	//Dprintf(D_CACHE, "virtual time is %i, last finish tag of app %i is %i\n", virtual_time, app_index+1, last_finish_tags[app_index]);
	//int start_tag=last_finish_tags[app_index];//non-work-conserving
	int cost;
	int reducer, finish_tag;
	if (cost_model==COST_MODEL_NONE)
	{
		//reducer=REDUCER;
		cost=length;
		finish_tag = start_tag+cost/weight/REDUCER;
	}
	else
	{
		//cost=length;//testing mode, just want output


		cost=length;
				get_expected_resp(app_index, r_socket_index, io_type);//length*(sfqd_current_depth+1);
				finish_tag = start_tag+cost/weight/REDUCER;
		//fprintf (stderr, "Throughput is translated into %i / %i = %i\n", length, cost, length/cost);
		//cost = length / cost;
		//fprintf(stderr,"Max resp time for this kind of IO for app %i is cost = %i\n", app_index, cost);
		//finish_tag = start_tag+cost*10/weight;
	}

	sfqd3_last_finish_tags[app_index]=finish_tag;
	//Dprintf(D_CACHE, "my previous finish tag is updated to %i, weight %i\n",finish_tag,s_pool.socket_state_list[r_socket_index].weight);


	struct generic_queue_item * generic_item =  (struct generic_queue_item * )malloc(sizeof(struct generic_queue_item));
	struct sfqd3_queue_item * item = (struct sfqd3_queue_item *)(malloc(sizeof(struct sfqd3_queue_item)));
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
	generic_item->item_id=sfqd3_item_id++;
	//fprintf(stderr,"item %li strip size is %i\n",generic_item->item_id,item->strip_size);
/*	char bptr[20];
    struct timeval tv;
    time_t tp;
    gettimeofday(&tv, 0);
    tp = tv.tv_sec;
    strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
    sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);
    s_pool.socket_state_list[r_socket_index].last_work_time=tv;*/

	//Dprintf(D_CACHE, "adding start_tag:%i finish_tag:%i weight:%i app:%i ip:%s\n",start_tag, finish_tag, weight,s_pool.socket_state_list[r_socket_index].app_index+1,s_pool.socket_state_list[r_socket_index].ip);

	//Dprintf(D_CACHE, "adding to %ith socket %i\n",r_socket_index, s_pool.socket_state_list[r_socket_index].socket);
	item->data_port=port;
	item->data_ip=ip;
	item->task_size=length;
	//fprintf(stderr,"task size:%i/%i\n",length, pi->aggregate_size);

	item->data_socket=d_socket;
	item->socket_tag=socket_tag;
	item->app_index=app_index;
	item->stream_id=app_stats[app_index].stream_id++;
	item->got_size=0;
	item->request_socket=s_pool.socket_state_list[r_socket_index].socket;
	item->io_type=io_type;
	item->buffer=request;
	item->buffer_size=req_size;

	if 	(length<=65536)
	{
		small_io_size+=length;
	}


	generic_item->socket_data=si;

	//Dprintf(D_CACHE, "[INITIALIZE]ip:%s port:%i tag:%i\n", item->data_ip, item->data_port, item->socket_tag);


	app_stats[app_index].req_come+=1;
	if ( app_stats[app_index].diff > 10240  || sfqd3_current_depths[app_index] >=sfqd3_depths[app_index] || !heap_empty(sfqd3_heap_queues[app_index]))
			//if i'm over-using the resources, i will rest for a while and wait for others...
			//fail..
	{		//fprintf(stderr," app %i is resting for a while...checked depth %i>=%i, diff %i>10240 \n", app_index, sfqd3_current_depths[app_index],sfqd3_depths[app_index],app_stats[app_index].diff);
			//app_stats[l].rest=1;
		sfqd3_add_item(sfqd3_heap_queues[app_index],generic_item);
		return 0;
	}

	int dispatched=1;
	dispatched=1;

	sfqd3_add_item(sfqd3_heap_queues[app_index],generic_item);

	//Dprintf(D_CACHE, "[IN QUEUE]ip:%s port:%i tag:%i\n", item->data_ip, item->data_port, item->socket_tag);
	//Dprintf(D_CACHE,"current queue has %i items\n",heap_queue->all_count);

//	if (pthread_mutex_lock (&counter_mutex)!=0)
	{
		//Dprintf(D_CACHE, "error locking when incrementing counter in additional counters\n");
	}
	//add addition counter


	//Dprintf(D_CACHE, "Performance++req_come:%i,%i from %s\n",req_come[app_index],app_index+1, s_pool.socket_state_list[r_socket_index].ip);
//	if (pthread_mutex_unlock (&counter_mutex)!=0)
	{
		//Dprintf(D_CACHE, "error unlocking when done incrementing counter in additional counters\n");
	}
	//fprintf(stderr,"enqueue depth:%i\n",current_depth);
	return dispatched;


}

struct generic_queue_item * sfqd3_dequeue(struct dequeue_reason r)
{

	return sfqd3_get_next_request(r);
}

void sfqd3_get_scheduler_info()
{
	//fprintf(stderr,"depth remains at %i", current_depth);
	//fprintf(stderr," current queue has %i items\n",heap_queue->all_count);
}

void sfqd3_fill_proxy_message(void* arg)
{


}

int sfqd3_update_on_request_completion(void* arg)
{

	struct complete_message * complete = (struct complete_message *)arg;
	//struct proxy_message * request = (struct proxy_message)(complete->proxy_message);
	struct generic_queue_item * current_item = (complete->current_item);

	struct sfqd3_queue_item * sfqd_item = (struct sfqd3_queue_item * )(current_item->embedded_queue_item);
	sfqd_item->got_size+=(complete->complete_size);
/*
	fprintf(stderr,"[Current Item %s:%i]:%i/%i\n",
			sfqd_item->data_ip, sfqd_item->data_port,
			sfqd_item->got_size,sfqd_item->task_size);
			*/
	//fprintf(stderr,"%i still got %i bytes to send.\n",sfqd_item->data_socket_index, s_pool.buffer_sizes[sfqd_item->data_socket_index]);



	if (sfqd_item->task_size==sfqd_item->got_size)
	{
		//fprintf(stderr,"completed:%i\n",sfqd_item->got_size);

		//lock

		return sfqd_item->got_size;
	}
	else if (sfqd_item->task_size < sfqd_item->got_size)
	{
		sfqd_item->over_count++;
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

/* *
 * SFQ:
 *
 * Start Tag = max (Virtual Time, Previous Finish Time of current stream/app)
 * Virtual Time = Start Tag of last dispatched (currently being dispatched) request.
 * Finish Tag = Start Tag  + length_of_request/share(or weight, priority)
 *
 * From the client:
 * Write I/O (meaningful length)-> interposed / held to queue
 * Read Req  (meaningful length)-> interposed / held to queue
 * Other Messages -> assigned l_req of 0, ignored
 *
 * if mode==WRITE, socket_index is the same from request (client)
 * if mode==READ, socket_index is the counter part's index (data from server as opposed from the source of request)
 *
 * */

//on closing, remove certain items from the queue
struct generic_queue_item* sfqd3_get_next_request(struct dequeue_reason r)
{
	/*
	 * Called after add_request to determine if it's dispatched immediately
	 *
	 * Update last_serviced_request time
	 * */
	//last_io_type works as a app_index now!

	struct generic_queue_item * next;
	int change_app_index=-1;
	//free up current item
	int i;
	int empty=1;



	next_retry:

	if (r.event==COMPLETE_IO)
	{
		//struct generic_queue_item * last_item =  r.item;
		//struct sfqd3_queue_item * last_s_item= (struct sfqd3_queue_item *)last_item->embedded_queue_item;
		//fprintf(stderr,"%s app %i finished...start tag %i\n", log_prefix, r.last_app_index, last_s_item->start_tag);
		sfqd3_current_depths[r.last_app_index]--;
	}
	if (r.event==DIFF_CHANGE)
	{
		//it also means other app's complete, so we don't change the depth here for the completed application

		change_app_index = r.last_app_index;


		if (heap_empty(sfqd3_heap_queues[r.last_app_index]))
		{
			return NULL;
		}
		/*
		 * The dispatch here will only consider r.last_app_index
		 */

		if (sfqd3_current_depths[r.last_app_index]<sfqd3_depths[r.last_app_index])
		{
			//fprintf(stderr,"not empty\n");
			//fprintf(stderr, "app queue: %i has %i items left, all_exist=%i\n",l+1,heap_queue->count[l],first_receive);
			struct heap_node * hn  = heap_take(sfqd3_packet_cmp, sfqd3_heap_queues[r.last_app_index]);
			next  = heap_node_value(hn);
			struct sfqd3_queue_item *next_item = (struct sfqd3_queue_item *)(next->embedded_queue_item);
			sfqd3_virtual_times[r.last_app_index]=next_item->start_tag;

			sfqd3_current_depths[r.last_app_index]++;
			app_stats[r.last_app_index].dispatched_requests+=1;
			return next;
		}
		else
		{
			return NULL;
		}

	}
	if (r.event==NEW_IO)
	{
		//fprintf(stderr,"%s app %i new... \n ", log_prefix, r.last_app_index);
	}


	for (i=0;i<num_apps;i++)
	{
		if (!heap_empty(sfqd3_heap_queues[i]))
		{
			empty=0;
			break;
		}

	}
	if (empty==1)
	{
		//fail...


		//cannot happen herer if it's a new request or that must be dispatched immediately. won't modify depth incorrectly.

		return NULL;

	}
	int l;
	int smallest_start_tag=INT_MAX;
	int open_app=-1;
	for (l=0;l<num_apps;l++)
	{

		if ( app_stats[l].diff > 10240)
		{
			//if i'm over-using the resources, i will rest for a while and wait for others...
			//fail..
			//fprintf(stderr,"%s app %i is resting for a while...diff %i in dispatch out ttl %i\n", log_prefix, l, app_stats[l].diff, total_throughput);
			continue;
		}
		//fprintf(stderr,"checking app %i\n",l);
		if (sfqd3_current_depths[l]<sfqd3_depths[l] && !heap_empty(sfqd3_heap_queues[l]))
		{
			//fprintf(stderr,"not empty\n");
		//fprintf(stderr, "app queue: %i has %i items left, all_exist=%i\n",l+1,heap_queue->count[l],first_receive);
			struct heap_node * hn  = heap_peek(sfqd3_packet_cmp, sfqd3_heap_queues[l]);
			next  = heap_node_value(hn);
			struct sfqd3_queue_item *next_item = (struct sfqd3_queue_item *)(next->embedded_queue_item);
			if (next_item->start_tag<smallest_start_tag)
			{
				smallest_start_tag=next_item->start_tag;
				open_app=l;
			}
			//fprintf(stderr,"start tag of app %i is %i\n", l, next_item->start_tag);
		}
		else
		{
			//fprintf(stderr,"depth full %i %i or no items %i app %i\n", sfqd3_current_depths[l], sfqd3_depths[l], heap_empty(sfqd3_heap_queues[l]), l);
		}


		//check depth again
	}


	if (open_app<0)
	{
		//fail...
		//fprintf(stderr,"queue has items, but smallest tag cannot be found!\n");

		return NULL;
		//if the other application has not started...this app exceeds limit, then no smallest tag will be returned.
	}


	sfqd3_current_depths[open_app]++;


	struct heap_node * hn  = heap_take(sfqd3_packet_cmp, sfqd3_heap_queues[open_app]);
	next  = heap_node_value(hn);
	struct sfqd3_queue_item * next_item = (struct sfqd3_queue_item *)(next->embedded_queue_item);
	sfqd3_virtual_times[open_app]=next_item->start_tag;


	int app_index=next->socket_data->app_index;
	app_stats[app_index].req_go+=1;


	app_stats[app_index].dispatched_requests+=1;
	return next;
}

int sfqd3_load_data_from_config (dictionary * dict)
{
	sfqd3_depths=(int*)malloc(num_apps*sizeof(int));
	sfqd3_depths[0]=iniparser_getint(dict, "SFQD3:depth", 8);
	sfqd3_depths[1]=iniparser_getint(dict, "SFQD3:depth", 8);
	fprintf(stderr,"SFQD using depth_large:%i\n",sfqd3_depths[1]);


}

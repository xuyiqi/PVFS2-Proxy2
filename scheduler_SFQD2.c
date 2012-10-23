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
#include "scheduler_SFQD2.h"
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
#define REDUCER 1024
int sfqd2_default_weight=1;

long long small_io_size=0;
long long large_io_size=0;

int sfqd2_depth_large=1;
int sfqd2_depth_small=100;

int sfqd2_current_depth_large=0;
int sfqd2_current_depth_small=0;

int sfqd2_virtual_time_large=0;
int sfqd2_virtual_time_small=0;

int* sfqd2_last_finish_tags_large;
int* sfqd2_last_finish_tags_small;

FILE* sfqd2_depthtrack;

struct heap * sfqd2_heap_queue_large;
struct heap * sfqd2_heap_queue_small;
int sfqd2_item_id=0;
extern struct app_statistics * app_stats;

int print_ip_app_item(void* item);

int sfqd2_add_ttl_tp(int this_amount, int app_index)
{
	total_throughput+=this_amount;
    app_stats[app_index].byte_counter=app_stats[app_index].byte_counter+this_amount;
    app_stats[app_index].app_throughput=app_stats[app_index].app_throughput+this_amount;


}

const struct scheduler_method sch_sfqd2 = {
    .method_name = SFQD2_SCHEDULER,
    .work_conserving=1,
    .sch_initialize = sfqd2_init,
    .sch_finalize = NULL,
    .sch_enqueue = sfqd2_enqueue,
    .sch_dequeue = sfqd2_dequeue,
    .sch_load_data_from_config = sfqd2_load_data_from_config,
    .sch_update_on_request_completion = sfqd2_update_on_request_completion,
    .sch_get_scheduler_info = sfqd2_get_scheduler_info,
    .sch_is_idle = sfqd2_is_idle,
    .sch_current_size = sfqd2_current_size,
    .sch_add_ttl_throughput = sfqd2_add_ttl_tp,
    .sch_self_dispatch = 0
};



int sfqd2_init()
{
	sfqd2_heap_queue_large=(struct heap*)malloc(sizeof(struct heap));
	sfqd2_heap_queue_small=(struct heap*)malloc(sizeof(struct heap));

	heap_init(sfqd2_heap_queue_large);
	heap_init(sfqd2_heap_queue_small);

	sfqd2_last_finish_tags_large=(int*)malloc(num_apps*sizeof(int));
	sfqd2_last_finish_tags_small=(int*)malloc(num_apps*sizeof(int));

	int i;
	for (i=0;i<num_apps;i++)
	{
		sfqd2_last_finish_tags_large[i]=0;
		sfqd2_last_finish_tags_small[i]=0;
	}

	char* deptht=(char*)malloc(sizeof(char)*40);
	snprintf(deptht, 40, "%s.depthtrack.txt", log_prefix);
	sfqd2_depthtrack = fopen(deptht,"w");

}

//weight_mapping-ip-app mapping
//app-weight mapping
int sfqd2_packet_cmp(struct heap_node* _a, struct heap_node* _b)
{
	struct generic_queue_item *g_a, *g_b;
	struct sfqd2_queue_item *a, *b;

	g_a = (struct generic_queue_item*) heap_node_value(_a);
	g_b = (struct generic_queue_item*) heap_node_value(_b);
	a = (struct sfqd2_queue_item *)g_a->embedded_queue_item;
	b = (struct sfqd2_queue_item *)g_b->embedded_queue_item;
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

int sfqd2_add_item(struct heap* heap, struct generic_queue_item* item)
{
	struct heap_node* hn = malloc(sizeof(struct heap_node));
	heap_node_init(hn, item);
	int dispatched=0;


	heap_insert(sfqd2_packet_cmp, heap, hn);
	return dispatched;
}
int sfqd2_is_idle()
{

	return sfqd2_heap_queue_large->all_count+sfqd2_heap_queue_small->all_count;
			//heap_empty(heap_queue);
	//obsolete for sfqd2!
}

int sfqd2_current_size(struct request_state * original_rs, long long actual_data_file_size)
{

	struct generic_queue_item * current_item = original_rs->current_item;

	struct sfqd2_queue_item * sfqd_item = (struct sfqd2_queue_item * )(current_item->embedded_queue_item);

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


int sfqd2_enqueue(struct socket_info * si, struct pvfs_info* pi)
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
	int * sfqd2_last_finish_tags;
	struct heap * sfqd2_heap_queue;
	int sfqd2_virtual_time;
	if 	(length<=65536)
	{
		sfqd2_last_finish_tags = sfqd2_last_finish_tags_small;
		sfqd2_heap_queue = sfqd2_heap_queue_small;
		small_io_size+=length;
		sfqd2_virtual_time=sfqd2_virtual_time_small;
		//fprintf(stderr," small io is %lli\n",small_io_size);

	}
	else
	{
		sfqd2_last_finish_tags = sfqd2_last_finish_tags_large;
		sfqd2_heap_queue = sfqd2_heap_queue_large;
		sfqd2_virtual_time=sfqd2_virtual_time_large;
	}
	//Dprintf(D_CACHE,"weight got from request socket is %i,ip %s\n",weight,ip);
	int start_tag=MAX(sfqd2_virtual_time, sfqd2_last_finish_tags[app_index]);//work-conserving
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

	sfqd2_last_finish_tags[app_index]=finish_tag;
	//Dprintf(D_CACHE, "my previous finish tag is updated to %i, weight %i\n",finish_tag,s_pool.socket_state_list[r_socket_index].weight);


	struct generic_queue_item * generic_item =  (struct generic_queue_item * )malloc(sizeof(struct generic_queue_item));
	struct sfqd2_queue_item * item = (struct sfqd2_queue_item *)(malloc(sizeof(struct sfqd2_queue_item)));
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
	generic_item->item_id=sfqd2_item_id++;
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



	generic_item->socket_data=si;

	//Dprintf(D_CACHE, "[INITIALIZE]ip:%s port:%i tag:%i\n", item->data_ip, item->data_port, item->socket_tag);
	sfqd2_add_item(sfqd2_heap_queue,generic_item);

	int dispatched=0;


	if (length<=65536)
	{
		if (sfqd2_current_depth_small<sfqd2_depth_small)//heap_empty(heap_queue))
		{
			dispatched=1;
			sfqd2_current_depth_small++;
			//this means the queue is empty before adding item to it.
		}
		if (dispatched)
		{
			//Dprintf(D_CACHE, "current_depth increased to %i\n",current_depth);
			//virtual_time=start_tag;
			//Dprintf(D_CACHE,"[CLOCK] virtual time is updated to %i\n",virtual_time);
		}

	}
	else
	{
		if (sfqd2_current_depth_large<sfqd2_depth_large)//heap_empty(heap_queue))
		{
			dispatched=1;
			sfqd2_current_depth_large++;
			//this means the queue is empty before adding item to it.
		}
		if (dispatched)
		{
			//Dprintf(D_CACHE, "current_depth increased to %i\n",current_depth);
			//virtual_time=start_tag;
			//Dprintf(D_CACHE,"[CLOCK] virtual time is updated to %i\n",virtual_time);
		}
	}

	//Dprintf(D_CACHE, "[IN QUEUE]ip:%s port:%i tag:%i\n", item->data_ip, item->data_port, item->socket_tag);
	//Dprintf(D_CACHE,"current queue has %i items\n",heap_queue->all_count);

//	if (pthread_mutex_lock (&counter_mutex)!=0)
	{
		//Dprintf(D_CACHE, "error locking when incrementing counter in additional counters\n");
	}
	//add addition counter
	app_stats[app_index].req_come+=1;

	//Dprintf(D_CACHE, "Performance++req_come:%i,%i from %s\n",req_come[app_index],app_index+1, s_pool.socket_state_list[r_socket_index].ip);
//	if (pthread_mutex_unlock (&counter_mutex)!=0)
	{
		//Dprintf(D_CACHE, "error unlocking when done incrementing counter in additional counters\n");
	}
	//fprintf(stderr,"enqueue depth:%i\n",current_depth);
	return dispatched;


}

struct generic_queue_item * sfqd2_dequeue(struct dequeue_reason r)
{

	return sfqd2_get_next_request(r);
}

void sfqd2_get_scheduler_info()
{
	//fprintf(stderr,"depth remains at %i", current_depth);
	//fprintf(stderr," current queue has %i items\n",heap_queue->all_count);
}

void sfqd2_fill_proxy_message(void* arg)
{


}

int sfqd2_update_on_request_completion(void* arg)
{

	struct complete_message * complete = (struct complete_message *)arg;
	//struct proxy_message * request = (struct proxy_message)(complete->proxy_message);
	struct generic_queue_item * current_item = (complete->current_item);

	struct sfqd2_queue_item * sfqd_item = (struct sfqd2_queue_item * )(current_item->embedded_queue_item);
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
struct generic_queue_item* sfqd2_get_next_request(struct dequeue_reason r)
{
	/*
	 * Called after add_request to determine if it's dispatched immediately
	 *
	 * Update last_serviced_request time
	 * */

	int last_io_type=r.complete_size;
	struct generic_queue_item * next;
	struct heap * sfqd2_heap_queue ;
	int * sfqd2_virtual_time, * sfqd2_current_depth, *sfqd2_depth;
	//free up current item
	if (last_io_type<=65536)
	{
		sfqd2_heap_queue = sfqd2_heap_queue_small;
		sfqd2_virtual_time = & sfqd2_virtual_time_small;
		sfqd2_depth = &sfqd2_depth_small;
		sfqd2_current_depth = &sfqd2_current_depth_small;

	}
	else
	{
		sfqd2_heap_queue = sfqd2_heap_queue_large;
		sfqd2_virtual_time = & sfqd2_virtual_time_large;
		sfqd2_depth = &sfqd2_depth_large;
		sfqd2_current_depth = &sfqd2_current_depth_large;
	}
	next_retry:
	if (!heap_empty(sfqd2_heap_queue))
	{

		struct heap_node * hn  = heap_take(sfqd2_packet_cmp, sfqd2_heap_queue);
		next  = heap_node_value(hn);
		struct sfqd2_queue_item *next_item = (struct sfqd2_queue_item *)(next->embedded_queue_item);

		//current_node = hn;
		*sfqd2_virtual_time=next_item->start_tag;


		//add addition counter
		int app_index=next->socket_data->app_index;
		app_stats[app_index].req_go+=1;

		app_stats[app_index].dispatched_requests+=1;


		return next;
	}
	else
	{
		//Dprintf(D_CACHE, "app queue: no job found!, all_exist=%i\n",first_receive);

		//fprintf(stderr,"dequeue depth:%i\n",current_depth);

		(*sfqd2_current_depth)--;

		return NULL;
	}
}

int sfqd2_load_data_from_config (dictionary * dict)
{
	sfqd2_depth_large=iniparser_getint(dict, "SFQD2:depth" ,sfqd2_depth_large);
	fprintf(stderr,"SFQD using depth_large:%i\n",sfqd2_depth_large);

}

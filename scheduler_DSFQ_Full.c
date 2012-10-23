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
#include "scheduler_SFQD.h"
#include "heap.h"
#include "iniparser.h"
#include "llist.h"
#include "performance.h"
#include "scheduler_main.h"
#include "config.h"
#include "cost_model_history.h"
int sfqdfull_sorted=0;
extern struct socket_pool  s_pool;
extern char* log_prefix;
#define REDUCER 1024
int sfqdfull_default_weight=1;
int sfqdfull_depth=1;
int sfqdfull_current_depth=0;
int sfqdfull_virtual_time=0;
int* sfqdfull_last_finish_tags;
int* sfqdfull_list_queue_count;
extern long long total_throughput;
extern int finished[2];
extern int completerecv[2];
extern int completefwd[2];
#define SFQD_FULL_USE_HEAP_QUEUE 0

struct heap * sfqdfull_heap_queue;
PINT_llist_p sfqdfull_llist_queue;
int sfqdfull_item_id=0;

/* a simple version of local SFQD who takes meta-data ops as well
 * Full means all the commands are probably queueed now
 *
 * This also need the support form the framework - layer who also calls
 * enqueue when non-IOs arrive. So we need a tag in the scheduler to
 * indicate that.
 *
 * */
int print_ip_app_item(void* item);

int sfqdfull_add_ttl_tp(int this_amount, int app_index)
{
	total_throughput+=this_amount;
    app_stats[app_index].byte_counter=app_stats[app_index].byte_counter+this_amount;
    app_stats[app_index].app_throughput=app_stats[app_index].app_throughput+this_amount;
}

const struct scheduler_method sch_sfqd_full = {
    .method_name = SFQD_FULL_SCHEDULER,
    .work_conserving = 1,
    .sch_initialize = sfqdfull_init,
    .sch_finalize = NULL,
    .sch_enqueue = sfqdfull_enqueue,
    .sch_dequeue = sfqdfull_dequeue,
    .sch_load_data_from_config = sfqdfull_load_data_from_config,
    .sch_update_on_request_completion = sfqdfull_update_on_request_completion,
    .sch_get_scheduler_info = sfqdfull_get_scheduler_info,
    .sch_is_idle = sfqdfull_is_idle,
 //   .sch_current_size = sfqdfull_current_size,
    .sch_add_ttl_throughput = sfqdfull_add_ttl_tp,
    .sch_self_dispatch = 0, //i'm work-conserving, dispatched by the framework instead of within threads of the scheduler
    .sch_accept_meta =1
};



int sfqdfull_init()
{
	if (SFQD_FULL_USE_HEAP_QUEUE==1)
	{
		sfqdfull_heap_queue=(struct heap*)malloc(sizeof(struct heap));

		heap_init(sfqdfull_heap_queue);
	}
	else
	{
		sfqdfull_llist_queue=PINT_llist_new();
		assert(sfqdfull_llist_queue!=NULL);
		sfqdfull_list_queue_count=(int*)malloc(num_apps*sizeof(int));
	}

	sfqdfull_last_finish_tags=(int*)malloc(num_apps*sizeof(int));


	int i;
	for (i=0;i<num_apps;i++)
	{
		sfqdfull_list_queue_count[i]=0;
		//last_finish_tags[i]=0; do not forget!
	}
	char* deptht=(char*)malloc(sizeof(char)*40);
	snprintf(deptht, 40, "%s.depthtrack.txt", log_prefix);
	depthtrack = fopen(deptht,"w");//

}

//function used by heap to compare between two elements
int sfqdfull_packet_cmp(struct heap_node* _a, struct heap_node* _b)
{
	struct generic_queue_item *g_a, *g_b;
	struct sfqdfull_queue_item *a, *b;

	g_a = (struct generic_queue_item*) heap_node_value(_a);
	g_b = (struct generic_queue_item*) heap_node_value(_b);
	a = (struct sfqdfull_queue_item *)g_a->embedded_queue_item;
	b = (struct sfqdfull_queue_item *)g_b->embedded_queue_item;


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

int sfqdfull_add_item_to_queue(struct generic_queue_item* item)
{
	int dispatched=0;

	if (SFQD_FULL_USE_HEAP_QUEUE==1)
	{

		struct heap_node* hn = malloc(sizeof(struct heap_node));
		heap_node_init(hn, item);
		heap_insert(sfqdfull_packet_cmp, sfqdfull_heap_queue, hn);

	}
	else
	{
		if  (PINT_llist_add_to_tail(
				sfqdfull_llist_queue,
		    item))
		{

			fprintf(stderr,"sfqd_full queue insertion error!\n");
			exit(-1);
		}


	}
	return dispatched;
}
int sfqdfull_is_idle()
{
	if (SFQD_FULL_USE_HEAP_QUEUE==1)

		return sfqdfull_heap_queue->all_count;
			//heap_empty(heap_queue);
	else
		return PINT_llist_empty(sfqdfull_llist_queue);
}

int sfqdfull_current_size(struct request_state * original_rs, long long actual_data_file_size)
{

	struct generic_queue_item * current_item = original_rs->current_item;


	struct sfqdfull_queue_item * sfqdfull_item = (struct sfqdfull_queue_item * )(current_item->embedded_queue_item);

	sfqdfull_item->data_file_size=actual_data_file_size;

	int strip_size=sfqdfull_item->strip_size;
	int server_nr = sfqdfull_item->server_nr;
	int server_count= sfqdfull_item->server_count;

	long long offset=sfqdfull_item->file_offset;
	//update expected receivables for current item!
	long long ask_size= sfqdfull_item->aggregate_size;
	int my_shared_size = get_my_share(strip_size, server_count, offset, ask_size, server_nr);

    //fprintf(stderr,"current item adjusted to %i\n",my_shared_size);
    sfqdfull_item->task_size=my_shared_size;
    /*
     *
     * !!!remember, you can change last_finish_tag?//unless you use a linked list/flat format data structure that supports re-ordering
     *
     * */

    return my_shared_size;

}

int sfqdfull_get_finish_tag(int length, int weight, int start_tag)
{
	int cost, finish_tag;
	if (cost_model==COST_MODEL_NONE)
	{
		//reducer=REDUCER;
		cost=length;
		if (weight*REDUCER>cost)
		{
			finish_tag = start_tag+1;
		}
		else
		{
			finish_tag = start_tag + cost/weight/REDUCER;
		}
	}
	else
	{
		//cost=length;//testing mode, just want output
		cost=length;
		//get_expected_resp(app_index, r_socket_index, io_type);//length*(sfqd_current_depth+1);

		if (weight*REDUCER>cost)
		{
			finish_tag = start_tag+1;
		}
		else
		{
			finish_tag = start_tag + cost/weight/REDUCER;
		}
	}
	return finish_tag;
}

int sfqdfull_enqueue(struct socket_info * si, struct pvfs_info* pi)
{
	struct timeval now;
	gettimeofday(&now, 0);
	sfqdfull_sorted=0;
	switch (pi->op){
	case PVFS_SERV_SMALL_IO:
	case PVFS_SERV_IO:
		fprintf(stderr, "IO type %s\n", ops[pi->op]);
		break;

	default:
		fprintf(stderr, "Meta type %s, code %i\n", ops[pi->op], pi->op);


		break;
	}
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

	int start_tag=MAX(sfqdfull_virtual_time, sfqdfull_last_finish_tags[app_index]);//work-conserving

	sfqdfull_list_queue_count[app_index]++;

	int cost;
	int reducer, finish_tag;
	finish_tag = start_tag+1;
			//get_finish_tag(length, weight,start_tag);
	sfqdfull_last_finish_tags[app_index]=finish_tag;

	struct generic_queue_item * generic_item =  (struct generic_queue_item * )malloc(sizeof(struct generic_queue_item));
	struct sfqd_queue_item * item = (struct sfqd_queue_item *)(malloc(sizeof(struct sfqd_queue_item)));
	generic_item->embedded_queue_item=item;

	item->queuedtime = now;
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
	generic_item->item_id=sfqdfull_item_id++;

	char bptr[20];
    struct timeval tv;
    time_t tp;
    gettimeofday(&tv, 0);
    tp = tv.tv_sec;
    strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
    sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);
    //s_pool.socket_state_list[r_socket_index].last_work_time=tv;

	item->data_port=port;
	item->data_ip=ip;
	item->request_port= s_pool.socket_state_list[r_socket_index].port;
	item->task_size=length;
	//fprintf(stderr,"task size:%i/%i\n",length, pi->aggregate_size);

	//fprintf(depthtrack, "%s %s offset %lli size %i %s:%i, %i\n", log_prefix, bptr, pi->req_offset, length,
		//	s_pool.socket_state_list[r_socket_index].ip,s_pool.socket_state_list[r_socket_index].port, io_type);
	item->data_socket=d_socket;
	item->socket_tag=socket_tag;
	item->app_index=app_index;
	item->stream_id=app_stats[app_index].stream_id++;
	item->got_size=0;
	item->request_socket=s_pool.socket_state_list[r_socket_index].socket;
	item->io_type=io_type;
	item->buffer=request;
	item->buffer_size=req_size;

	sfqdfull_add_item_to_queue(generic_item);

	generic_item->socket_data=si;

	fprintf(stderr,"%s %s enqueuing tag %i app%i\n", bptr, log_prefix, item->socket_tag, item->app_index);


	int dispatched=0;
	if (sfqdfull_current_depth<sfqdfull_depth)//heap_empty(heap_queue))
	{
		dispatched=1;
		sfqdfull_current_depth++;
		//this means the queue is empty before adding item to it.
	}
	else
	{
		fprintf(stderr,"blocked\n");
	}
	app_stats[app_index].req_come+=1;
	return dispatched;


}

void sfqdfull_get_scheduler_info()
{
	//fprintf(stderr,"depth remains at %i", current_depth);
	//fprintf(stderr," current queue has %i items\n",heap_queue->all_count);
}

int sfqdfull_update_on_request_completion(void* arg)
{

	struct complete_message * complete = (struct complete_message *)arg;
	//struct proxy_message * request = (struct proxy_message)(complete->proxy_message);
	struct generic_queue_item * current_item = (complete->current_item);

	struct sfqdfull_queue_item * sfqdfull_item = (struct sfqdfull_queue_item * )(current_item->embedded_queue_item);
	sfqdfull_item->got_size+=(complete->complete_size);

	if (sfqdfull_item->task_size==sfqdfull_item->got_size)
	{
		struct timeval diff;
		get_time_diff(&(sfqdfull_item->dispatchtime), &diff);
		fprintf(depthtrack, "response time of class %i: %i ms\n", sfqdfull_item->app_index, (int)(diff.tv_sec*1000+diff.tv_usec/1000));
		average_resp_time[sfqdfull_item->app_index]+=(diff.tv_sec*1000+diff.tv_usec/1000);
		finished[sfqdfull_item->app_index]++;
		return sfqdfull_item->got_size;
	}
	else if (sfqdfull_item->task_size < sfqdfull_item->got_size)
	{
		sfqdfull_item->over_count++;
		fprintf(stderr,"error from sock %i, item %i\n",sfqdfull_item->request_socket_index, sfqdfull_item->stream_id);
		fprintf(stderr, "error...got_size %i> task_size %i (completed %i), op is %i, error_count is %i\n",
				sfqdfull_item->got_size,sfqdfull_item->task_size, complete->complete_size, sfqdfull_item->io_type,sfqdfull_item->over_count);

		return -1;

	}
	else
	{
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

struct generic_queue_item * sfqdfull_dequeue(struct dequeue_reason r)
{

	//int last_io_type = r.complete_size;
	struct generic_queue_item * next;//for heap and list
	//free up current item
	struct generic_queue_item * next_item = NULL; //for actual list item removal
	struct sfqdfull_queue_item *next_queue_item;

	int found=0;
	next_retry:

	if (SFQD_FULL_USE_HEAP_QUEUE==1 && !heap_empty(sfqdfull_heap_queue))
	{
		struct heap_node * hn  = heap_take(sfqd_packet_cmp, sfqdfull_heap_queue);
		next_item  = heap_node_value(hn);
		next_queue_item = (struct sfqdfull_queue_item *)(next_item->embedded_queue_item);
		found=1;
	}

	char bptr[20];
    struct timeval tv;
    time_t tp;
    gettimeofday(&tv, 0);
    tp = tv.tv_sec;
    strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
    sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);

	//fprintf(stderr,"%s %s, finished %i complete recv %i, completefwd %i\n",	bptr, log_prefix, finished[0], completerecv[0], completefwd[0]);
	if (SFQD_FULL_USE_HEAP_QUEUE==0)
	{
		/*if (sfqdfull_sorted==0)
		{
			sfqdfull_llist_queue = PINT_llist_sort(sfqdfull_llist_queue,list_sfqd_sort_comp);
			sfqdfull_sorted=1;
		}*/

		int i;
		fprintf(stderr,"%s %s ",bptr,log_prefix);
		for (i=0;i<num_apps;i++)
		{
			fprintf(stderr,"app%i:%i,", i, sfqdfull_list_queue_count[i]);
		}
		fprintf(stderr,"\n");

		next  = (struct generic_queue_item *)PINT_llist_head(sfqdfull_llist_queue);
		if (next!=NULL)
		{
			next_item = (struct generic_queue_item *)PINT_llist_rem(sfqdfull_llist_queue, (void*)next->item_id,  list_req_comp);

		}
		if (next_item!=NULL)
		{
			next_queue_item = (struct sfqdfull_queue_item *)(next_item->embedded_queue_item);
			sfqdfull_list_queue_count[next_queue_item->app_index]--;
			found=1;
			fprintf(stderr," meta dispatching \n");
			int app_index=next_queue_item->app_index;
			if (next_queue_item->app_index==1 && sfqdfull_list_queue_count[0]==0){
				fprintf(stderr,"%s %s, ********************************************** warning app 0 has no item left in the queue\n", bptr, log_prefix);

			}
		}

	}

	if (found==1)
	{
		//current_node = hn;
		sfqdfull_virtual_time=next_queue_item->start_tag;

		fprintf(depthtrack,"%s %s dispatching tag %i app%i on %s\n", bptr, log_prefix, next_queue_item->socket_tag, next_queue_item->app_index,
				s_pool.socket_state_list[next_queue_item->request_socket_index].ip);

		int app_index=next_item->socket_data->app_index;;
		app_stats[app_index].req_go+=1;

		app_stats[app_index].dispatched_requests+=1;

		struct timeval dispatch_time, diff;
		gettimeofday(&dispatch_time, 0);
		next_queue_item->dispatchtime = dispatch_time;
		double ratio = ((float)(app_stats[0].dispatched_requests))/((float)app_stats[1].dispatched_requests);
		fprintf(stderr,"%s %s $$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$ instant ratio is %d/%d=%f\n",
				bptr, log_prefix, app_stats[0].dispatched_requests, app_stats[1].dispatched_requests, ratio);
		return next_item;
	}
	else
	{
		sfqdfull_current_depth--;
		return NULL;
	}
}

int sfqdfull_load_data_from_config (dictionary * dict)
{
	char depth_entry[100];
	snprintf(depth_entry, 100, "%s:depth", SFQD_FULL_SCHEDULER);
	fprintf(stderr,"%s\n",depth_entry);
	sfqdfull_depth=iniparser_getint(dict, depth_entry ,sfqdfull_depth);
	fprintf(stderr,"SFQD_Full using depth:%i\n",sfqdfull_depth);

}

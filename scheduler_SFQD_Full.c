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
#include "scheduler_SFQD_Full.h"
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
#define REDUCER 32
int sfqdfull_default_weight=1;
int sfqdfull_depth=1;
int sfqdfull_current_depth=0;
int sfqdfull_virtual_time=0;
int sfqdfull_small_ios = 0;


int* sfqdfull_last_finish_tags;

int* sfqdfull_list_queue_count;
extern long long total_throughput;
extern int finished[2];
extern int completerecv[2];
extern int completefwd[2];

#define DEFAULT_META_COST 10240 //IN BYTES
#define SFQD_FULL_USE_HEAP_QUEUE 0
#define SFQD_FULL_DEFAULT_LARGE_IO_FACTOR 10 //defines the number of depth each large IO occupies, semi-blocking more large IOs
#define SFQD_FULL_ENABLE_LARGE_IO_BLOCKING 0
#define SFQD_FULL_LARGE_IO_THRESHOLD 65536
#define SFQD_FULL_ENABLE_HARD_BLOCK 0

extern int total_weight;
struct heap * sfqdfull_heap_queue;
PINT_llist_p sfqdfull_llist_queue;
int sfqdfull_item_id=0;

int sfqdfull_large_io_factor = SFQD_FULL_DEFAULT_LARGE_IO_FACTOR;
int sfqdfull_enable_large_io_blocking = SFQD_FULL_ENABLE_LARGE_IO_BLOCKING;
int sfqdfull_large_io_threshold = SFQD_FULL_LARGE_IO_THRESHOLD;
int sfqdfull_enable_hard_block = SFQD_FULL_ENABLE_HARD_BLOCK;
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
    app_stats[app_index].req_go=app_stats[app_index].req_go+1;
}

long long sfqdfull_calculate_diff(int app_index)
{
	long long diff = app_stats[app_index].app_throughput*1.0f-total_throughput*1.0f*app_stats[app_index].app_weight/(float)total_weight;


	fprintf(stderr,"diff is %lli = %lli - %lli * %i / %i \n",diff,
			app_stats[app_index].app_throughput,
			total_throughput,
			app_stats[app_index].app_weight,
			total_weight);
	return diff;
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
    .sch_current_size = sfqdfull_current_size,
    .sch_calculate_diff = sfqdfull_calculate_diff,
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
		sfqdfull_last_finish_tags[i]=0;
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
		else
		{
			sfqdfull_llist_queue = PINT_llist_sort(sfqdfull_llist_queue,list_sfqd_sort_comp);
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

    int my_shared_size = get_my_share(strip_size, server_count, offset, ask_size, server_nr, actual_data_file_size);
    //fprintf(stderr,"item %i adjusted to %i (offset %i, datafile_size %i, asking %i, %i/%i)\n",
    //		sfqdfull_item->socket_tag,  my_shared_size, offset, actual_data_file_size, ask_size, server_nr, server_count);
    int temp = sfqdfull_item->task_size;
    sfqdfull_item->task_size = my_shared_size;

    return my_shared_size;

}



int sfqdfull_get_finish_tag(int length, int weight, int start_tag,enum PVFS_server_op op)
{
	int cost, finish_tag;
	if (cost_model==COST_MODEL_NONE)
	{
		//reducer=REDUCER;
		cost=length;
		if (op!=PVFS_SERV_IO && op!=PVFS_SERV_SMALL_IO)
		{
			cost = DEFAULT_META_COST;
		}

		//fprintf(stderr,"cost is %i\n", cost);

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
		fprintf(depthtrack, "IO type %s ", ops[pi->op]);
		break;

	default:
		fprintf(depthtrack, "Meta type %s, code %i ", ops[pi->op], pi->op);


		break;
	}
	app_stats[si->app_index].received_requests+=1;
	int r_socket_index, d_socket_index, length, tag, io_type, req_size;
	char* request = si->buffer;
	r_socket_index = si->request_socket;
	d_socket_index = si->data_socket;

	if (pi->op != PVFS_SERV_IO && pi->op != PVFS_SERV_SMALL_IO)
	{
		length = DEFAULT_META_COST;
	}
	else
	{
		length = pi->current_data_size;
	}
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
	finish_tag = sfqdfull_get_finish_tag(length, weight,start_tag, pi->op);
	sfqdfull_last_finish_tags[app_index]=finish_tag;

	struct generic_queue_item * generic_item =  (struct generic_queue_item * )malloc(sizeof(struct generic_queue_item));
	struct sfqdfull_queue_item * item = (struct sfqdfull_queue_item *)(malloc(sizeof(struct sfqdfull_queue_item)));
	generic_item->embedded_queue_item=item;

	item->depth = 1;

	if (length <= sfqdfull_large_io_threshold)
	{
		sfqdfull_small_ios++;
		//fprintf(stderr,"small ios increased to %i\n", sfqdfull_small_ios);
	}
	else
	{
		item->depth = sfqdfull_large_io_factor;
		//fprintf(stderr,"large ios %i\n", length);
	}


	//fprintf(depthtrack, "length %i depth %i\n", length, item->depth);

	item->queuedtime = now;
	item->start_tag=start_tag;
	item->finish_tag=finish_tag;
	//fprintf(stderr,"app %i start %i end %i\n",app_index, start_tag, finish_tag);

	item->data_socket_index=d_socket_index;
	item->request_socket_index=r_socket_index;
	item->data_file_size=0;
	item->server_count=pi->total_server;
	item->server_nr=pi->current_server;
	item->file_offset=pi->req_offset;
	item->aggregate_size=pi->aggregate_size;
	item->strip_size=pi->strip_size;
	generic_item->item_id=sfqdfull_item_id++;
	//fprintf(stderr,"adding new item %li\n", generic_item->item_id);

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

	item->task_size=length;//derived from current_datat_size




	item->got_size=0;

	//fprintf(stderr,"task size:%i/%i\n",length, pi->aggregate_size);

	//fprintf(depthtrack, "%s %s offset %lli size %i %s:%i, %i\n", log_prefix, bptr, pi->req_offset, length,
		//	s_pool.socket_state_list[r_socket_index].ip,s_pool.socket_state_list[r_socket_index].port, io_type);
	item->data_socket=d_socket;
	item->socket_tag=socket_tag;
	item->app_index=app_index;
	item->stream_id=app_stats[app_index].stream_id++;

	fprintf(depthtrack,"enqueuing app %i, id %li, start %i, end %i\n",app_index,
			generic_item->item_id, start_tag, finish_tag);


	item->request_socket=s_pool.socket_state_list[r_socket_index].socket;
	item->io_type=io_type;
	item->buffer=request;
	item->buffer_size=req_size;

	sfqdfull_add_item_to_queue(generic_item);

	generic_item->socket_data=si;

	/*fprintf(stderr,"%s %s enqueuing tag %i app%i, start %i end %i\n",
			bptr, log_prefix, item->socket_tag, item->app_index,
			start_tag, finish_tag);
*/

	if (pi->op ==PVFS_SERV_SMALL_IO || pi->op == PVFS_SERV_IO)
	{
		//fprintf(stderr,"item %i enqueuing IO %i start %lli length %i\n", item->socket_tag, io_type, pi->req_offset,length);
	}

	int dispatched=0;
	//fprintf(stderr, "adding from %i + %i = %i\n ",sfqdfull_current_depth, item->depth, sfqdfull_current_depth + item->depth);

	//if (sfqdfull_small_ios > 0 && length > sfqdfull_large_io_threshold)
		//fprintf(depthtrack, "small io activated\n");
	//else
		//fprintf(depthtrack, "small io deactivated\n");

	if (sfqdfull_current_depth +
			((sfqdfull_small_ios > 0 && sfqdfull_enable_large_io_blocking || sfqdfull_enable_hard_block) ? item->depth : 1) <= sfqdfull_depth)
		//a shortcircuit check. the best case scenario must be met
	{
		//dispatched = 1;
		PINT_llist_p l_p = sfqdfull_llist_queue;
		assert(l_p != NULL && l_p->next != NULL);
		if (l_p!=NULL)
			l_p = l_p->next;
		int count=0;
		struct request_state * last = NULL;

		for (; l_p; l_p = l_p->next)
		{
			struct generic_queue_item * next  = (struct generic_queue_item *)(l_p->item);
			struct sfqdfull_queue_item * next_queue_item = (struct sfqdfull_queue_item*)(next->embedded_queue_item);

			int tentative_depth = (sfqdfull_small_ios > 0 && sfqdfull_enable_large_io_blocking || sfqdfull_enable_hard_block) ? next_queue_item->depth : 1;
			if (tentative_depth + sfqdfull_current_depth <= sfqdfull_depth && next != generic_item)
			{
				dispatched = 0;
				break;
			}
			if (tentative_depth + sfqdfull_current_depth <= sfqdfull_depth && next == generic_item)
			{
				dispatched = 1;
				break;
			}
		}
		//sfqdfull_current_depth+=item->depth;
		//fprintf(stderr, "scheduler depth increased to %i\n",sfqdfull_current_depth);
		//this means the queue is empty before adding item to it.
	}
	else
	{
		//fprintf(stderr," scheduler blocked\n");
	}
	if (dispatched == 1)
	{
		//fprintf(stderr,"sfqdfull immediately dispatching item %li %i size %i\n", generic_item->item_id, item->depth, length);
	}
	else
	{
		//fprintf(stderr,"sfqdfull blocked by item %li %i size %i\n", generic_item->item_id, item->depth, length);
	}
	app_stats[app_index].req_come+=1;
	return dispatched;


}

void sfqdfull_get_scheduler_info()
{
	//fprintf(stderr,"depth is at %i", sfqdfull_current_depth);
	//fprintf(stderr," current queue has %i items\n",heap_queue->all_count);
}

int sfqdfull_update_on_request_completion(void* arg)
{


	struct complete_message * complete = (struct complete_message *)arg;
	if (complete->complete_size==-1)
	{
		//this is a skip flag for response messages
		//there is no in-fly task, any response should be followed by a complete message
		//although you might argue that you could receive multiple times before a complete listdir is received
		//finished[sfqdfull_item->app_index]++;
		//app_stats[sfqdfull_item->app_index].completed_requests+=1;
		return 0;

	}


	struct generic_queue_item * current_item = (complete->current_item);

	struct sfqdfull_queue_item * sfqdfull_item = (struct sfqdfull_queue_item * )(current_item->embedded_queue_item);
	//fprintf(stderr,"completed size: %i\n", complete->complete_size);


	//struct proxy_message * request = (struct proxy_message)(complete->proxy_message);
	//fprintf(stderr,"complete size from %i",sfqdfull_item->got_size);
	sfqdfull_item->got_size+=(complete->complete_size);
	//fprintf(stderr,"item %i: %i after incrementing %i\n", sfqdfull_item->socket_tag, complete->complete_size,sfqdfull_item->got_size);

	if (sfqdfull_item->task_size==sfqdfull_item->got_size)
	{
		struct timeval diff;
		get_time_diff(&(sfqdfull_item->dispatchtime), &diff);
		//fprintf(depthtrack, "response time of class %i: %i ms\n", sfqdfull_item->app_index, (int)(diff.tv_sec*1000+diff.tv_usec/1000));
		average_resp_time[sfqdfull_item->app_index]+=(diff.tv_sec*1000+diff.tv_usec/1000);
		//finished[sfqdfull_item->app_index]++;
		//app_stats[sfqdfull_item->app_index].completed_requests+=1;
		if (sfqdfull_item->task_size==0)
		{
			return 1;
		}
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
	//fprintf(stderr, "old item %i", r.item);

	int last_io_depth;
	if (r.event == COMPLETE_IO) last_io_depth = 1;
	else last_io_depth = 0;
	if (r.event == COMPLETE_IO && r.item != NULL)
	{
		//fprintf(stderr, " %i", r.item->embedded_queue_item);
		if (r.item->embedded_queue_item != NULL)
		{
			last_io_depth = ((struct sfqdfull_queue_item*)(r.item->embedded_queue_item))->depth;
			//fprintf(stderr, " depth %i, item id %li\n", last_io_depth, r.item->item_id);
		}
		else
		{
			//fprintf(stderr, "complete io but no embedded queue item\n");
			last_io_depth = 0;
		}
	}
	else
	{
		//fprintf(stderr, "new item? %d %p\n", r.event, r.item);
		last_io_depth = 0;
	}

	if (r.event == COMPLETE_IO && last_io_depth == 1)
	{
		sfqdfull_small_ios--;
		//fprintf(stderr,"small ios decreased to %i\n", sfqdfull_small_ios);
		assert(sfqdfull_small_ios >= 0);
	}


	//fprintf(stderr, "\n");
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
    int reduce_depth = 0;
    int tentative_depth = 0;
	//fprintf(stderr,"%s %s, finished %i complete recv %i, completefwd %i\n",	bptr, log_prefix, finished[0], completerecv[0], completefwd[0]);
	if (SFQD_FULL_USE_HEAP_QUEUE==0)
	{
		if (sfqdfull_sorted==0)
		{
			//sfqdfull_llist_queue = PINT_llist_sort(sfqdfull_llist_queue,list_sfqd_sort_comp);
			sfqdfull_sorted=1;
		}

		int i;

		PINT_llist_p l_p = sfqdfull_llist_queue;
		if (l_p!=NULL)
			l_p = l_p->next;
		int count=0;
		struct request_state * last = NULL;



		if (r.event == COMPLETE_IO)
		{
			if (sfqdfull_enable_hard_block)
			{
				reduce_depth = last_io_depth;
			}
			else
			{
				reduce_depth = 1;
			}
		}
		for (; l_p; l_p = l_p->next)
		{
			next  = (struct generic_queue_item *)(l_p->item);
			next_queue_item = (struct sfqdfull_queue_item*)(next->embedded_queue_item);
			tentative_depth = (sfqdfull_small_ios > 0 && sfqdfull_enable_large_io_blocking || sfqdfull_enable_hard_block) ? next_queue_item->depth : 1;

			if (tentative_depth + sfqdfull_current_depth - reduce_depth <= sfqdfull_depth)
			{
				//fprintf(stderr,"new item %li real depth %i + current depth %i - last depth %i = %i\n",
						//next->item_id, next_queue_item->depth, sfqdfull_current_depth, last_io_depth,
						//tentative_depth + sfqdfull_current_depth - reduce_depth );
				next_item = (struct generic_queue_item *)PINT_llist_rem(sfqdfull_llist_queue, (void*)next->item_id,  list_req_comp);
				break;
			}
		}
		if (next_item!=NULL)
		{
			next_queue_item = (struct sfqdfull_queue_item *)(next_item->embedded_queue_item);
			sfqdfull_list_queue_count[next_queue_item->app_index]--;
			found=1;

			fprintf(depthtrack,"dispatching app %i, id %li, start %i, end %i\n",
					next_queue_item->app_index, next_item->item_id, next_queue_item->start_tag, next_queue_item->finish_tag);
			int app_index=next_queue_item->app_index;
			if (next_queue_item->app_index==1 && sfqdfull_list_queue_count[0]==0){
				//fprintf(stderr,"%s %s, ********************************************** warning app 0 has no item left in the queue\n", bptr, log_prefix);
			}
		}

	}

	if (found==1)
	{
		//current_node = hn;
		sfqdfull_virtual_time=next_queue_item->start_tag;
		//depending hardblock or not, difference between this IO and last IO will be added to the updated depth
		//hard: tentative_depth-last_io
		//soft: 1-last_io
		//sfqdfull_current_depth = sfqdfull_current_depth + next_queue_item->depth - last_io_depth;

		sfqdfull_current_depth = sfqdfull_current_depth + tentative_depth - reduce_depth;

		/*fprintf(stderr,"%s %s dispatching tag %i starttag %i app %i on %s\n", bptr, log_prefix,
				next_queue_item->socket_tag, next_queue_item->start_tag, next_queue_item->app_index,
				s_pool.socket_state_list[next_queue_item->request_socket_index].ip);
*/


		int app_index=next_item->socket_data->app_index;
		app_stats[app_index].req_go+=1;

		app_stats[app_index].dispatched_requests+=1;

		struct timeval dispatch_time, diff;
		gettimeofday(&dispatch_time, 0);
		next_queue_item->dispatchtime = dispatch_time;
		/*double ratio = ((float)(app_stats[0].dispatched_requests))/((float)app_stats[1].dispatched_requests);
		fprintf(stderr,"%s %s instant ratio is %d/%d=%f\n",
				bptr, log_prefix, app_stats[0].dispatched_requests, app_stats[1].dispatched_requests, ratio);
		*/
		//fprintf(stderr, " new item %i\n", next_item);
		//fprintf(stderr, " scheduler depth changed at %i by item %li\n",sfqdfull_current_depth, next_item->item_id);
		return next_item;
	}
	else
	{
		//fprintf(stderr, "\n");
		//sfqdfull_current_depth -= last_io_depth;
		sfqdfull_current_depth-=reduce_depth;
		//fprintf(stderr, " scheduler depth decreased to %i\n",sfqdfull_current_depth);
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

	char largefactor_entry[100];
	snprintf(largefactor_entry, 100, "%s:largefactor", SFQD_FULL_SCHEDULER);
	fprintf(stderr,"%s\n",largefactor_entry);
	sfqdfull_large_io_factor=iniparser_getint(dict, largefactor_entry, sfqdfull_large_io_factor);
	fprintf(stderr,"SFQD_Full using largefactor:%i\n", sfqdfull_large_io_factor);

	char enablelargeblock_entry[100];
	snprintf(enablelargeblock_entry, 100, "%s:enablelargeblock", SFQD_FULL_SCHEDULER);
	fprintf(stderr,"%s\n",enablelargeblock_entry);
	sfqdfull_enable_large_io_blocking=iniparser_getint(dict, enablelargeblock_entry ,sfqdfull_enable_large_io_blocking);
	fprintf(stderr,"SFQD_Full using largeblock:%i\n",sfqdfull_enable_large_io_blocking);

	char enablehardblock_entry[100];
	snprintf(enablehardblock_entry, 100, "%s:hardblock", SFQD_FULL_SCHEDULER);
	fprintf(stderr,"%s\n",enablehardblock_entry);
	sfqdfull_enable_hard_block=iniparser_getint(dict, enablehardblock_entry ,sfqdfull_enable_hard_block);
	fprintf(stderr,"SFQD_Full using hardblock:%i\n",sfqdfull_enable_hard_block);

}

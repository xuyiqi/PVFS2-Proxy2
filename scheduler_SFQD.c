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
#include "scheduler_SFQD.h"
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
int sfqd_default_weight=1;
int sfqd_depth=1;
int sfqd_current_depth=0;
int sfqd_virtual_time=0;
int* sfqd_last_finish_tags;
extern long long total_throughput;



struct heap * sfqd_heap_queue;
int sfqd_item_id=0;


int print_ip_app_item(void* item);
int sfqd_add_ttl_tp(int this_amount, int app_index)
{
	total_throughput+=this_amount;
    app_stats[app_index].byte_counter=app_stats[app_index].byte_counter+this_amount;
    app_stats[app_index].app_throughput=app_stats[app_index].app_throughput+this_amount;


}

const struct scheduler_method sch_sfqd = {
    .method_name = SFQD_SCHEDULER,
    .work_conserving = 1,
    .sch_initialize = sfqd_init,
    .sch_finalize = NULL,
    .sch_enqueue = sfqd_enqueue,
    .sch_dequeue = sfqd_dequeue,
    .sch_load_data_from_config = sfqd_load_data_from_config,
    .sch_update_on_request_completion = sfqd_update_on_request_completion,
    .sch_get_scheduler_info = sfqd_get_scheduler_info,
    .sch_is_idle = sfqd_is_idle,
    .sch_current_size = sfqd_current_size,
    .sch_add_ttl_throughput = sfqd_add_ttl_tp,
    .sch_self_dispatch = 0

};




int sfqd_init()
{
	sfqd_heap_queue=(struct heap*)malloc(sizeof(struct heap));

	heap_init(sfqd_heap_queue);

	sfqd_last_finish_tags=(int*)malloc(num_apps*sizeof(int));

	int i;
	for (i=0;i<num_apps;i++)
	{
		sfqd_last_finish_tags[i]=0;
	}
	char* deptht=(char*)malloc(sizeof(char)*40);
	snprintf(deptht, 40, "%s.depthtrack.txt", log_prefix);
	depthtrack = fopen(deptht,"w");

}

//weight_mapping-ip-app mapping
//app-weight mapping
int sfqd_packet_cmp(struct heap_node* _a, struct heap_node* _b)
{
	struct generic_queue_item *g_a, *g_b;
	struct sfqd_queue_item *a, *b;

	g_a = (struct generic_queue_item*) heap_node_value(_a);
	g_b = (struct generic_queue_item*) heap_node_value(_b);
	a = (struct sfqd_queue_item *)g_a->embedded_queue_item;
	b = (struct sfqd_queue_item *)g_b->embedded_queue_item;
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

int add_item(struct heap* heap, struct generic_queue_item* item)
{
	struct heap_node* hn = malloc(sizeof(struct heap_node));
	heap_node_init(hn, item);
	int dispatched=0;


	heap_insert(sfqd_packet_cmp, heap, hn);
	return dispatched;
}
int sfqd_is_idle()
{

	return sfqd_heap_queue->all_count;
			//heap_empty(heap_queue);
}

int sfqd_current_size(int sock_index, long long actual_data_file_size)
{

	struct generic_queue_item * current_item = s_pool.socket_state_list[sock_index].current_item;

	struct sfqd_queue_item * sfqd_item = (struct sfqd_queue_item * )(current_item->embedded_queue_item);

	sfqd_item->data_file_size=actual_data_file_size;

	int strip_size=sfqd_item->strip_size;
	int server_nr = sfqd_item->server_nr;
	int server_count= sfqd_item->server_count;

	long long offset=sfqd_item->file_offset;
	//update expected receivables for current item!
	long long ask_size= sfqd_item->aggregate_size;

	//if (ask_size+offset> actual_data_file_size)
	{

		//ask_size=actual_data_file_size-offset;
	}

	//fprintf(stderr,"item %i strip size:%i, server nr: %i, server count:%i offset:%lli, ask size:%lli\n",
	//sfqd_item->stream_id,strip_size, server_nr, server_count, offset, ask_size );

	int whole_strip_size=strip_size*server_count;


	int offset_nr_whole_strip=offset/whole_strip_size;
	int left_over_first=offset % whole_strip_size;
	int left_over_second;



	if (left_over_first>0)
	{
		left_over_second= whole_strip_size - left_over_first;
	}
	else
	{
		left_over_second=0;
	}

	//fprintf(stderr,"whole strip:%i, offset strips:%i, leftover left:%i, leftover right:%i\n",
	//whole_strip_size, offset_nr_whole_strip, left_over_first, left_over_second
	//);

	int data_nr_whole_strip= (ask_size-left_over_second)/whole_strip_size;
	int left_over_third=(ask_size-left_over_second) % whole_strip_size;

	int left_over_current_top=0;
	int left_over_current_bottom=0;

	//fprintf(stderr,"data_nr_whole_strip:%i, left_over_third:%i\n", data_nr_whole_strip, left_over_third);

    if(left_over_third >= server_nr*strip_size)
    {
        /* if so, tack that on to the physical offset as well */
        if(left_over_third < (server_nr + 1) * strip_size)
            left_over_current_bottom += left_over_third - (server_nr * strip_size);
        else
            left_over_current_bottom += strip_size;
    }
    //fprintf(stderr,"left over bottom1:%i\n", left_over_current_bottom);

    if(left_over_first>0 && left_over_first >= server_nr*strip_size)
    {
        /* if so, tack that on to the physical offset as well */
        if(left_over_first < (server_nr + 1) * strip_size)
            left_over_current_bottom -= left_over_first - (server_nr * strip_size);
        else
            left_over_current_bottom -= strip_size;
    }
    //fprintf(stderr,"left over bottom2:%i\n",left_over_current_bottom);

    int all_shared_size = data_nr_whole_strip*strip_size;
    if (left_over_first>0)
    {
    	all_shared_size+=strip_size;
    }

    int my_shared_size=all_shared_size+left_over_current_bottom;
    //fprintf(stderr,"current item adjusted to %i\n",my_shared_size);
    sfqd_item->task_size=my_shared_size;
    /*
     *
     * !!!remember, you can change last_finish_tag?//unless you use a linked list/flat format data structure that supports re-ordering
     *
     * */

    return my_shared_size;

}


int sfqd_enqueue(struct socket_info * si, struct pvfs_info* pi)
{
	struct timeval now;
	gettimeofday(&now, 0);

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
	int start_tag=MAX(sfqd_virtual_time, sfqd_last_finish_tags[app_index]);//work-conserving
	//Dprintf(D_CACHE, "virtual time is %i, last finish tag of app %i is %i\n", virtual_time, app_index+1, last_finish_tags[app_index]);
	//int start_tag=last_finish_tags[app_index];//non-work-conserving
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
		//cost=length;//testing mode, just want output


		cost=length;
				get_expected_resp(app_index, r_socket_index, io_type);//length*(sfqd_current_depth+1);
				finish_tag = start_tag+cost/weight/REDUCER;
		//fprintf (stderr, "Throughput is translated into %i / %i = %i\n", length, cost, length/cost);
		//cost = length / cost;
		//fprintf(stderr,"Max resp time for this kind of IO for app %i is cost = %i\n", app_index, cost);
		//finish_tag = start_tag+cost*10/weight;
	}

	sfqd_last_finish_tags[app_index]=finish_tag;
	//Dprintf(D_CACHE, "my previous finish tag is updated to %i, weight %i\n",finish_tag,s_pool.socket_state_list[r_socket_index].weight);

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
	generic_item->item_id=sfqd_item_id++;
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
	add_item(sfqd_heap_queue,generic_item);

	int dispatched=0;
	if (sfqd_current_depth<sfqd_depth)//heap_empty(heap_queue))
	{
		dispatched=1;
		sfqd_current_depth++;
		//this means the queue is empty before adding item to it.
	}
	if (dispatched)
	{
		//Dprintf(D_CACHE, "current_depth increased to %i\n",current_depth);

		//virtual_time=start_tag;
		//Dprintf(D_CACHE,"[CLOCK] virtual time is updated to %i\n",virtual_time);

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

struct generic_queue_item * sfqd_dequeue(struct dequeue_reason r)
{
	return sfqd_get_next_request(r);
}

void sfqd_get_scheduler_info()
{
	//fprintf(stderr,"depth remains at %i", current_depth);
	//fprintf(stderr," current queue has %i items\n",heap_queue->all_count);
}

void sfqd_fill_proxy_message(void* arg)
{


}

int sfqd_update_on_request_completion(void* arg)
{

	struct complete_message * complete = (struct complete_message *)arg;
	//struct proxy_message * request = (struct proxy_message)(complete->proxy_message);
	struct generic_queue_item * current_item = (complete->current_item);

	struct sfqd_queue_item * sfqd_item = (struct sfqd_queue_item * )(current_item->embedded_queue_item);
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

		struct timeval diff;
		get_time_diff(&(sfqd_item->dispatchtime), &diff);
		fprintf(depthtrack, "response time of class %i: %i ms\n", sfqd_item->app_index, (int)(diff.tv_sec*1000+diff.tv_usec/1000));
		average_resp_time[sfqd_item->app_index]+=(diff.tv_sec*1000+diff.tv_usec/1000);
		return sfqd_item->got_size;
		//lock

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
struct generic_queue_item* sfqd_get_next_request(struct dequeue_reason r)
{
	/*
	 * Called after add_request to determine if it's dispatched immediately
	 *
	 * Update last_serviced_request time
	 * */



	int last_io_type = r.complete_size;
	struct generic_queue_item * next;
	//free up current item

	next_retry:
	if (!heap_empty(sfqd_heap_queue))
	{

		int l;
		for (l=0;l<num_apps;l++)
		{
			//fprintf(stderr, "app queue: %i has %i items left, all_exist=%i\n",l+1,heap_queue->count[l],first_receive);

		}

		struct heap_node * hn  = heap_take(sfqd_packet_cmp, sfqd_heap_queue);
		next  = heap_node_value(hn);
		struct sfqd_queue_item *next_item = (struct sfqd_queue_item *)(next->embedded_queue_item);



		//current_node = hn;
		sfqd_virtual_time=next_item->start_tag;
		//Dprintf(D_CACHE,"[CLOCK] virtual time is updated to %i\n",virtual_time);
		//Dprintf(D_CACHE, "unblocking socket %i, %s:%i, tag %i\n", next_item->data_socket, next_item->data_ip, next_item->data_port, next_item->socket_tag);


		int i;
		for (i=0;i<s_pool.pool_size;i++)
		{
			if (s_pool.socket_state_list[i].socket==next_item->request_socket)
			{
				//fprintf(stderr, "[NEXT]socket from %s:%i is being unlocked\n",
				//		s_pool.socket_state_list[i].ip,s_pool.socket_state_list[i].port);

/*				if (pthread_mutex_lock (&counter_mutex)!=0)
				{
					Dprintf(D_CACHE, "error locking when getting counter\n");
				}*/
				next->socket_data->unlock_index=i;
				//s_pool.socket_state_list[i].locked=0;
				/*#
				 * locked should be put outside of the scheduler
				 * why lock?
				 * #*/
/*
				if (pthread_mutex_unlock (&counter_mutex)!=0)
				{
					Dprintf(D_CACHE, "error locking when getting counter\n");
				}
*/
				s_pool.socket_state_list[i].current_item=next;
				//s_pool.socket_state_list[i].locked=0;
				//Dprintf(D_CACHE, "adding Original socket was %i\n",next_item->request_socket);
				break;
			}
		}
		if (i>=s_pool.pool_size)
		{
			fprintf(stderr,"[FATAL] socket %i not found anymore\n", next_item->request_socket);
			heap_take(sfqd_packet_cmp, sfqd_heap_queue);
			goto next_retry;
		}
/*		char bptr[20];
	    struct timeval tv;
	    time_t tp;
	    gettimeofday(&tv, 0);
	    tp = tv.tv_sec;
	    strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
	    sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);
	    char delay[20];
	    struct timeval diff;
	    timersub(&tv,&(s_pool.socket_state_list[i].last_work_time),&diff);

	    tp = diff.tv_sec;
	    strftime(delay, 9, "%H:%M:%S", localtime(&tp));
	    sprintf(delay+8, ".%06ld", (long)diff.tv_usec);
		fprintf(stderr, "%s releasing start_tag %i finish_tag:%i, weight:%i, delay:%s, app: %i, %s, queue length is %i\n", bptr, next_item->start_tag, next_item->finish_tag,
				s_pool.socket_state_list[i].weight, delay,s_pool.socket_state_list[i].app_index+1,s_pool.socket_state_list[i].ip, heap_queue->all_count);
*/


/*		Dprintf(D_CACHE, "releasing start_tag %i finish_tag:%i, weight:%i, app: %i, %s, queue length is %i, socket %i\n",
				next_item->start_tag, next_item->finish_tag,
						s_pool.socket_state_list[i].weight, s_pool.socket_state_list[i].app_index+1,s_pool.socket_state_list[i].ip, heap_queue->all_count,
						s_pool.socket_state_list[i].socket);
		Dprintf(D_CACHE, "virtual time updated to %i\n", virtual_time);*/

		//if (pthread_mutex_lock (&counter_mutex)!=0)
		{
			//Dprintf(D_CACHE, "error locking when incrementing counter in additional counters\n");
		}
		//add addition counter
		int app_index=s_pool.socket_state_list[i].app_index;
		app_stats[app_index].req_go+=1;

		//Dprintf(D_CACHE, "Performance++req_go:%i,%i from %s\n",req_go[app_index],app_index+1, s_pool.socket_state_list[i].ip);
		//if (pthread_mutex_unlock (&counter_mutex)!=0)
		{
			//Dprintf(D_CACHE, "error unlocking when done incrementing counter in additional counters\n");
		}
		//fprintf(stderr,"dequeue depth:%i\n",current_depth);
		app_stats[app_index].dispatched_requests+=1;
		//if (next->item_id %10 ==0)
		{
			//fprintf(depthtrack, "%s: dispatching item: %li; queue size: %i, depth: %i, app1: %i, app2: %i, tag %i\n",
					//log_prefix, next->item_id, sfqd_heap_queue->all_count, sfqd_current_depth, sfqd_heap_queue->count[0], sfqd_heap_queue->count[1], next_item->socket_tag);//, dispatched_requests[0], dispatched_requests[1]);
		}
		struct timeval dispatch_time, diff;
		gettimeofday(&dispatch_time, 0);
		next_item->dispatchtime = dispatch_time;
		//fprintf(stderr,"setting dispatch time to be %i.%06i\n",
		//		(int)next_item->dispatchtime.tv_sec, (int)next_item->dispatchtime.tv_usec );
		return next;
	}
	else
	{
		//Dprintf(D_CACHE, "app queue: no job found!, all_exist=%i\n",first_receive);
		sfqd_current_depth--;
		//fprintf(stderr,"dequeue depth:%i\n",current_depth);
		return NULL;
	}
}

int sfqd_load_data_from_config (dictionary * dict)
{
	sfqd_depth=iniparser_getint(dict, "SFQD:depth" ,sfqd_depth);
	fprintf(stderr,"SFQD using depth:%i\n",sfqd_depth);

}

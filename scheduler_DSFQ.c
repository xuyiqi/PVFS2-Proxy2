/*
 * scheduler_DSFQ.c
 *
 *  Created on: Dec 13, 2010
 *      Author: yiqi
 */



#include <sys/types.h>
#include <sys/param.h>
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
#include "scheduler_DSFQ.h"
#include "heap.h"
#include "iniparser.h"
#include "llist.h"
#include "performance.h"
#include "scheduler_main.h"
#include <resolv.h>
#include <pthread.h>
#include <poll.h>
#include "config.h"
extern char* pvfs_config;
extern char* log_prefix;
extern long long total_throughput;

#define DEFAULT_DSFQ_BROADCAST_THRESHOLD 10//in KBytes
#define BROADCAST_QUEUE_SIZE 2000 //a little larger than # clients
#define MAX_SERVER_NUM 1000
#define RECEIVE_QUEUE_SIZE (BROADCAST_QUEUE_SIZE*MAX_SERVER_NUM)
#define DEFAULT_DSFQ_DEPTH 1
#define DEFAULT_DSFQ_MIN_SHARE 0.5//which means that for apps, the shares are at least 1
#define REDUCER 1024
extern int total_weight;
extern int broadcast_amount;
extern FILE* broadcast_file;
pthread_mutex_t dsfq_accept_mutex;
pthread_mutex_t dsfq_queue_mutex;
pthread_cond_t dsfq_accept_cond;
pthread_mutex_t dsfq_broadcast_mutex;
pthread_cond_t dsfq_broadcast_complete_cond;
pthread_cond_t dsfq_broadcast_begin_cond;
pthread_cond_t dsfq_threshold_cond;

int dsfq_item_id=0;
PINT_llist_p other_server_names;
char** other_server_address;
struct sockaddr_in  ** other_server_ips;
int* dsfq_last_finish_tags;
int dsfq_depth=DEFAULT_DSFQ_DEPTH;
double dsfq_min_share = DEFAULT_DSFQ_MIN_SHARE;
int dsfq_current_depth=0;
int dsfq_threshold=DEFAULT_DSFQ_BROADCAST_THRESHOLD*1024;//
int dsfq_current_io_counter=0;
int dsfq_virtual_time=0;
PINT_llist_p receive_message;
PINT_llist_p broadcast_message;
//struct proxy_queue receive_queue;
//struct proxy_queue broadcast_queue;
struct dsfq_statistics * dsfq_stats;

//put them in a struct
PINT_llist_p dsfq_llist_queue;
//int dsfq_stream_id=0;

int dsfq_purecost=1;

int other_server_count=0;
int fill_ip_count=0;

int dsfq_queue_size=0;

int dsfq_new_item_added=0;
char ** client_ips;


//int* broadcast_queue;//don't know what it is
//int* request_receive_queue;//from client
//int* dsfq_delay_values;//proxy receive queue, we need this for lazy receive/update?
//int* dsfq_delay_value_bitmap;//proxy receive queue, we need this for lazy receive/update?
//int * dsfq_last_applied_delay;
//int *dsfq_app_dispatch;
//int *dsfq_last_finish_tags;
struct proxy_message ** broadcast_buffer;
struct proxy_message ** receive_buffer;
int* broadcast_buffer_size;
int* receive_buffer_size;
int * dsfq_last_applied_item_ids;

int broadcast_complete=1;	//waited by broadcast coordinator, signaled by broadcast worker
int accept_ready=0;			//waited by dsfq_init, signaled by accept thread
int broadcast_begin=0;		//waited by broadcast worker, signaled by broadcast coordinator
int threshold_ready=0;		//waited by broadcast coordinator, signaled by receive thread

int dsfq_proxy_listen_port=DEFAULT_DSFQ_PROXY_PORT;

struct pollfd * broadcast_sockets, *receive_sockets;

void* dsfq_proxy_broadcast_work(void * t_arg);
void* dsfq_proxy_receive_work(void * t_arg);
void* dsfq_proxy_accept_work(void * t_arg);
void* dsfq_broadcast_trigger_work (void* t_arg);
int dsfq_load_data_from_config (dictionary * dict);

int dsfq_load_data_from_config (dictionary * dict)
{

	dsfq_depth=iniparser_getint(dict, "DSFQ:depth" ,dsfq_depth);
	dsfq_proxy_listen_port=iniparser_getint(dict,"DSFQ:listen_port",dsfq_proxy_listen_port);
	dsfq_threshold=iniparser_getint(dict, "DSFQ:threshold", dsfq_threshold);
	dsfq_min_share=iniparser_getdouble(dict, "DSFQ:min_share", dsfq_min_share);

	if (dsfq_depth<=0)
	{
		fprintf(stderr, "wrong depth\n");
		exit(-1);

	}
	if (dsfq_proxy_listen_port<=1024 || dsfq_proxy_listen_port>65535)
	{
		fprintf(stderr, "wrong port\n");
		exit(-1);

	}
	if (dsfq_proxy_listen_port==active_port)
	{
		fprintf(stderr, "conflict port with proxy\n");
		exit(-1);
	}
	if (dsfq_threshold<=0)
	{
		fprintf(stderr, "wrong threshold\n");
		exit(-1);

	}
	if (dsfq_min_share<=0.0001f)
	{
		fprintf(stderr, "wrong minimum share\n");
		exit(-1);

	}
	fprintf(stderr,"DSFQ using depth:%i\n",dsfq_depth);
	fprintf(stderr,"DSFQ using port:%i\n",dsfq_proxy_listen_port);
	dsfq_threshold*=1024;
	fprintf(stderr,"DSFQ using threshold:%i\n",dsfq_threshold);
	fprintf(stderr,"DSFQ using min_share:%f for HYBRID_DSFQ\n",dsfq_min_share);
}



int list_pmsg_print_all(void * item)
{
	//fprintf(stderr, "item address:%#010x\n", (((struct generic_queue_item *)item)->embedded_queue_item));
	return 0;
}

void dsfq_get_scheduler_info()
{
	//Dprintf(D_CACHE,"depth remains at %i\n", dsfq_current_depth);
	//Dprintf(D_CACHE,"current queue has %i items\n",dsfq_queue_size);
}

int dsfq_enqueue(struct socket_info * si, struct pvfs_info* pi)
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
	int d_socket=s_pool.socket_state_list[d_socket_index].socket;
	int socket_tag = tag;

	int app_index= s_pool.socket_state_list[r_socket_index].app_index;
	//Dprintf(D_CACHE, "app_index is %i for ip %s\n", app_index, ip);
	int weight = s_pool.socket_state_list[r_socket_index].weight;

	struct dsfq_queue_item * item =  (struct dsfq_queue_item * )malloc(sizeof(struct dsfq_queue_item));

	//Dprintf(D_CACHE,"weight got from request socket is %i,ip %s\n",weight,ip);
	pthread_mutex_lock(&dsfq_broadcast_mutex);
	dsfq_stats[app_index].request_receive_queue+=length;

	dsfq_current_io_counter+=length;
	if (dsfq_current_io_counter>=dsfq_threshold)
	{
		//fprintf(stderr,"%s: counter reached %i > %i\n", log_prefix, dsfq_current_io_counter, dsfq_threshold);
		threshold_ready=1;
		pthread_cond_signal(&dsfq_threshold_cond);
	}

	pthread_mutex_unlock(&dsfq_broadcast_mutex);


	pthread_mutex_lock(&dsfq_queue_mutex);

	item->delay_value=dsfq_stats[app_index].dsfq_delay_values;
	dsfq_stats[app_index].dsfq_delay_values=0;
	//fprintf(stderr,"%s: setting app %i value to zero\n", log_prefix, app_index);
	item->weight=s_pool.socket_state_list[r_socket_index].weight;
	item->delay_value/=(item->weight * (REDUCER));

	//fprintf(stderr,"%s: app %i delay value applied is %i value to zero\n", log_prefix, app_index, item->delay_value);

	int start_tag=MAX(dsfq_virtual_time,dsfq_last_finish_tags[app_index]+item->delay_value);//work-conserving

	//fprintf(stderr,"%s: virtual time: %i, finish tag: %i, delay value: %i  , taken value: %i\n",log_prefix, dsfq_virtual_time,dsfq_last_finish_tags[app_index], item->delay_value, start_tag);
	//int start_tag=last_finish_tags[app_index];//non-work-conserving
	//item->last_finish_tag=dsfq_last_finish_tags[app_index];
	//item->virtual_time=dsfq_virtual_time;
	int cost;
	if (dsfq_purecost)
	{
		cost=length;
	}
	else
	{
		cost=length;//*(dsfq_current_depth+1);
	}
	int finish_tag = start_tag+cost/weight/REDUCER;
	dsfq_last_finish_tags[app_index]=finish_tag;
	//Dprintf(D_CACHE, "my previous finish tag is updated to %i, weight %i\n",finish_tag,s_pool.socket_state_list[r_socket_index].weight);



	item->start_tag=start_tag;
	item->finish_tag=finish_tag;


/*
 *  the section deals with the queueing delay of requests...from the moment it is queued to when it is released
 *  in the later section we will deal with the response time of requests from the moment it is released until when it is completed
 * char bptr[20];
    struct timeval tv;
    time_t tp;
    gettimeofday(&tv, 0);
    tp = tv.tv_sec;
    strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
    sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);
    s_pool.socket_state_list[r_socket_index].last_work_time=tv;*/
	//Dprintf(D_CACHE, "%s adding start_tag:%i finish_tag:%i weight:%i app:%i ip:%s\n",bptr, start_tag, finish_tag, weight,s_pool.socket_state_list[r_socket_index].app_index+1,s_pool.socket_state_list[r_socket_index].ip);
	//Dprintf(D_CACHE, "adding to %ith socket %i\n",r_socket_index, s_pool.socket_state_list[r_socket_index].socket);
	item->data_port=port;
	item->data_ip=ip;
	item->task_size=length;
	item->data_socket=d_socket;
	item->socket_tag=socket_tag;
	item->app_index=app_index;
	item->stream_id=app_stats[app_index].stream_id++;
	item->got_size=0;
	item->request_socket=s_pool.socket_state_list[r_socket_index].socket;
	item->io_type=io_type;
	item->buffer=request;
	item->buffer_size=req_size;

	struct generic_queue_item * generic_item = (struct generic_queue_item *) malloc(sizeof(struct generic_queue_item));

	//Dprintf(D_CACHE, "[INITIALIZE]ip:%s port:%i tag:%i\n", item->data_ip, item->data_port, item->socket_tag);

	generic_item->embedded_queue_item=item;
	generic_item->item_id=dsfq_item_id++;
	generic_item->socket_data=si;

	if  (PINT_llist_add_to_tail(
			dsfq_llist_queue,
	    generic_item))
	{

		fprintf(stderr,"queue insertion error!\n");
		exit(-1);
	}

	//PINT_llist_doall(dsfq_llist_queue,list_pmsg_print_all);
	int dispatched=0;
	if (dsfq_current_depth<dsfq_depth)//meaning the queue must be empty before this insertion
	{
		dispatched=1;
		dsfq_current_depth++;
		//this means the queue is empty before adding item to it.
	}

	dsfq_queue_size++;


	dsfq_new_item_added=1;
	if (dispatched)
	{
		//fprintf(stderr, "current_depth increased to %i\n",dsfq_current_depth);
		//fprintf(stderr, "dispatched immeidately\n");

	}
	else
	{
		//fprintf(stderr,"sorting after adding new item...\n");
		//fprintf(stderr,"service delayed\n");




		//dsfq_llist_queue = PINT_llist_dsfq_sort(dsfq_llist_queue);//sort
	}

	pthread_mutex_unlock(&dsfq_queue_mutex);


	return dispatched;

}

struct generic_queue_item * dsfq_dequeue(struct dequeue_reason r)
{
	pthread_mutex_lock(&dsfq_queue_mutex);
	PINT_llist_doall(dsfq_llist_queue,list_pmsg_print_all);
	//get the head of the list, remove it from the head


	if (dsfq_new_item_added==1)
	{
		dsfq_llist_queue = PINT_llist_sort(dsfq_llist_queue,list_dsfq_sort_comp);
		dsfq_new_item_added=0;
	}

	struct generic_queue_item * item  = (struct generic_queue_item *)PINT_llist_head(dsfq_llist_queue);
	//fprintf(stderr, "msg index is %i\n",head_message->msg_index);
	struct generic_queue_item * next_item = NULL;

	if (item!=NULL)
	{
		next_item = (struct generic_queue_item *)PINT_llist_rem(dsfq_llist_queue, (void*)item->item_id,  list_req_comp);

	}

	if (next_item!=NULL)
	{
		struct dsfq_queue_item * dsfq_item = (struct dsfq_queue_item *)(next_item->embedded_queue_item);
		dsfq_virtual_time=dsfq_item->start_tag;

		int i;
		for (i=0;i<s_pool.pool_size;i++)
		{
			if (s_pool.socket_state_list[i].socket==dsfq_item->request_socket)
			{
				//Dprintf(D_CACHE, "[NEXT]socket from %s:%i is being unlocked\n",
				//		s_pool.socket_state_list[i].ip,s_pool.socket_state_list[i].port);

				next_item->socket_data->unlock_index=i;
				s_pool.socket_state_list[i].current_item=next_item;
				//s_pool.socket_state_list[i].locked=0;
				//Dprintf(D_CACHE, "adding Original socket was %i\n",dsfq_item->request_socket);
				break;
			}
		}
		//fprintf(stderr, "releasing start_tag %i finish_tag:%i, weight:%i, app: %i, %s, queue length is %i\n", dsfq_item->start_tag, dsfq_item->finish_tag,
		//		dsfq_item->weight, dsfq_item->app_index+1,s_pool.socket_state_list[i].ip, dsfq_queue_size-1);

		dsfq_stats[dsfq_item->app_index].dsfq_app_dispatch++;
/*		for (i=0;i<dsfq_num_apps;i++)
		{
			fprintf(stderr,"app %i:%i;",i, dsfq_app_dispatch[i]);
		}
		fprintf(stderr,"\n");*/

		dsfq_queue_size--;
		int app_index=s_pool.socket_state_list[i].app_index;
		app_stats[app_index].dispatched_requests+=1;
	}
	else
	{
		dsfq_current_depth--;
	}

	pthread_mutex_unlock(&dsfq_queue_mutex);

	return next_item;
}



int dsfq_init()//
{

	pthread_mutex_init(&dsfq_queue_mutex, NULL);
	pthread_mutex_init(&dsfq_accept_mutex, NULL);
	pthread_cond_init (&dsfq_accept_cond, NULL);
	pthread_mutex_init(&dsfq_broadcast_mutex, NULL);
	pthread_cond_init (&dsfq_broadcast_complete_cond, NULL);
	pthread_cond_init (&dsfq_broadcast_begin_cond,NULL);
	pthread_cond_init (&dsfq_threshold_cond,NULL);

	//read threshold from config file

	if (!dsfq_parse_server_config())
	{
		fprintf(stderr, "reading config is successful\n");

	}
	else
	{
		fprintf(stderr, "error reading config\n");
		exit(-1);
	}
	char filename[100];
	snprintf(filename,100, "dsfq.%s.b.txt",log_prefix);
	fprintf(stderr,"bfile:%s\n",filename);
	broadcast_file = fopen(filename, "w+");

	/* assuming a dictionary * dict = iniparser_load(config_s) has been loaded
	 * execute num_apps = iniparser_getint(dict, "apps:count" ,0) for your own use
	 */



	dsfq_llist_queue=PINT_llist_new();

	//parse other server information now!

        dsfq_stats= (struct dsfq_statistics*)malloc(sizeof(struct dsfq_statistics)*num_apps);

        dsfq_last_finish_tags= (int*)malloc(sizeof(int)*num_apps);
        memset(dsfq_stats,0,sizeof(struct dsfq_statistics)*num_apps);
	memset(dsfq_last_finish_tags,0,sizeof(int)*num_apps);

	int i;

	//initialize listening ports and connection to other proxies
	//retry every 1 second

	broadcast_buffer=(struct proxy_message **)malloc(sizeof(struct proxy_message *)*other_server_count);//receive queue
	receive_buffer=(struct proxy_message **)malloc(sizeof(struct proxy_message *)*other_server_count);//receive queue

	for (i=0;i<other_server_count;i++)
	{
		broadcast_buffer[i]=(struct proxy_message *)malloc(sizeof(struct proxy_message)*num_apps);
		receive_buffer[i]=(struct proxy_message *)malloc(sizeof(struct proxy_message)*num_apps);
	}
	

	client_ips = (char**)malloc(other_server_count*sizeof(char*));

	broadcast_buffer_size = (int*)malloc(sizeof(int )*other_server_count);
	receive_buffer_size = (int*)malloc(sizeof(int )*other_server_count);

	broadcast_sockets= (struct pollfd *) malloc(other_server_count*sizeof(struct pollfd));//we need the same number of sockets to receive
	receive_sockets= (struct pollfd *) malloc(other_server_count*sizeof(struct pollfd));//we need the same number of sockets to receive
	memset(broadcast_sockets, 0,other_server_count*sizeof(struct pollfd) );
	memset(receive_sockets, 0,other_server_count*sizeof(struct pollfd) );
	memset(broadcast_buffer_size, 0, sizeof(int )*other_server_count);
	memset(receive_buffer_size, 0, sizeof(int )*other_server_count);

	for (i=0;i<other_server_count;i++)
	{
		broadcast_buffer[i]=(struct proxy_message*)malloc(sizeof(struct proxy_message *)*num_apps);
	}

	long int listen_socket=BMI_sockio_new_sock();//setup listening socket

	int ret=BMI_sockio_set_sockopt(listen_socket,SO_REUSEADDR, 1);//reuse immediately
	if (ret==-1)
	{
		fprintf(stderr, "Error setting SO_REUSEADDR: %i when starting proxy communicating listening socket\n",ret);

	}
	ret = BMI_sockio_bind_sock(listen_socket,dsfq_proxy_listen_port);//bind to the specific port
	if (ret==-1)
	{
		fprintf(stderr, "Error binding socket: %li\n",listen_socket);
		return 1;
	}

	if (listen(listen_socket, BACKLOG) == -1) {
		fprintf(stderr, "Error listening on socket %li\n", listen_socket);
		return 1;
	}
	fprintf(stderr, "Proxy communication started, waiting on message...%li\n",listen_socket);
	int rc;
	pthread_t thread_accept;

	rc = pthread_create(&thread_accept, NULL, dsfq_proxy_accept_work, (void*)listen_socket);
	if (rc){
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}


	/* start contacting other proxies */
	PINT_llist_p proxy_entry;
	if (other_server_names)
		proxy_entry = other_server_names->next;

	for (i=0;i<other_server_count;i++)
	{
		if (proxy_entry==NULL)
		{
			fprintf(stderr, "%ith proxy name entry does not exist!\n", i+1);
			break;
		}
		int server_socket = BMI_sockio_new_sock();
		char * peer_ip = (char*)malloc(25);
		int peer_port;
		char * peer_name = ((char*)(proxy_entry->item));
		fprintf(stderr,"connecting to other proxy:%s, %s\n", peer_name,other_server_address[i]);
		int ret=0;
		proxy_retry:
		if ((ret=BMI_sockio_connect_sock(server_socket,other_server_address[i],dsfq_proxy_listen_port,peer_ip, &peer_port))!=server_socket)//we establish a pairing connecting to our local server
		//begin to contact other proxies in other servers
		{
			fprintf(stderr, "connecting to proxy failed, %i\n",ret);
			sleep(1);
			goto proxy_retry;

		}
		else
		{
			fprintf(stderr,"successful, peer_ip:%s peer_port: %i\n", peer_ip, peer_port);
			broadcast_sockets[i].fd=server_socket;
			broadcast_sockets[i].events=POLLOUT;
		}
		proxy_entry = proxy_entry->next;
	}
	//try to wait other proxies' connection

	/* default retry time is 1 second
	 * loop here; don't exit until all the sockets have been established
	 * llist of outgoing server sockets with names for searching and sockets to send
	 * combine the values of all applications together in a single message, checking threshold value of the sum of all applications
	 * double buffering enables the proxy to be able to receive message while sending the ready messages(blocking until all the servers are notified)
	 * make num_apps copies for each server so that this message progress can be polled and track each server's broadcasted status
	 * */

	pthread_mutex_lock(&dsfq_accept_mutex);

	while (!accept_ready)
	{
		fprintf(stderr,"waiting for accept all to finish\n");
		pthread_cond_wait(&dsfq_accept_cond, &dsfq_accept_mutex);
	}

	int j;
	for (j=0;j<other_server_count;j++)
	{
		fprintf(stderr,"socket:%i\n",broadcast_sockets[j].fd);
	}


	pthread_mutex_unlock(&dsfq_accept_mutex);

	fprintf(stderr,"wait on other proxies' initialization is over\n");

	rc=0;
	pthread_t thread_broadcast_work;
	rc = pthread_create(&thread_broadcast_work, NULL, dsfq_proxy_broadcast_work, NULL);
	if (rc){
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
	
	pthread_t thread_receive_work;
	rc = pthread_create(&thread_receive_work, NULL, dsfq_proxy_receive_work, NULL);
	if (rc){
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
	pthread_t thread_coordinator_work;
	rc = pthread_create(&thread_coordinator_work, NULL, dsfq_broadcast_trigger_work, NULL);
	if (rc){
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}

}


/*
 * broadcast_recieve queue's enqueue function
 * when we get each item from the receive queue, several things needs to be done:
 * 1. add this value to the receive array (app1, app2, app3) though same value is added to each variable, they'll be consumed at a different time
 *    when each app's request arrives and got applied by the delay function and cleared out.
 * 2. thus, receive queue is really not a queue for each app
 * 3. receive queue's each variable just got added up by every broadcasted message arrvied
 * 4. broadcast is based on request completion
 * */

void* dsfq_proxy_accept_work(void * t_arg)
{

	struct sockaddr_storage their_addr;
	//struct sockaddr_in their_addr;
	socklen_t tsize;
	int i;
	int listen_socket=(long int)t_arg;

	pthread_mutex_lock(&dsfq_accept_mutex);
	//for (i=other_server_count;i<0;i++)
	for (i=0;i<other_server_count;i++)
	{
		fprintf(stderr, "trying to accept...%i/%i\n", i+1, other_server_count);
		int client_socket = accept(listen_socket, (struct sockaddr *)&their_addr, &tsize);
		if (client_socket <0) {
			fprintf(stderr,"Error accepting %i\n",client_socket);
			return (void *)1;
		}

	    int oldfl = fcntl(client_socket, F_GETFL, 0);
	    if (!(oldfl & O_NONBLOCK))
	    {
	    	fcntl(client_socket, F_SETFL, oldfl | O_NONBLOCK);
	    	//fprintf(stderr,"setting client socket to nonblock\n");
	    }
	    int optval=1;
	    if (setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval))==-1)
	    {

	    	fprintf(stderr,"error setsockopt on client socket\n");
	    	exit(-1);
	    }
		getpeername(client_socket, (__SOCKADDR_ARG) &their_addr, &tsize);
		unsigned int addr=((struct sockaddr_in *)&their_addr)->sin_addr.s_addr;
		//Dprintf(L_NOTICE, "Socket from IP: %ui.%ui.%ui.%ui:%i\n", addr>>24, (addr<<8)>>24,(addr<<16)>>24, (addr<<24)>>24,
				//((struct sockaddr_in *)&their_addr)->sin_port
		//);

		fprintf(stderr,"Socket from IP: %u.%u.%u.%u:%u\n",
				(addr<<24)>>24,	(addr<<16)>>24,(addr<<8)>>24, addr>>24,
			ntohs(((struct sockaddr_in *)&their_addr)->sin_port));
		char * client_ip = (char*)malloc(25);

		int l = sizeof (their_addr);
		int client_port=ntohs(((struct sockaddr_in *)&their_addr)->sin_port);
		int ssize = sprintf(client_ip,"%u.%u.%u.%u",(addr<<24)>>24,	(addr<<16)>>24,(addr<<8)>>24, addr>>24);
		client_ips[i]=client_ip;
		receive_sockets[i].fd=client_socket;
		receive_sockets[i].events=POLLIN;
	}
	accept_ready=1;
	pthread_cond_signal(&dsfq_accept_cond);
	//fprintf(stderr,"signaled accept_ready\n");
	pthread_mutex_unlock(&dsfq_accept_mutex);
}

void* dsfq_broadcast_trigger_work (void* t_arg)
{
	//watch the threshold
	//awaken by :	update of receive queue (POLL_IN)
	//				completion of last broadcast (POLL_OUT)

	while (1)
	{

		pthread_mutex_lock(&dsfq_broadcast_mutex);

		//fprintf(stderr,"coordinator working\n");
		//int broadcast_complete=1;	//waited by broadcast coordinator, signaled by broadcast worker
		//int accept_ready=0;			//waited by dsfq_init, signaled by accept thread
		//int broadcast_begin=0;		//waited by broadcast worker, signaled by broadcast coordinator
		//int threshold_ready=0;		//waited by broadcast coordinator, signaled by receive thread
		while (!broadcast_complete)
		{
			//fprintf(stderr,"coordinator waiting for broadcast completion signal\n");
			pthread_cond_wait(&dsfq_broadcast_complete_cond, &dsfq_broadcast_mutex);
		}

		while (!threshold_ready)
		{
			//fprintf(stderr,"coordinator waiting for threshold signal to be triggered\n");
			pthread_cond_wait(&dsfq_threshold_cond, &dsfq_broadcast_mutex);
		}

		struct proxy_message * new_msg = (struct proxy_message *)malloc(num_apps*sizeof(struct proxy_message));

		int i;

		for (i=0;i<num_apps;i++)
		{
			new_msg[i].app_index=i;
			new_msg[i].period_count=dsfq_stats[i].request_receive_queue;
			//fprintf(stderr,"%s: Preparing %i bytes for app %i#\n",log_prefix,request_receive_queue[i] ,i);
		}

		for (i=0;i<other_server_count;i++)
		{
			memcpy(broadcast_buffer[i], new_msg, num_apps*sizeof(struct proxy_message));
			broadcast_buffer_size[i]=num_apps*sizeof(struct proxy_message);
		}

		broadcast_complete=0;
		threshold_ready=0;

		//clearing previous dirty counters

		for (i=0;i<num_apps;i++)
		{
			dsfq_stats[i].request_receive_queue=0;
		}

		dsfq_current_io_counter=0;

		broadcast_begin=1;
		pthread_cond_signal(&dsfq_broadcast_begin_cond);
		//fprintf(stderr, "coordinator just sent broadcast_begin signal\n");
		pthread_mutex_unlock(&dsfq_broadcast_mutex);
	}
}

void* dsfq_proxy_broadcast_work(void * t_arg)
{

	fprintf(stderr,"hello from broadcast framework by the proxy\n");
	while (1)
	{
		int i;

		pthread_mutex_lock(&dsfq_broadcast_mutex);

		//if broadcast condition is met, but broadcast
		while (!broadcast_begin)
		{
			//fprintf(stderr, "broadcast worker is waiting on begin signal\n");
			pthread_cond_wait(&dsfq_broadcast_begin_cond, &dsfq_broadcast_mutex);
		}
		//fprintf(stderr,"Broadcast working...\n");


		pthread_mutex_unlock(&dsfq_broadcast_mutex);

		int num_poll=0;
		begin_broadcast:
		//fprintf(stderr,"starting polling for broadcasting\n");
		num_poll=poll(broadcast_sockets,other_server_count,-1);//poll the whole pool using a wait time of 5 milliseconds at most.
		//fprintf(stderr,"polling returned with %i\n", num_poll);
		int total_sent_size=0;
		if (num_poll>0)
		{
			//Dprintf(D_CALL,"%i detected\n", num_poll);
			for (i=0;i<other_server_count;i++)//iterate through the whole socket pool for returned events
			{
				if (broadcast_sockets[i].revents & POLLOUT)
				{
					//Dprintf(L_NOTICE, "processing %ith socket %i for read\n",i,s_pool.poll_list[i].fd);
					int sent_size = send(broadcast_sockets[i].fd, broadcast_buffer[i],broadcast_buffer_size[i], 0);
					if (sent_size!=-1)
					{
						//fprintf(stderr, "send returned %i\n",sent_size);

						if (broadcast_buffer_size[i]!=sent_size)
						{
							memmove(broadcast_buffer[i], broadcast_buffer[i]+sent_size, broadcast_buffer_size[i]-sent_size);
						}
						broadcast_buffer_size[i]-=sent_size;
						total_sent_size+=sent_size;

					}
					else
					{
						fprintf(stderr,"send error!\n");
						dump_stat(0);
					}
					//try to update the queue as soon as possible
				}
			}
		}

		if (total_sent_size>0)
		{
			if (pthread_mutex_lock (&counter_mutex)!=0)
			{
				fprintf(stderr, "error locking when getting counter on throughput\n");
			}
				broadcast_amount+=total_sent_size;
			if (pthread_mutex_unlock (&counter_mutex)!=0)
			{
				fprintf(stderr, "error locking when unlocking counter throughput\n");
			}
		}

		int complete_flag=1;
		for (i=0;i<other_server_count;i++)//broadcast events
		{
			if (broadcast_buffer_size[i]>0)
			{
				complete_flag=0;
				break;
			}

		}

		if (complete_flag)
		{
			pthread_mutex_lock(&dsfq_broadcast_mutex);
			broadcast_begin=0;
			broadcast_complete=1;
			//fprintf(stderr,"broadcast complete signaled\n");
			pthread_cond_signal(&dsfq_broadcast_complete_cond);
			pthread_mutex_unlock(&dsfq_broadcast_mutex);
		}
		else
		{
			goto begin_broadcast;
		}
	}
	pthread_exit(NULL);

}


int print_dsfq_items_simple(void * item)
{
	struct generic_queue_item * generic_item =(struct generic_queue_item * ) item;
	struct dsfq_queue_item * dsfq_item = (struct dsfq_queue_item *)(generic_item->embedded_queue_item);
	fprintf(stderr,"%i",dsfq_item->app_index);
}

int print_dsfq_items_more(void * item)
{
	struct generic_queue_item * generic_item =(struct generic_queue_item * ) item;
	struct dsfq_queue_item * dsfq_item = (struct dsfq_queue_item *)(generic_item->embedded_queue_item);
	fprintf(stderr,"%i(%li,%i)[%i,%i]",dsfq_item->app_index, generic_item->item_id, dsfq_item->stream_id, dsfq_item->start_tag, dsfq_item->finish_tag);
}

void* dsfq_proxy_receive_work(void * t_arg)
{
	fprintf(stderr,"hello from receive framework by the proxy\n");
	while (1)
	{
		int i;

		//fprintf(stderr,"starting polling for receiving\n");

		int num_poll=poll(receive_sockets,other_server_count,-1);//poll the whole pool using a wait time of 5 milliseconds at most.
		//fprintf(stderr,"polling returned with %i\n", num_poll);

		if (num_poll>0)
		{
			//Dprintf(D_CALL,"%i detected\n", num_poll);
			for (i=0;i<other_server_count;i++)//iterate through the whole socket pool for returned events
			{
				if (receive_sockets[i].revents & POLLIN)
				{
					//receive whole fix-lengthed message
					//update the queue/resort

					int recv_size = recv(receive_sockets[i].fd, receive_buffer[i]+receive_buffer_size[i], num_apps*sizeof(struct proxy_message)-receive_buffer_size[i], 0);
					//fprintf(stderr, "receive returned %i\n", recv_size);
					if (recv_size!=-1 && recv_size!=0)
					{
						receive_buffer_size[i]+=recv_size;
					}
					else
					{
						fprintf(stderr, "fatal: proxy recv error!\n");
						dump_stat(0);
						//exit(-1);

					}

					if (receive_buffer_size[i]!=num_apps*sizeof(struct proxy_message))
					{
						//fprintf(stderr, "failed to receive the whole message %i\n", receive_buffer_size[i]);
					}
					else
					{
						receive_buffer_size[i]=0;
						int j;

						for (j=0;j<num_apps;j++)
						{
							/*lock queue here!*/

							//fprintf(stderr,"%s: app %i got %i from other server this time\n",log_prefix, receive_buffer[i][j].app_index,receive_buffer[i][j].period_count);
							//receive buffer is per server; however the values are saved per app


							//apply delay function in each poll call
							//re-sort

							/*end locking*/
							pthread_mutex_lock(&dsfq_queue_mutex);
							int app_index = receive_buffer[i][j].app_index;

							dsfq_stats[app_index].dsfq_delay_values+=receive_buffer[i][j].period_count;
							app_stats[app_index].app_throughput+=receive_buffer[i][j].period_count;
							total_throughput+=receive_buffer[i][j].period_count;
						    app_stats[app_index].byte_counter=app_stats[app_index].byte_counter+receive_buffer[i][j].period_count;

							//fprintf(stderr,"%s: app %i got %i from other server in total\n",log_prefix, receive_buffer[i][j].app_index,dsfq_delay_values[receive_buffer[i][j].app_index]);
							pthread_mutex_unlock(&dsfq_queue_mutex);
							//delay values are used locally
						}//eager mode can update the queue here
					}//end receiving a complete message
				}//end processing existing revents
			}//end receiving from every other proxy


		}//end num_poll>0
	}
	pthread_exit(NULL);
}

//this is called in the main proxy loop when a request or flow is totally completed
//mainly for dsfq broadcasting preparation/local scheduler debugging and statistics
int dsfq_update_on_request_completion(void* arg)
{
	struct complete_message * complete = (struct complete_message *)arg;
	struct generic_queue_item * current_item = (complete->current_item);
	struct dsfq_queue_item* dsfq_item = (struct dsfq_queue_item*)(current_item->embedded_queue_item);

	int app_index = dsfq_item->app_index;
	//pthread_mutex_lock(&dsfq_broadcast_mutex);
	//value taken by broadcast thread and reset to 0

	//pthread_mutex_unlock(&dsfq_broadcast_mutex);


	//fprintf(stderr,"app %i's counter got added by %i to %i; total counter is now %i\n",
	//dsfq_item->app_index, complete->complete_size, request_receive_queue[dsfq_item->app_index], dsfq_current_io_counter);

	dsfq_item->got_size+=complete->complete_size;


/*
	Dprintf(D_CACHE,"adding [Current Item %s:%i]:%i/%i\n",
			dsfq_item->data_ip, dsfq_item->data_port,
			dsfq_item->got_size,dsfq_item->task_size);
*/


	if (dsfq_item->got_size==dsfq_item->task_size)
	{
		return dsfq_item->got_size;
	}

	if (dsfq_item->task_size<dsfq_item->got_size)
	{
		return -1;
	}

	return 0;

	//also update the queue item's got size field
	//check completion, hen call getn next prototype returns int?

}

int dsfq_parse_configs()
{

}

int dsfq_fill_ip(void * item)
{
	char * char_item = (char*)item;
	fprintf(stderr,"filling %s to %ith slot\n",char_item, fill_ip_count);


	int status;
	struct addrinfo hints, *p;
	struct addrinfo * servinfo;
	char ipstr[INET6_ADDRSTRLEN];
	memset(&hints, 0, sizeof hints);
	hints.ai_family=AF_UNSPEC;
	hints.ai_socktype=SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	if ((status=getaddrinfo(char_item, NULL, &hints, &servinfo))!=0)
	{

		fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
		//exit(1);
	}
	else
	{
		fprintf(stderr,"IP addresses for %s:\n\n", char_item);
		if (p=servinfo)
		{
			struct sockaddr_in * in_addr = (struct sockaddr_in *) malloc(sizeof(struct sockaddr_in));
			char *ipver;

			if (p->ai_family==AF_INET)
			{
				struct sockaddr_in * ipv4=(struct sockaddr_in *)p->ai_addr;
				memcpy(in_addr, &(ipv4->sin_addr), sizeof(struct sockaddr_in));
				other_server_ips[fill_ip_count]=in_addr;
				ipver="IPV4";
			}
			else
			{
/*				struct sockaddr_in6 * ipv6=(struct sockaddr_in6 *)p->ai_addr;
				addr = &(ipv6->sin6_addr);
				ipver="IPV6";*/
				fprintf(stderr,"Unrecognized address\n");
				exit(1);
			}


			inet_ntop(p->ai_family, in_addr, ipstr, sizeof ipstr);

			char* stored_ipstr = (char*)malloc(INET6_ADDRSTRLEN);
			memcpy(stored_ipstr, ipstr, strlen(ipstr)+1);
			stored_ipstr[strlen(ipstr)]='\0';
			other_server_address[fill_ip_count]=stored_ipstr;
			fprintf(stderr, " %s: %s\n", ipver, ipstr);

		}
		freeaddrinfo(servinfo);
	}


	fill_ip_count++;

	//fprintf(stderr,"resolving %s\n",inet_ntoa(*((struct in_addr*)(other_server_ips[fill_ip_count]->h_addr))));

	return fill_ip_count;
}

int dsfq_parse_server_config()
{

	char myhost[100];
	fprintf(stderr, "host name returned %i\n",gethostname(myhost, 100));


	FILE * server_config = fopen(pvfs_config,"r");
	if (!server_config)
	{
		fprintf(stderr, "opening %s error\n", pvfs_config);
		return -1;
	}
	char line[10000];
	int ret=0;
	if (ret=fread(line, 1, 9999, server_config))
	{
		fprintf(stderr,"returning %i\n",ret);
		line[ret]='\0';

	}
	char * begin = strtok(line, "\n");
	char * alias, * alias2;

	other_server_names = PINT_llist_new();
	while (begin)
	{

		if (alias=strstr(begin, "Alias "))
		{
			alias2 = strstr(alias+6, " ");

			char * tmp = (char*)malloc((alias2-alias-6+1)*sizeof (char));
			tmp[alias2-alias-6]='\0';
			memcpy(tmp,alias+6,alias2-alias-6);
			if (!strcmp(myhost, tmp))
			{
				fprintf(stderr,"excluding same host name from server config: %s\n", myhost);

			}
			else
			{
				PINT_llist_add_to_tail(other_server_names, tmp);
				other_server_count++;
			}
		}
		begin = strtok(NULL, "\n");
	}



	other_server_ips = (struct sockaddr_in  **)malloc(sizeof(struct sockaddr_in  *)*other_server_count);
	other_server_address = (char**)malloc(sizeof(char*)*other_server_count);
	PINT_llist_doall(other_server_names, dsfq_fill_ip);

	fclose(server_config);
	return 0;

	//read server names
	//use resolve to resolve ip address


}

int dsfq_is_idle()
{

	return PINT_llist_empty( dsfq_llist_queue);
}


int dsfq_add_ttl_tp(int this_amount, int app_index)
{
	total_throughput+=this_amount;
    app_stats[app_index].byte_counter=app_stats[app_index].byte_counter+this_amount;
    app_stats[app_index].app_throughput=app_stats[app_index].app_throughput+this_amount;


}

const struct scheduler_method sch_dsfq = {
    .method_name = DSFQ_SCHEDULER,
    .work_conserving = 1,
    .sch_initialize = dsfq_init,
    .sch_finalize = NULL,
    .sch_enqueue = dsfq_enqueue,
    .sch_dequeue = dsfq_dequeue,
    .sch_load_data_from_config = dsfq_load_data_from_config,
    .sch_update_on_request_completion = dsfq_update_on_request_completion,
    .sch_get_scheduler_info = dsfq_get_scheduler_info,
    .sch_is_idle = dsfq_is_idle,
    .sch_add_ttl_throughput = dsfq_add_ttl_tp,
    .sch_self_dispatch = 0
};

/*
 * (C) 2009-2012 Florida International University
 *
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * See COPYING in top-level directory.
 *
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
#include "scheduler_vDSFQ2.h"
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
#define DEFAULT_VDSFQ2_BROADCAST_THRESHOLD 1//app connections
#define VBROADCAST_QUEUE_SIZE2 200 //a little larger than # clients
#define VMAX_SERVER_NUM2 10
#define VRECEIVE_QUEUE_SIZE2 (VBROADCAST_QUEUE_SIZE2 * VMAX_SERVER_NUM2)
#define DEFAULT_VDSFQ2_DEPTH 1
#define DEFAULT_VDSFQ2_MIN_SHARE 0.5//which means that for apps, the shares are at least 1
#define VREDUCER2 1024
#define VDSFQ2_TIMEOUT 3
extern int total_weight;
int vdsq2_timeout_max=VDSFQ2_TIMEOUT;

pthread_mutex_t vdsfq2_accept_mutex;
pthread_mutex_t vdsfq2_queue_mutex;
pthread_cond_t vdsfq2_accept_cond;
pthread_mutex_t vdsfq2_broadcast_mutex;
pthread_cond_t vdsfq2_broadcast_complete_cond;
pthread_cond_t vdsfq2_broadcast_begin_cond;
pthread_cond_t vdsfq2_threshold_cond;

int vdsfq2_item_id=0;
PINT_llist_p vother_server_names2;
char** vother_server_address2;
struct sockaddr_in  ** vother_server_ips2;
int* vdsfq2_last_finish_tags;
int vdsfq2_depth=DEFAULT_VDSFQ2_DEPTH;
double vdsfq2_min_share = DEFAULT_VDSFQ2_MIN_SHARE;
int vdsfq2_current_depth=0;
int vdsfq2_threshold=DEFAULT_VDSFQ2_BROADCAST_THRESHOLD;//
int vdsfq2_current_io_counter=0;
int vdsfq2_virtual_time=0;
PINT_llist_p vreceive_message2;
PINT_llist_p vbroadcast_message2;
//struct proxy_queue receive_queue;
//struct proxy_queue broadcast_queue;
struct vdsfq2_statistics * vdsfq2_stats;

//put them in a struct
PINT_llist_p vdsfq2_llist_queue;
//int dsfq_stream_id=0;

int vdsfq2_purecost=1;

int vother_server_count2=0;
int vfill_ip_count2=0;

int vdsfq2_queue_size=0;

char ** vclient_ips2;


//int* broadcast_queue;//don't know what it is
//int* request_receive_queue;//from client
//int* dsfq_delay_values;//proxy receive queue, we need this for lazy receive/update?
//int* dsfq_delay_value_bitmap;//proxy receive queue, we need this for lazy receive/update?
//int * dsfq_last_applied_delay;
//int *dsfq_app_dispatch;
//int *dsfq_last_finish_tags;
struct vproxy_message2 ** vbroadcast_buffer2;
struct vproxy_message2 ** vreceive_buffer2;
int* vbroadcast_buffer_size2;
int* vreceive_buffer_size2;
int * vdsfq2_last_applied_item_ids;

int * vapp_entries_local2, *vapp_entries_remote2;
int vdsfq2_timeout=0;
int vdsfq2_left=1;
int vdsfq2_left2=1;

int vbroadcast_complete2=1;	//waited by broadcast coordinator, signaled by broadcast worker
int vaccept_ready2=0;			//waited by dsfq_init, signaled by accept thread
int vbroadcast_begin2=0;		//waited by broadcast worker, signaled by broadcast coordinator
int vthreshold_ready2=0;		//waited by broadcast coordinator, signaled by receive thread

int vdsfq2_proxy_listen_port=DEFAULT_VDSFQ2_PROXY_PORT;

struct pollfd * vbroadcast_sockets2, *vreceive_sockets2;

void* vdsfq2_proxy_broadcast_work(void * t_arg);
void* vdsfq2_proxy_receive_work(void * t_arg);
void* vdsfq2_proxy_accept_work(void * t_arg);
void* vdsfq2_broadcast_trigger_work (void* t_arg);
int vdsfq2_load_data_from_config (dictionary * dict);

int vdsfq2_load_data_from_config (dictionary * dict)
{

	vdsfq2_depth=iniparser_getint(dict, "VDSFQ2:depth" ,vdsfq2_depth);
	vdsfq2_proxy_listen_port=iniparser_getint(dict,"VDSFQ2:listen_port",vdsfq2_proxy_listen_port);
	vdsfq2_threshold=iniparser_getint(dict, "VDSFQ2:threshold", vdsfq2_threshold);
	vdsfq2_min_share=iniparser_getdouble(dict, "VDSFQ2:min_share", vdsfq2_min_share);

	if (vdsfq2_depth<=0)
	{
		fprintf(stderr, "wrong depth\n");
		exit(-1);

	}
	if (vdsfq2_proxy_listen_port<=1024 || vdsfq2_proxy_listen_port>65535)
	{
		fprintf(stderr, "wrong port\n");
		exit(-1);

	}
	if (vdsfq2_proxy_listen_port==active_port)
	{
		fprintf(stderr, "conflict port with proxy\n");
		exit(-1);
	}
	if (vdsfq2_threshold<=0)
	{
		fprintf(stderr, "wrong threshold\n");
		exit(-1);

	}
	if (vdsfq2_min_share<=0.0001f)
	{
		fprintf(stderr, "wrong minimum share\n");
		exit(-1);

	}
	fprintf(stderr,"VDSFQ2 using depth:%i\n",vdsfq2_depth);
	fprintf(stderr,"VDSFQ2 using port:%i\n",vdsfq2_proxy_listen_port);
	//vdsfq_threshold*=1024;
	fprintf(stderr,"VDSFQ2 using threshold:%i\n",vdsfq2_threshold);
	fprintf(stderr,"VDSFQ2 using min_share:%f for HYBRID_DSFQ\n",vdsfq2_min_share);
}

int vlist_req_comp2(void * a, void * b)
{
	return !(((long int)a)==((struct generic_queue_item *)b)->item_id);
}

int vlist_pmsg_print_all2(void * item)
{
	//fprintf(stderr, "item address:%#010x\n", (((struct generic_queue_item *)item)->embedded_queue_item));
	return 0;
}

void vdsfq2_get_scheduler_info()
{
	//Dprintf(D_CACHE,"depth remains at %i\n", dsfq_current_depth);
	//Dprintf(D_CACHE,"current queue has %i items\n",dsfq_queue_size);
}

int vdsfq2_enqueue(struct socket_info * si, struct pvfs_info* pi)
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

	struct vdsfq2_queue_item * item =  (struct vdsfq2_queue_item * )malloc(sizeof(struct vdsfq2_queue_item));

	//Dprintf(D_CACHE,"weight got from request socket is %i,ip %s\n",weight,ip);
	pthread_mutex_lock(&vdsfq2_broadcast_mutex);

	/*
	 * get time difference too, because there may be some slow apps
	 * we want to send out the information with a time out condition too.
	 * */
	//0 means never got this app
	//1 means it has been updated but not flushed to other servers
	//2 means it has been updated and has been broadcasted

	int needs_broadcast=0;

	if (vapp_entries_local2[app_index]==0)//if no app has contacted this server yet then we have a
	{
		vdsfq2_left2=1;//if this time it has not reached a timeout yet, next time we will consider....
		vdsfq2_stats[app_index].request_receive_queue=1;//not accumulating actual I/O bytes any more...
		//make sure that this value gets updated to 1 only once. after broadcasted, it will be set to 0 and never be updated
		vdsfq2_current_io_counter+=1;
		vapp_entries_local2[app_index]=1;
		fprintf(stderr,"%s server is serving app %i\n", log_prefix, app_index);
	}

	if (vdsfq2_left2 == 1 && vdsfq2_timeout==1)
	{needs_broadcast=1;}

	if (vdsfq2_current_io_counter>=vdsfq2_threshold)
	{
		needs_broadcast=1;
	}
	if (needs_broadcast==1)
	{
		fprintf(stderr,"%s: counter reached %i <> %i; timeout: %i, left: %i\n", log_prefix, vdsfq2_current_io_counter, vdsfq2_threshold, vdsfq2_timeout, vdsfq2_left);
		vthreshold_ready2=1;
		pthread_cond_signal(&vdsfq2_threshold_cond);
	}
	pthread_mutex_unlock(&vdsfq2_broadcast_mutex);


	pthread_mutex_lock(&vdsfq2_queue_mutex);

	item->delay_value=vapp_entries_remote2[app_index];//vdsfq_stats[app_index].vdsfq_delay_values;
	//actually we can also use stats

	//vdsfq_entries_remote

	//vdsfq_stats[app_index].vdsfq_delay_values=0;
	//we don't need to reset it to 0 any more!'

	//fprintf(stderr,"%s: setting app %i value to zero\n", log_prefix, app_index);
	item->weight=s_pool.socket_state_list[r_socket_index].weight;
	int temp_delay = item->delay_value*length;
	//item->delay_value+=length;
	temp_delay/=(item->weight * VREDUCER2);

	//fprintf(stderr,"%s: app %i delay value applied is %i value to zero\n", log_prefix, app_index, item->delay_value);

	int start_tag=MAX(vdsfq2_virtual_time,vdsfq2_last_finish_tags[app_index]+temp_delay);//work-conserving

	//fprintf(stderr,"%s: virtual time: %i, finish tag: %i, delay value: %i  , taken value: %i\n",log_prefix, dsfq_virtual_time,dsfq_last_finish_tags[app_index], item->delay_value, start_tag);
	//int start_tag=last_finish_tags[app_index];//non-work-conserving
	//item->last_finish_tag=dsfq_last_finish_tags[app_index];
	//item->virtual_time=dsfq_virtual_time;
	int cost;
	if (vdsfq2_purecost)
	{
		cost=length;
	}
	else
	{
		cost=length;//*(dsfq_current_depth+1);
	}
	int finish_tag = start_tag+cost/weight/VREDUCER2;
	vdsfq2_last_finish_tags[app_index]=finish_tag;
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
	generic_item->item_id=vdsfq2_item_id++;
	generic_item->socket_data=si;

	if  (PINT_llist_add_to_tail(
			vdsfq2_llist_queue,
	    generic_item))
	{

		fprintf(stderr,"queue insertion error!\n");
		exit(-1);
	}

	//PINT_llist_doall(dsfq_llist_queue,list_pmsg_print_all);
	int dispatched=0;
	if (vdsfq2_current_depth<vdsfq2_depth)//meaning the queue must be empty before this insertion
	{
		dispatched=1;
		vdsfq2_current_depth++;
		//this means the queue is empty before adding item to it.
	}

	vdsfq2_queue_size++;

	if (dispatched)
	{
		//fprintf(stderr, "current_depth increased to %i\n",dsfq_current_depth);
		//fprintf(stderr, "dispatched immeidately\n");

	}
	else
	{
		//fprintf(stderr,"sorting after adding new item...\n");
		//fprintf(stderr,"service delayed\n");
		vdsfq2_llist_queue = PINT_llist_sort(vdsfq2_llist_queue, list_vdsfq_sort_comp);//sort
	}

	pthread_mutex_unlock(&vdsfq2_queue_mutex);


	return dispatched;

}

struct generic_queue_item * vdsfq2_dequeue(struct dequeue_reason r)
{
	pthread_mutex_lock(&vdsfq2_queue_mutex);
	PINT_llist_doall(vdsfq2_llist_queue,vlist_pmsg_print_all2);
	//get the head of the list, remove it from the head
	struct generic_queue_item * item  = (struct generic_queue_item *)PINT_llist_head(vdsfq2_llist_queue);
	//fprintf(stderr, "msg index is %i\n",head_message->msg_index);
	struct generic_queue_item * next_item = NULL;

	if (item!=NULL)
	{
		next_item = (struct generic_queue_item *)PINT_llist_rem(vdsfq2_llist_queue, (void*)item->item_id,  vlist_req_comp2);

	}

	if (next_item!=NULL)
	{
		struct vdsfq2_queue_item * vdsfq2_item = (struct vdsfq2_queue_item *)(next_item->embedded_queue_item);
		vdsfq2_virtual_time=vdsfq2_item->start_tag;


		vdsfq2_stats[vdsfq2_item->app_index].vdsfq_app_dispatch++;
/*		for (i=0;i<dsfq_num_apps;i++)
		{
			fprintf(stderr,"app %i:%i;",i, dsfq_app_dispatch[i]);
		}
		fprintf(stderr,"\n");*/

		vdsfq2_queue_size--;
		int app_index=next_item->socket_data->app_index;
		app_stats[app_index].dispatched_requests+=1;
	}
	else
	{
		vdsfq2_current_depth--;
	}

	pthread_mutex_unlock(&vdsfq2_queue_mutex);

	return next_item;
}



void* vdsfq2_timeout_work(void * t_arg)
{
	int* timeout = (int *)t_arg;

	fprintf(stderr,"sleep time is %i seconds\n", *timeout);
	while (1)
	{
		sleep(*timeout);
		if (vdsfq2_timeout==0)
		{
			fprintf(stderr,"%s Timeout begins!\n", log_prefix);
			vdsfq2_timeout=1;

		}
		else
		{

			fprintf(stderr,"%s Timeout already!\n", log_prefix);
		}

	}

}

int vdsfq2_init()//
{

	pthread_mutex_init(&vdsfq2_queue_mutex, NULL);
	pthread_mutex_init(&vdsfq2_accept_mutex, NULL);
	pthread_cond_init (&vdsfq2_accept_cond, NULL);
	pthread_mutex_init(&vdsfq2_broadcast_mutex, NULL);
	pthread_cond_init (&vdsfq2_broadcast_complete_cond, NULL);
	pthread_cond_init (&vdsfq2_broadcast_begin_cond,NULL);
	pthread_cond_init (&vdsfq2_threshold_cond,NULL);

	//read threshold from config file

	if (!vdsfq2_parse_server_config())
	{
		fprintf(stderr, "reading config is successful\n");

	}
	else
	{
		fprintf(stderr, "error reading config\n");
		exit(-1);
	}


	/* assuming a dictionary * dict = iniparser_load(config_s) has been loaded
	 * execute num_apps = iniparser_getint(dict, "apps:count" ,0) for your own use
	 */



	vdsfq2_llist_queue=PINT_llist_new();

	//parse other server information now!

	vdsfq2_stats= (struct vdsfq2_statistics*)malloc(sizeof(struct vdsfq2_statistics)*num_apps);
	vdsfq2_last_finish_tags= (int*)malloc(sizeof(int)*num_apps);
	vapp_entries_local2 = (int*)malloc(sizeof(int)*num_apps);
	vapp_entries_remote2 = (int*)malloc(sizeof(int)*num_apps);

	memset(vdsfq2_stats,0,sizeof(struct vdsfq2_statistics)*num_apps);
	memset(vdsfq2_last_finish_tags,0,sizeof(int)*num_apps);
	memset(vapp_entries_local2,0,sizeof(int)*num_apps);
	memset(vapp_entries_remote2,0,sizeof(int)*num_apps);

	int i;

	//initialize listening ports and connection to other proxies
	//retry every 1 second

	vbroadcast_buffer2=(struct vproxy_message2 **)malloc(sizeof(struct vproxy_message2 *)*vother_server_count2);//receive queue
	vreceive_buffer2=(struct vproxy_message2 **)malloc(sizeof(struct vproxy_message2 *)*vother_server_count2);//receive queue

	for (i=0;i<vother_server_count2;i++)
	{
		vbroadcast_buffer2[i]=(struct vproxy_message2 *)malloc(sizeof(struct vproxy_message2)*num_apps);
		vreceive_buffer2[i]=(struct vproxy_message2 *)malloc(sizeof(struct vproxy_message2)*num_apps);
	}
	

	vclient_ips2 = (char**)malloc(vother_server_count2*sizeof(char*));

	vbroadcast_buffer_size2 = (int*)malloc(sizeof(int )*vother_server_count2);
	vreceive_buffer_size2 = (int*)malloc(sizeof(int )*vother_server_count2);

	vbroadcast_sockets2= (struct pollfd *) malloc(vother_server_count2*sizeof(struct pollfd));//we need the same number of sockets to receive
	vreceive_sockets2= (struct pollfd *) malloc(vother_server_count2*sizeof(struct pollfd));//we need the same number of sockets to receive
	memset(vbroadcast_sockets2, 0,vother_server_count2*sizeof(struct pollfd) );
	memset(vreceive_sockets2, 0,vother_server_count2*sizeof(struct pollfd) );
	memset(vbroadcast_buffer_size2, 0, sizeof(int )*vother_server_count2);
	memset(vreceive_buffer_size2, 0, sizeof(int )*vother_server_count2);

	for (i=0;i<vother_server_count2;i++)
	{
		vbroadcast_buffer2[i]=(struct vproxy_message2*)malloc(sizeof(struct vproxy_message2 *)*num_apps);
	}

	long int listen_socket=BMI_sockio_new_sock();//setup listening socket

	int ret=BMI_sockio_set_sockopt(listen_socket,SO_REUSEADDR, 1);//reuse immediately
	if (ret==-1)
	{
		fprintf(stderr, "Error setting SO_REUSEADDR: %i when starting proxy communicating listening socket\n",ret);

	}
	ret = BMI_sockio_bind_sock(listen_socket,vdsfq2_proxy_listen_port);//bind to the specific port
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

	rc = pthread_create(&thread_accept, NULL, vdsfq2_proxy_accept_work, (void*)listen_socket);
	if (rc){
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}


	/* start contacting other proxies */
	PINT_llist_p proxy_entry;
	if (vother_server_names2)
		proxy_entry = vother_server_names2->next;

	for (i=0;i<vother_server_count2;i++)
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
		fprintf(stderr,"connecting to other proxy:%s, %s\n", peer_name,vother_server_address2[i]);
		int ret=0;
		proxy_retry:
		if ((ret=BMI_sockio_connect_sock(server_socket,vother_server_address2[i],vdsfq2_proxy_listen_port,peer_ip, &peer_port))!=server_socket)//we establish a pairing connecting to our local server
		//begin to contact other proxies in other servers
		{
			fprintf(stderr, "connecting to proxy failed, %i\n",ret);
			sleep(1);
			goto proxy_retry;

		}
		else
		{
			fprintf(stderr,"successful, peer_ip:%s peer_port: %i\n", peer_ip, peer_port);
			vbroadcast_sockets2[i].fd=server_socket;
			vbroadcast_sockets2[i].events=POLLOUT;
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

	pthread_mutex_lock(&vdsfq2_accept_mutex);

	while (!vaccept_ready2)
	{
		fprintf(stderr,"waiting for accept all to finish\n");
		pthread_cond_wait(&vdsfq2_accept_cond, &vdsfq2_accept_mutex);
	}

	int j;
	for (j=0;j<vother_server_count2;j++)
	{
		fprintf(stderr,"socket:%i\n",vbroadcast_sockets2[j].fd);
	}


	pthread_mutex_unlock(&vdsfq2_accept_mutex);

	fprintf(stderr,"wait on other proxies' initialization is over\n");

	rc=0;
	pthread_t thread_broadcast_work;
	rc = pthread_create(&thread_broadcast_work, NULL, vdsfq2_proxy_broadcast_work, NULL);
	if (rc){
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
	
	pthread_t thread_receive_work;
	rc = pthread_create(&thread_receive_work, NULL, vdsfq2_proxy_receive_work, NULL);
	if (rc){
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
	pthread_t thread_coordinator_work;
	rc = pthread_create(&thread_coordinator_work, NULL, vdsfq2_broadcast_trigger_work, NULL);
	if (rc){
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
	pthread_t thread_timeout;

	rc = pthread_create(&thread_timeout, NULL, vdsfq2_timeout_work, (void*)&vdsq2_timeout_max);
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

void* vdsfq2_proxy_accept_work(void * t_arg)
{

	struct sockaddr_storage their_addr;
	//struct sockaddr_in their_addr;
	socklen_t tsize;
	int i;
	int listen_socket=(long int)t_arg;

	pthread_mutex_lock(&vdsfq2_accept_mutex);
	//for (i=other_server_count;i<0;i++)
	for (i=0;i<vother_server_count2;i++)
	{
		fprintf(stderr, "trying to accept...%i/%i\n", i+1, vother_server_count2);
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
		vclient_ips2[i]=client_ip;
		vreceive_sockets2[i].fd=client_socket;
		vreceive_sockets2[i].events=POLLIN;
	}
	vaccept_ready2=1;
	pthread_cond_signal(&vdsfq2_accept_cond);
	//fprintf(stderr,"signaled accept_ready\n");
	pthread_mutex_unlock(&vdsfq2_accept_mutex);
}

void* vdsfq2_broadcast_trigger_work (void* t_arg)
{
	//watch the threshold
	//awaken by :	update of receive queue (POLL_IN)
	//				completion of last broadcast (POLL_OUT)

	while (1)
	{

		pthread_mutex_lock(&vdsfq2_broadcast_mutex);

		//fprintf(stderr,"coordinator working\n");
		//int broadcast_complete=1;	//waited by broadcast coordinator, signaled by broadcast worker
		//int accept_ready=0;			//waited by dsfq_init, signaled by accept thread
		//int broadcast_begin=0;		//waited by broadcast worker, signaled by broadcast coordinator
		//int threshold_ready=0;		//waited by broadcast coordinator, signaled by receive thread
		while (!vbroadcast_complete2)
		{
			//fprintf(stderr,"coordinator waiting for broadcast completion signal\n");
			pthread_cond_wait(&vdsfq2_broadcast_complete_cond, &vdsfq2_broadcast_mutex);
		}

		while (!vthreshold_ready2)
		{
			//fprintf(stderr,"coordinator waiting for threshold signal to be triggered\n");
			pthread_cond_wait(&vdsfq2_threshold_cond, &vdsfq2_broadcast_mutex);
		}

		struct vproxy_message2 * new_msg = (struct vproxy_message2 *)malloc(num_apps*sizeof(struct vproxy_message2));

		int i;int found=0;

		for (i=0;i<num_apps;i++)
		{

			new_msg[i].app_index=i;
			if (vapp_entries_local2[i]==1)
			{
				new_msg[i].period_count=vdsfq2_stats[i].request_receive_queue;
				vapp_entries_local2[i]=2;
			}
			else
			{
				new_msg[i].period_count=0;
				if (vdsfq2_left==1 && vapp_entries_local2[i]==0)
				{
					found=1;
					fprintf(stderr,"%s still has app %i not serving\n",log_prefix,i);
				}
			}

			vdsfq2_left2=0;//no left unsent apps
			if (found==0)
			{
				vdsfq2_left=0;//no left unserved apps

				fprintf(stderr, "%s all the apps have arrived\n", log_prefix);
			}

			//fprintf(stderr,"%s: Preparing %i bytes for app %i#\n",log_prefix,request_receive_queue[i] ,i);
		}

		for (i=0;i<vother_server_count2;i++)
		{
			memcpy(vbroadcast_buffer2[i], new_msg, num_apps*sizeof(struct vproxy_message2));
			vbroadcast_buffer_size2[i]=num_apps*sizeof(struct vproxy_message2);
		}

		vbroadcast_complete2=0;
		vthreshold_ready2=0;
		vdsfq2_timeout=0;

		//clearing previous dirty counters

		for (i=0;i<num_apps;i++)
		{
			vdsfq2_stats[i].request_receive_queue=0;
		}

		vdsfq2_current_io_counter=0;

		vbroadcast_begin2=1;
		pthread_cond_signal(&vdsfq2_broadcast_begin_cond);
		//fprintf(stderr, "coordinator just sent broadcast_begin signal\n");
		pthread_mutex_unlock(&vdsfq2_broadcast_mutex);
	}
}

void* vdsfq2_proxy_broadcast_work(void * t_arg)
{

	fprintf(stderr,"hello from broadcast framework by the proxy\n");
	while (1)
	{
		int i;

		pthread_mutex_lock(&vdsfq2_broadcast_mutex);

		//if broadcast condition is met, but broadcast
		while (!vbroadcast_begin2)
		{
			//fprintf(stderr, "broadcast worker is waiting on begin signal\n");
			pthread_cond_wait(&vdsfq2_broadcast_begin_cond, &vdsfq2_broadcast_mutex);
		}
		//fprintf(stderr,"Broadcast working...\n");

		pthread_mutex_unlock(&vdsfq2_broadcast_mutex);

		int num_poll=0;
		begin_broadcast:
		//fprintf(stderr,"starting polling for broadcasting\n");
		num_poll=poll(vbroadcast_sockets2,vother_server_count2,-1);//poll the whole pool using a wait time of 5 milliseconds at most.
		//fprintf(stderr,"polling returned with %i\n", num_poll);

		if (num_poll>0)
		{
			//Dprintf(D_CALL,"%i detected\n", num_poll);
			for (i=0;i<vother_server_count2;i++)//iterate through the whole socket pool for returned events
			{
				if (vbroadcast_sockets2[i].revents & POLLOUT)
				{
					//Dprintf(L_NOTICE, "processing %ith socket %i for read\n",i,s_pool.poll_list[i].fd);
					int sent_size = send(vbroadcast_sockets2[i].fd, vbroadcast_buffer2[i],vbroadcast_buffer_size2[i], 0);
					if (sent_size!=-1)
					{
						//fprintf(stderr, "send returned %i\n",sent_size);

						if (vbroadcast_buffer_size2[i]!=sent_size)
						{
							memmove(vbroadcast_buffer2[i], vbroadcast_buffer2[i]+sent_size, vbroadcast_buffer_size2[i]-sent_size);
						}
						vbroadcast_buffer_size2[i]-=sent_size;
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


		int complete_flag=1;
		for (i=0;i<vother_server_count2;i++)//broadcast events
		{
			if (vbroadcast_buffer_size2[i]>0)
			{
				complete_flag=0;
				break;
			}

		}

		if (complete_flag)
		{
			pthread_mutex_lock(&vdsfq2_broadcast_mutex);
			vbroadcast_begin2=0;
			vbroadcast_complete2=1;
			//fprintf(stderr,"broadcast complete signaled\n");
			pthread_cond_signal(&vdsfq2_broadcast_complete_cond);
			pthread_mutex_unlock(&vdsfq2_broadcast_mutex);
		}
		else
		{
			goto begin_broadcast;
		}
	}
	pthread_exit(NULL);

}


int print_vdsfq2_items_simple(void * item)
{
	struct generic_queue_item * generic_item =(struct generic_queue_item * ) item;
	struct vdsfq2_queue_item * dsfq2_item = (struct vdsfq2_queue_item *)(generic_item->embedded_queue_item);
	fprintf(stderr,"%i",dsfq2_item->app_index);
}

int print_vdsfq2_items_more(void * item)
{
	struct generic_queue_item * generic_item =(struct generic_queue_item * ) item;
	struct vdsfq2_queue_item * vdsfq2_item = (struct vdsfq2_queue_item *)(generic_item->embedded_queue_item);
	fprintf(stderr,"%i(%li,%i)[%i,%i]",vdsfq2_item->app_index, generic_item->item_id, vdsfq2_item->stream_id, vdsfq2_item->start_tag, vdsfq2_item->finish_tag);
}

void* vdsfq2_proxy_receive_work(void * t_arg)
{
	fprintf(stderr,"hello from receive framework by the proxy\n");
	while (1)
	{
		int i;

		//fprintf(stderr,"starting polling for receiving\n");

		int num_poll=poll(vreceive_sockets2,vother_server_count2,-1);//poll the whole pool using a wait time of 5 milliseconds at most.
		//fprintf(stderr,"polling returned with %i\n", num_poll);

		if (num_poll>0)
		{
			//Dprintf(D_CALL,"%i detected\n", num_poll);
			for (i=0;i<vother_server_count2;i++)//iterate through the whole socket pool for returned events
			{
				if (vreceive_sockets2[i].revents & POLLIN)
				{
					//receive whole fix-lengthed message
					//update the queue/resort

					int recv_size = recv(vreceive_sockets2[i].fd, vreceive_buffer2[i]+vreceive_buffer_size2[i], num_apps*sizeof(struct vproxy_message2)-vreceive_buffer_size2[i], 0);
					//fprintf(stderr, "receive returned %i\n", recv_size);
					if (recv_size!=-1 && recv_size!=0)
					{
						vreceive_buffer_size2[i]+=recv_size;
					}
					else
					{
						fprintf(stderr, "fatal: proxy recv error!\n");
						dump_stat(0);
						//exit(-1);

					}

					if (vreceive_buffer_size2[i]!=num_apps*sizeof(struct vproxy_message2))
					{
						//fprintf(stderr, "failed to receive the whole message %i\n", receive_buffer_size[i]);
					}
					else
					{
						vreceive_buffer_size2[i]=0;
						int j;

						for (j=0;j<num_apps;j++)
						{
							/*lock queue here!*/

							//fprintf(stderr,"%s: app %i got %i from other server this time\n",log_prefix, receive_buffer[i][j].app_index,receive_buffer[i][j].period_count);
							//receive buffer is per server; however the values are saved per app


							//apply delay function in each poll call
							//re-sort

							/*end locking*/
							pthread_mutex_lock(&vdsfq2_queue_mutex);

							//vdsfq_stats[vreceive_buffer[i][j].app_index].vdsfq_delay_values+=vreceive_buffer[i][j].period_count;
							//above is the simple DSFQ algorithm
							vapp_entries_remote2[vreceive_buffer2[i][j].app_index]+=vreceive_buffer2[i][j].period_count;
							fprintf(stderr,"%s: app %i got %i from other server in total\n",log_prefix, vreceive_buffer2[i][j].app_index,vapp_entries_remote2[vreceive_buffer2[i][j].app_index]);

									//vdsfq_stats[vreceive_buffer[i][j].app_index].vdsfq_delay_values);
							pthread_mutex_unlock(&vdsfq2_queue_mutex);
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
int vdsfq2_update_on_request_completion(void* arg)
{
	struct complete_message * complete = (struct complete_message *)arg;
	struct generic_queue_item * current_item = (complete->current_item);
	struct vdsfq2_queue_item* vdsfq2_item = (struct vdsfq2_queue_item*)(current_item->embedded_queue_item);

	int app_index = vdsfq2_item->app_index;
	//pthread_mutex_lock(&dsfq_broadcast_mutex);
	//value taken by broadcast thread and reset to 0

	//pthread_mutex_unlock(&dsfq_broadcast_mutex);


	//fprintf(stderr,"app %i's counter got added by %i to %i; total counter is now %i\n",
	//dsfq_item->app_index, complete->complete_size, request_receive_queue[dsfq_item->app_index], dsfq_current_io_counter);

	vdsfq2_item->got_size+=complete->complete_size;



	if (vdsfq2_item->got_size==vdsfq2_item->task_size)
	{
		return vdsfq2_item->got_size;
	}

	if (vdsfq2_item->task_size<vdsfq2_item->got_size)
	{
		return -1;
	}

	return 0;

	//also update the queue item's got size field
	//check completion, hen call getn next prototype returns int?

}

int vdsfq2_parse_configs()
{

}

int vdsfq2_fill_ip(void * item)
{
	char * char_item = (char*)item;
	fprintf(stderr,"filling %s to %ith slot\n",char_item, vfill_ip_count2);


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
				vother_server_ips2[vfill_ip_count2]=in_addr;
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
			vother_server_address2[vfill_ip_count2]=stored_ipstr;
			fprintf(stderr, " %s: %s\n", ipver, ipstr);

		}
		freeaddrinfo(servinfo);
	}


	vfill_ip_count2++;

	//fprintf(stderr,"resolving %s\n",inet_ntoa(*((struct in_addr*)(other_server_ips[fill_ip_count]->h_addr))));

	return vfill_ip_count2;
}

int vdsfq2_parse_server_config()
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

	vother_server_names2 = PINT_llist_new();
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
				PINT_llist_add_to_tail(vother_server_names2, tmp);
				vother_server_count2++;
			}
		}
		begin = strtok(NULL, "\n");
	}

	vother_server_ips2 = (struct sockaddr_in  **)malloc(sizeof(struct sockaddr_in  *)*vother_server_count2);
	vother_server_address2 = (char**)malloc(sizeof(char*)*vother_server_count2);
	PINT_llist_doall(vother_server_names2, vdsfq2_fill_ip);

	fclose(server_config);
	return 0;

	//read server names
	//use resolve to resolve ip address


}

int vdsfq2_is_idle()
{

	return PINT_llist_empty( vdsfq2_llist_queue);
}

int vdsfq2_add_ttl_tp(int this_amount, int app_index)
{
	total_throughput+=this_amount*(vapp_entries_remote2[app_index]+1);
    app_stats[app_index].byte_counter=app_stats[app_index].byte_counter+this_amount*(vapp_entries_remote2[app_index]+1);
    app_stats[app_index].app_throughput=app_stats[app_index].app_throughput+this_amount*(vapp_entries_remote2[app_index]+1);

}

int vdsfq2_calculate_diff()
{
	//no need to implement, because work_conserving = 1
}

const struct scheduler_method sch_vdsfq2 = {
    .method_name = VDSFQ2_SCHEDULER,
    .work_conserving = 0,
    .sch_initialize = vdsfq2_init,
    .sch_finalize = NULL,
    .sch_enqueue = vdsfq2_enqueue,
    .sch_dequeue = vdsfq2_dequeue,
    .sch_load_data_from_config = vdsfq2_load_data_from_config,
    .sch_update_on_request_completion = vdsfq2_update_on_request_completion,
    .sch_get_scheduler_info = vdsfq2_get_scheduler_info,
    .sch_is_idle = vdsfq2_is_idle,
    .sch_add_ttl_throughput = vdsfq2_add_ttl_tp,
    .sch_self_dispatch = 0
};

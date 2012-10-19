/*
 * (C) 2009-2012 Florida International University
 *
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * See COPYING in top-level directory.
 *
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
#include <unistd.h>
#include <signal.h>
#include <limits.h>


#include "proxy2.h"
#include "logging.h"
#include "dump.h"
#include "scheduler_SFQD.h"
#include "scheduler_DSFQ.h"
#include "performance.h"
#include "message.h"
#include "config.h"
extern char* log_prefix;
char *directions[2]={"SERVER","CLIENT"};
int enable_first_receive=0;
struct socket_pool  s_pool;

extern int logging;
int timer_stop=0;
int poll_delay=POLL_DELAY;
int max_tag=-1;
int first_receive=0;
int scheduler_on=1;
int counter_start=0;
void dump_stat(int sig);
char* dump_file="stat.txt";
#define PVFS_CONFIG_LOCATION "/home/users/yiqi/pvfs2-fs.conf"
char* pvfs_config = PVFS_CONFIG_LOCATION;
extern int* completed_requests;
extern char* log_prefix;
extern long long total_throughput;
extern int total_weight;
//struct queue_item * current_item=NULL;
int item_removal=0;
int finished[10];
int completerecv[10];
int completefwd[10];

int setup_socket()
{
    int listen_socket = BMI_sockio_new_sock();//setup listening socket

    int ret = BMI_sockio_set_sockopt( listen_socket, SO_REUSEADDR, 1 );//reuse immediately
    if ( ret == -1 )
    {
        fprintf( stderr, "Error setting SO_REUSEADDR: %i\n", ret );
    }
    ret = BMI_sockio_bind_sock( listen_socket, active_port );//bind to the specific port
    if ( ret == -1 )
    {
        fprintf( stderr,"Error binding socket: %i\n", listen_socket );
        return 1;
    }

    int oldfl = fcntl( listen_socket, F_GETFL, 0 );
    if ( !( oldfl & O_NONBLOCK ) )
    {
    	fcntl( listen_socket, F_SETFL, oldfl | O_NONBLOCK );
    	fprintf( stderr, "setting accept socket to nonblock\n" );
    }

    int optval = 1;
    if ( setsockopt( listen_socket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval) ) == -1 )
    {

    	fprintf( stderr, "error setsockopt\n" );
    	exit( -1 );
    }

    if ( listen( listen_socket, BACKLOG ) == -1 ) {
            fprintf( stderr, "Error listening on socket %i\n", listen_socket );
            return 1;
    }
    fprintf( stderr, "Waiting on message...%i\n", listen_socket );

    //Dprintf(D_GENERAL,"Waiting on message...%i\n",listen_socket);


    add_socket_to_pool(listen_socket, 0, 1,
    		SERVER, CLIENT,
    		"PROXY ENTRY", "NULL",
    		3334, 0);
    //adding only one listening socket using ACCEPT with its counter part port set to 0,
    //which means it has no counterpart (server-side) socket number
    return listen_socket;

}

/* In the socket-concurrent mode, write is listened if any thing is on the unsent buffer
 * read is always on, since we put the lock on each request_state and allow multiple requests
 * to be received and outstanding on the same socket (differentiated by tag)
 * */
void prepare_events()
{
    int i;
    for (i=0;i<s_pool.pool_size;i++)//iterate through the whole socket pool to set specific listening s
    {
    	if (s_pool.socket_state_list[i].accept_mode!=1)//leave accept socket alone
    	{

			s_pool.poll_list[i].events=POLLIN;//remember, we will always listen for reads
			//especially true for concurrency-based polling, where new messages are always welcome to arrive


			//find if any unsent buffer exists on this socket
			int counter_index=s_pool.socket_state_list[i].counter_socket_index;//;find_counter_part(s_pool.poll_list[i].fd);

			if (s_pool.socket_state_list[i].current_send_item!= NULL)
			{
				fprintf(stderr,"send item exists with head %i tail %i size %i\n",
						s_pool.socket_state_list[i].current_send_item->buffer_head,
						s_pool.socket_state_list[i].current_send_item->buffer_tail,
						s_pool.socket_state_list[i].current_send_item->buffer_size);
				/*assert(s_pool.socket_state_list[i].current_send_item->buffer_head!=
						s_pool.socket_state_list[i].current_send_item->buffer_tail);*/
				//the above assertion is not necessary, because we're writing and the buffer has not gained new contents
				if (s_pool.socket_state_list[i].current_send_item->buffer_head
						== s_pool.socket_state_list[i].current_send_item->buffer_tail)
				{
					s_pool.poll_list[i].events &= (~POLLOUT);
				}
				else
				{
					s_pool.poll_list[i].events |= POLLOUT;
				}
				//keep sending current item until it finishes
			}
			else
			{
				//fprintf(stderr,"searching on socket %i\n", i);
				struct request_state * found_unsent_request = PINT_llist_search(
					s_pool.socket_state_list[counter_index].req_state_data,
					(void *)NULL,
					list_req_state_buffer_nonempty_nonlock_comp);

				if (found_unsent_request==NULL)
				{
					//fprintf(stderr,"Nothing is found to send\n");
					s_pool.poll_list[i].events &= (~POLLOUT);//cancel out pool out event, but keep original read
				}
				else
				{
					fprintf(stderr,"manually found size %i op %i\n",found_unsent_request->buffer_size, found_unsent_request->op);
					update_socket_current_send(i);
					assert(s_pool.socket_state_list[i].current_send_item != NULL);

					s_pool.poll_list[i].events |= POLLOUT;
				}
			}
    	}
    }
}

int accept_socket(int listen_socket)
{
    struct sockaddr_storage their_addr;
    //struct sockaddr_in their_addr;
    socklen_t tsize;
    tsize = sizeof(their_addr);

	int client_socket = accept(listen_socket, (struct sockaddr *)&their_addr, &tsize);
	fprintf(stderr,"%s accepting... from socket %i\n", log_prefix, client_socket);
	if (client_socket <0) {
			fprintf(stderr,"%s Error accepting %i\n",log_prefix, client_socket);
			exit (-3);
	}

    int oldfl = fcntl(client_socket, F_GETFL, 0);
    if (!(oldfl & O_NONBLOCK))
    {
        //fprintf(stderr,"setting client socket to nonblock...");
        fcntl(client_socket, F_SETFL, oldfl | O_NONBLOCK);

        //fprintf(stderr,"setting done %i\n",total_connected);
    }
    int optval=1;
    if (setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval))==-1)
    {

        fprintf(stderr,"error setsockopt on client socket\n");
        exit(-1);
    }

    getpeername(client_socket, (__SOCKADDR_ARG) &their_addr, &tsize);
    unsigned int addr=((struct sockaddr_in *)&their_addr)->sin_addr.s_addr;

    char * client_ip = (char*)malloc(25);
    int l = sizeof (their_addr);
    int client_port=ntohs(((struct sockaddr_in *)&their_addr)->sin_port);
    int ssize = sprintf(client_ip,"%u.%u.%u.%u",(addr<<24)>>24,	(addr<<16)>>24,(addr<<8)>>24, addr>>24);

    fprintf(stderr,"%s client ip got %s %i\n", log_prefix, client_ip, client_socket);
    int server_socket = BMI_sockio_new_sock();
    //Dprintf(L_NOTICE,"Connecting...%i\n",server_socket);
    char * server_ip = (char*)malloc(25);
    int server_port;
    int ret;
    if ((ret=BMI_sockio_connect_sock(server_socket,"localhost",3334,server_ip, &server_port))!=server_socket)//we establish a pairing connecting to our local server
    {
		fprintf(stderr,"Error connecting %i\n",ret);
		close(client_socket);
		close(server_socket);
		exit (-1);
    }
    //Dprintf(L_NOTICE,"Connection successful\n");
    int app_index=add_socket_to_pool(client_socket,server_socket,0,
    		SERVER,CLIENT,client_ip, server_ip, client_port, server_port);
    //now that we have a pair of socket with client and server, we pair them together and add to the pool
    //create thread
    //fprintf(stderr,"socket added\n");
    if (enable_first_receive && !first_receive)
    {
		first_receive=1;
		gettimeofday(&first_count_time,0);

    }
    return app_index;
}



void clean_up_request_state(int i, int counter_index, long tag, char* type)
{
	struct request_state * removed_rs;

	int server_c = 0;
	int client_c = 0;
	item_removal++;

	fprintf(stderr, "cleaning a %s of tag %i on socket %i/%i: ", type, tag, i, counter_index);

	removed_rs = (struct request_state*)PINT_llist_rem(
			s_pool.socket_state_list[i].req_state_data,
			(void*)tag,
			list_req_state_comp_curr);

	while (removed_rs!= NULL)
	{
		fprintf(stderr,"%s(%i) ",ops[removed_rs->op], removed_rs->op);
		if (removed_rs->buffer!=NULL)
			free(removed_rs->buffer);
		removed_rs->buffer = NULL;
		removed_rs->buffer_head = -2;
		removed_rs->buffer_tail = -2;
		removed_rs->buffer_size = -2;

		free(removed_rs);
		removed_rs =  (struct request_state*)PINT_llist_rem(
				s_pool.socket_state_list[i].req_state_data,
				(void*)tag,
				list_req_state_comp_curr);
		client_c++;
	}
	fprintf(stderr, "%i client messages freed, ", client_c);

	removed_rs =  (struct request_state*)PINT_llist_rem(
			s_pool.socket_state_list[counter_index].req_state_data,
			(void*)tag,
			list_req_state_comp_curr);
	while (removed_rs!= NULL)
	{
		if (removed_rs->buffer!=NULL)
			free(removed_rs->buffer);
		removed_rs->buffer = NULL;
		removed_rs->buffer_head = -2;
		removed_rs->buffer_tail = -2;
		removed_rs->buffer_size = -2;

		free(removed_rs);
		removed_rs =  (struct request_state*)PINT_llist_rem(
				s_pool.socket_state_list[counter_index].req_state_data,
				(void*)tag,
				list_req_state_comp_curr);
		server_c++;
	}
	fprintf(stderr, "%i server messages freed, %i items removed so far\n", server_c, item_removal);}

int main(int argc, char **argv)
{

    fprintf(stderr,"size for void * is %i\n", sizeof(void*));
    fprintf(stderr,"size for int is %i\n", sizeof(int));
    fprintf(stderr,"size for long int is %i\n", sizeof(long int));
    //fprintf(stderr, "starting\n");
    //fprintf(stderr,"%llu, %lli, %X\n", 0x80A31F00,0x80A31F00,18446744071572758272);


    struct proxy_option* option_p = (struct proxy_option*)malloc(sizeof(struct proxy_option));
    memset(option_p,0,sizeof(struct proxy_option));

    parse_options ( argc, argv, option_p);

    load_configuration();
    initialize_hashtable();
    cost_model_history_init ();
    s_pool.pool_size=0;
    create_socket_pool(10000);//creates a dynamically expandable and shrinkable pool, with initial size of 100000.

    if (scheduler_on)
    {
    	scheduler_main_init();
        (*(static_methods[scheduler_index]->sch_initialize))();
    }

    install_signal_handler(SIGTERM, dump_stat);
    install_signal_handler(SIGINT, dump_stat);

    if (counter_start)
    {
        start_counter();
    }

    int listen_socket = setup_socket();

    int counter_socket, counter_index;

    while (1)
    {
        //fprintf(stderr,"polling\n");
    	prepare_events();

        //fprintf(stderr,"before polling...size %i ",s_pool.pool_size);
        //print_socket_pool();
        int num_poll=poll(s_pool.poll_list,s_pool.pool_size,poll_delay);

        //fprintf(stderr,"eeeeeeeeeeeeeeeeend polling, got %i eeeeeeeeeeeeeeeee\n", num_poll);
        int process=0;//total processed socket for each poll

        if (num_poll>0)
        {
        	int i;
            fprintf(stderr,"%i detected\n", num_poll);
            for (i=0;i<s_pool.pool_size;i++)//iterate through the whole socket pool for returned events
            {
                if (process==num_poll)
                {
                    //fprintf(stderr,"%s: %i processed\n",log_prefix,process);
                    break;
                }
                //Dprintf(D_CALL, "starting to process %i/%i.\n",i,num_poll);
                int this_process=0;//one socket might have multiple events, flag to make sure our counter counts one socket only once at most.

                if (s_pool.poll_list[i].revents & POLLIN)
                {
                    //fprintf(stderr, "processing %ith socket %i for read\n",i,s_pool.poll_list[i].fd);
                    if (!this_process)
                    {
                            process++;//read events
                            this_process=1;
                    }
                    if (s_pool.poll_list[i].fd==listen_socket)//if this is a listening socket event, then a new connection is coming in
                    {
                    	int app_index = accept_socket(listen_socket);
                        //fprintf(stderr,"end accepd\n");

                    }//end accept
                    else//this is a real read event of data from a socket
                    {
                        int read_socket=s_pool.poll_list[i].fd;
                        int counter_index=s_pool.socket_state_list[i].counter_socket_index;//find_counter_part(read_socket);
                        int check_stat = 0;
                        if ( s_pool.socket_state_list[i].current_receive_item == NULL )
                        	//new message from the socket
                        {

                        	if ( s_pool.socket_state_list[i].source == SERVER )
							{
                        		fprintf(stderr,"checking response on socket %i\n", i);
                        		check_stat=check_response(i);
							}
                        	else
                        	{
                        		fprintf(stderr,"checking request on socket %i\n", i);
                        		check_stat=check_request(i);
                        	}
							if (check_stat<0)
							{
								if (check_stat==-1)
								{
									fprintf(stderr,"response returning 0, closing?\n");

								}
								else if (check_stat ==-2)
								{
									fprintf(stderr,"resp: didn't even get the bmi header, continuing\n");
									continue;
								}
								else if (check_stat == -3)
								{
									fprintf(stderr,"did not receive enough data for on a message %i\n", check_stat);
									continue;
									//exit(-1);
								}
								else
								{
									fprintf(stderr,"unrecognized return check %i\n", check_stat);
									exit(-1);
								}
							}

							if (check_stat!=-1)
							{
								assert(s_pool.socket_state_list[i].current_receive_item != NULL);
							}

							//request are just peeked, not received off the wire yhet.

                        }
                        else // current_receive_item != NULL //partial data of each individual message(req, resp, flow, etc)
                        {
							/*Dprintf(D_CACHE, "Partial data encountered:%i/%i, %s:%i, tag %i\n",
								s_pool.socket_state_list[i].completed_size,
								s_pool.socket_state_list[i].job_size,
								s_pool.socket_state_list[i].ip,
								s_pool.socket_state_list[i].port,
								s_pool.socket_state_list[i].current_tag);*/
                        }
                        struct request_state *current_receive_item = s_pool.socket_state_list[i].current_receive_item;
                        char* buffer;
                        char tmp_buffer[1];
                        int recv_size,got_size;//bytes we wish to receive and bytes we actually receive
                        int current_size, buffer_size, remaining_size;

                        if (check_stat!=-1)
                        {
                        	buffer = current_receive_item->buffer + current_receive_item->buffer_tail;
    						current_size= current_receive_item->buffer_tail - current_receive_item->buffer_head;
    						buffer_size = current_receive_item->buffer_size;
    						remaining_size = buffer_size - current_receive_item->buffer_tail;//space left for more data
                        }
                        else
                        {
                        	remaining_size = 1;//used for closing sockets
                        	buffer = tmp_buffer;
                        }

						if ( remaining_size > 0 )
						{
							//if (s_pool.socket_state_list[i].config_tag==1)
									//p_log(log_file,"config_TAG of %ith socket %i: %i\n",i,read_socket,s_pool.socket_state_list[i].config_tag);
							recv_size = remaining_size;//out_buffer_size);

							int err_count = 0;
							//fprintf(stderr, "trying to receive %i bytes\n", recv_size);
							recv_restart:
							errno = 0;
							got_size = recv( read_socket, buffer, recv_size, MSG_DONTWAIT );

							//fprintf(stderr,"#Got size:%i\n",got_size);
							if (got_size == -1 && errno!=EINTR && errno!=EAGAIN || got_size==0)
							{
								if (got_size==0)
								{
									fprintf(stderr,"socket closing on %i\n",i);
								}
								else
								{
									fprintf(stderr, "%s unknown error on %i\n",log_prefix, i);
									perror("Error ");
								}

								close(read_socket);
								fprintf(stderr,"removing %i\n",i);
								remove_socket_by_index(i);

								close(s_pool.socket_state_list[i+1].socket);
								fprintf(stderr,"removing %i\n",i+1);
								remove_socket_by_index(i+1);

								if (s_pool.pool_size>1)//not the last two deleted
								{
									s_pool.socket_state_list[i]=s_pool.socket_state_list[s_pool.pool_size];
									s_pool.poll_list[i]=s_pool.poll_list[s_pool.pool_size];
									s_pool.socket_state_list[i].counter_socket_index = i+1;
									s_pool.socket_state_list[i+1]=s_pool.socket_state_list[s_pool.pool_size+1];
									s_pool.poll_list[i+1]=s_pool.poll_list[s_pool.pool_size+1];
									s_pool.socket_state_list[i+1].counter_socket_index = i;
								}

								i--;
								fprintf(stderr,"sockets removed:\n");
								continue;


							}
							else if (got_size == -1 && (errno==EINTR ||errno==EAGAIN))
							{
								err_count++;
								if (err_count>=5)
								{
										fprintf(stderr, "interrupted on %i\n", read_socket);
										exit(-1);
										//next time....
										//Dprintf(D_CACHE, "interrupted on %i\n", read_socket);
								}
								else
								{
										fprintf(stderr, "Retrying for %i th time\n", err_count);
										goto recv_restart;
								}
							}
							else
							{
								current_receive_item->buffer_tail+=got_size;
								current_receive_item->completed_size+=got_size;
								fprintf(stderr, "received data size %i on socket %i\n", got_size, i);
							}
						}
						else
						{
							fprintf(stderr,"Buffer is already full, no job to be done on this socket!%i\n",i);
							fprintf(stderr,"size:%i head:%i tail:%i\n", buffer_size,
									current_receive_item->buffer_head,
									current_receive_item->buffer_tail);
							exit(-1);
						}
						//whenever a single message is completed, check whether the whole request/task is completed
						assert(current_receive_item->job_size >0);
						if (current_receive_item->completed_size == current_receive_item->job_size &&
							current_receive_item->job_size > 0)
						{
							//after each receive, check if it's a total complete of message/the response of get_config

							if (current_receive_item->config_tag==1)
							{
								current_receive_item->config_tag=0;//the sending lock can be released now
								int got_size=current_receive_item->job_size;
								//Dprintf(L_NOTICE,"Changing port\n");
								change_port(buffer+52, got_size-52,"3334","3335");
							}


							if (scheduler_on &&
								s_pool.socket_state_list[i].source == CLIENT &&
								current_receive_item->op == PVFS_DATA_FLOW
							) // a PVFS_WRITE
							{
								if (current_receive_item->original_request == NULL)
								{
									fprintf(stderr,"original request was not found in the write flow (client-side) message\n");
									exit(-1);
								}

								struct generic_queue_item* current_item= current_receive_item->original_request->current_item;

								//call completion hook with current item and message if dsfq
								struct complete_message cmsg;
								cmsg.complete_size=current_receive_item->completed_size-BMI_HEADER_LENGTH;
								cmsg.current_item=current_item;
								//*(static_methods[scheduler_index].sch_fill_proxy_message)(cmsg.proxy_message);
								int ret = (*(static_methods[scheduler_index]->sch_update_on_request_completion))((void*)&cmsg);
								/* write and read should all receive this completion call
								 * in 2l ssfqd, it just doesn't return a positive ever....
								 * because dispatch is not based on this...
								 * */
							}
							if (scheduler_on &&
								s_pool.socket_state_list[i].source == SERVER &&
								current_receive_item->op == PVFS_DATA_FLOW
							) // a PVFS_READ
							{

								if (current_receive_item->original_request == NULL)
								{
									fprintf(stderr,"original request was not found in the read flow (server-side) message\n");
									exit(-1);
								}

								struct generic_queue_item* current_item= current_receive_item->original_request->current_item;
								//queue items are always stored at the request (client) side socket.
								struct complete_message cmsg;
								cmsg.complete_size=current_receive_item->completed_size-BMI_HEADER_LENGTH;
								cmsg.current_item=current_item;
								int ret = (*(static_methods[scheduler_index]->sch_update_on_request_completion))((void*)&cmsg);
								//return value is the whole I/O request size
								fprintf(stderr,"flow returning %i\n",ret);
								if (ret>0 && static_methods[scheduler_index]->sch_self_dispatch==0)
								{
									s_pool.socket_state_list[i].current_receive_item->last_flow = 1;
									/*cost*/
									//fprintf(stderr,"read returned %i\n",ret);
									struct timeval tv;
									gettimeofday(&tv, 0);
									int app_index=s_pool.socket_state_list[counter_index].app_index;
									int read_resp_time = get_response_time(counter_index, tv, PVFS_IO_WRITE);
									update_history(app_index, counter_index, 0, ret, read_resp_time, PVFS_IO_READ);

									//update to app history
									/*cost*/

									current_receive_item->original_request->check_response_completion=1;
									current_receive_item->original_request->last_completion=ret;
									//fprintf(stderr,"[READ COMPLETE]\n");

									app_stats[app_index].completed_requests+=1;
									//first_receive=1;
									if (first_receive==0)
									{
										passed_completions++;
										app_stats[app_index].app_exist=app_stats[app_index].app_exist+1;

										fprintf(stderr,"client %s completed++ for app %i\n",
												s_pool.socket_state_list[counter_index].ip, app_index+1);
										check_all_app_stat();
										//fprintf(stderr,"%i completion passed, not starting counter yet\n", passed_completions);
									}

									if (timer_stop || !first_receive)
									{
											//fprintf(stderr,"Timer stopped or not started\n");
									}
									else
									{
										if (current_receive_item->original_request == NULL)
										{
											fprintf(stderr,"original request was not found in the write flow (client-side) message\n");
											exit(-1);
										}

										struct request_state * current_state= current_receive_item->original_request;

										(*(static_methods[scheduler_index]->sch_add_ttl_throughput))
												(current_state->last_completion, app_index);

										if (!static_methods[scheduler_index]->work_conserving)
										{
											int s;
											//here we try to fix the diff change based dispatch first
											for (s=0;s<num_apps;s++)
											{
												int old_diff=app_stats[s].diff;
												app_stats[s].diff=(*(static_methods[scheduler_index]->sch_calculate_diff))(s);
												if (old_diff>10240 && app_stats[s].diff<=10240)
												{
													struct dequeue_reason r;
													r.complete_size=0;
													r.event=DIFF_CHANGE;
													r.last_app_index=s;
													r.item=current_receive_item->current_item;
													struct generic_queue_item* new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
													while (new_item!=NULL)
													{
														struct request_state *new_dispatched = new_item->socket_data->rs;
														new_dispatched->current_item = new_item;
														new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
														new_dispatched->locked = 0;
													}
												}
											}
										}
									}
									struct dequeue_reason r;
									r.complete_size=ret;
									r.event=COMPLETE_IO;
									r.last_app_index=app_index;
									struct generic_queue_item* new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
									(*(static_methods[scheduler_index]->sch_get_scheduler_info))();
									if (new_item==NULL)
									{//fprintf(stderr, "no more jobs found, depth decreased...\n");
									}
									else
									{
										struct timeval tv;
										gettimeofday(&tv, 0);
										update_release_time(new_item->socket_data->unlock_index, tv);
										struct request_state *new_dispatched = new_item->socket_data->rs;
										assert(new_dispatched->locked!=0);
										new_dispatched->current_item = new_item;
										new_dispatched->locked = 0;
									}
								}
								else if (ret==-1)
								{
									fprintf(stderr, "error...got_size > task_size");
									exit(-1);
								}

							}//end write and read
							//fprintf(stderr, "Job Finished on %s:%i\n",s_pool.socket_state_list[i].ip,s_pool.socket_state_list[i].port);
							s_pool.socket_state_list[i].current_receive_item = NULL;

						}
						if ( current_receive_item->completed_size > current_receive_item->job_size )
						{
								fprintf(stderr, "We got more than we expected!\n");
								exit(-1);
						}
						update_socket_current_send(counter_index);
						/*assert(s_pool.socket_state_list[counter_index].current_send_item != NULL );*/
						//I could be blocked!

						//if anything is received on a socket, then the other side should not be starving.
                    }//end receive
                }//end pullin events

                if (s_pool.poll_list[i].revents & POLLOUT) //the port is ready to write without blocking
                {
                    //Dprintf(D_CALL,"Processing %ith socket %i for write\n",i,s_pool.socket_state_list[i].socket);
                    if (!this_process)
                    {
                        process++;//read events
                        this_process=1;
                    }

                    counter_index=s_pool.socket_state_list[i].counter_socket_index;
                    struct request_state * write_rs = s_pool.socket_state_list[i].current_send_item;
                    if (write_rs == NULL)
                    {
                    	fprintf(stderr, "write end has no corresponding read data!\n");
                    	exit(-1);
                		write_rs = (struct request_state *)
                				PINT_llist_head(s_pool.socket_state_list[counter_index].req_state_data);
                		s_pool.socket_state_list[i].current_send_item = write_rs;
                    }

                    if (write_rs ==NULL)
                    {
                    	fprintf(stderr, "write end has no corresponding read data after tyring to get one!\n");
                    	exit(-1);
                    }

                    int ready_size = write_rs->buffer_tail - write_rs->buffer_head;//bytes we want to send

                    if ( ready_size > 0 && ( write_rs->config_tag !=1 )
                    )//this is to avoid sending partial get_config buffer without modifying it.
                    {
                        char * ready_buffer = write_rs->buffer + write_rs->buffer_head;//reader's buffer

                        int sent_size=send(s_pool.poll_list[i].fd, ready_buffer, ready_size,0);
                        if (sent_size==0 || sent_size==-1)
                        {
                                fprintf(stderr, "Sent error on %i; returning %i\n",s_pool.poll_list[i].fd, sent_size);
                                exit(-1);

                        }
                        else
                        {
                        	fprintf(stderr,"sent %i bytes on socket %i, head %i, tail %i, size %i\n",
                        			sent_size, i, write_rs->buffer_head, write_rs->buffer_tail, write_rs->buffer_size);
                            //fprintf(stderr,"#Sent %i bytes to %i!\n",sent_size, s_pool.poll_list[i].fd);
                            write_rs->buffer_head += sent_size;//decrease the buffer size by the amount we have successfully sent
                            //s_pool.socket_state_list[i].finished+=sent_size;
                            if ( ready_size > sent_size )
                            {//if we only sent partial data, then we need the left data to be in the beginning part of the buffer
                                   // memmove(ready_buffer, ready_buffer+sent_size, ready_size-sent_size);//move the left data to the head
                                //fprintf(stderr,"partial send on %i...saved a chance for memmove!\n",i);
                            }
                            else if (ready_size < sent_size)
                            {
                            	fprintf(stderr,"sent size > buffer size ?!\n");
                            	exit(-1);
                            }
                            //then all exisitng data in the buffer is sent
                            else if (write_rs->buffer_size == write_rs->buffer_head)
							{
								//completion of message protocol declared size!
								//fprintf(stderr,"forwarding complete, cleaning buffer on socket %i\n",s_pool.socket_state_list[counter_index].socket);
								if (write_rs->op == PVFS_SERV_WRITE_COMPLETION ||
										write_rs->last_flow == 1 ||
										write_rs->meta_response == 1)
								{
									int tag = write_rs->current_tag;
									int app_index = s_pool.socket_state_list[i].app_index;
									completefwd[app_index]++;
									completerecv[app_index]--;
									//clean up both-end sockets using the same tag;
									if (write_rs->last_flow == 1 )
									{
										clean_up_request_state(i, counter_index, tag, "read");
									}
									else if (write_rs->meta_response == 1)
									{
										clean_up_request_state(i, counter_index, tag, "meta");
									}
									else if (write_rs->op == PVFS_SERV_WRITE_COMPLETION)
									{
										clean_up_request_state(i, counter_index, tag, "write");
									}


								}
								s_pool.socket_state_list[i].current_send_item = NULL;
								update_socket_current_send(i);//client side index is i, server side is counter_index

							}//job completed
                        }//send successful
                    }//unsent data
                    else if (write_rs->config_tag !=1)//outgoing socket is ready to be written, but the buffer is empty
                    {
                            //p_log(log_file, "%i: outgoing socket is ready, but buffer is empty from %i/%i (i:%i, counter:%i)\n",
                                    //	s_pool.poll_list[i].fd, s_pool.poll_list[counter_index].fd,s_pool.buffer_sizes[counter_index],i,counter_index);
                    	fprintf(stderr,"buffer has no content, I should not be polled from the beginning %i\n", write_rs->buffer_size);

                    	exit(-1);
                    }
                    //fprintf(stderr,"real2 event3\n");
                }//end pollout
            }//end iterating revents loop
        }//end if num_poll>0
    }//end polling
    return 0;
}


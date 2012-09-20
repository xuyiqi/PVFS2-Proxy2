/*
 * proxy2.c
 *
 *  Created on: Mar 30, 2010
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
char *directions[2]={"SERVER","CLIENT"};
int enable_first_receive=0;
struct socket_pool  s_pool;
//int out_buffer_size=OUT_BUFFER_SIZE;
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
    //dsfq_init();
    //exit(0);
    load_configuration();

    initialize_hashtable();

    cost_model_history_init ();
    //initialize_ip_sock();
    s_pool.pool_size=0;
    create_socket_pool(10000);//creates a dynamically expandable and shrinkable pool, with initial size of 1.
    //print_socket_pool();//for simple debugging output


    if (scheduler_on)
    {
        (*(static_methods[scheduler_index]->sch_initialize))();
    }

    install_signal_handler(SIGTERM, dump_stat);
    install_signal_handler(SIGINT, dump_stat);

    if (counter_start)
    {
        start_counter();
    }

    int listen_socket=BMI_sockio_new_sock();//setup listening socket

    int ret=BMI_sockio_set_sockopt(listen_socket,SO_REUSEADDR, 1);//reuse immediately
    if (ret==-1)
    {
        fprintf(stderr,"Error setting SO_REUSEADDR: %i\n",ret);
    }
    ret = BMI_sockio_bind_sock(listen_socket,active_port);//bind to the specific port
    if (ret==-1)
    {
        fprintf(stderr,"Error binding socket: %i\n",listen_socket);
        return 1;
    }

    int oldfl = fcntl(listen_socket, F_GETFL, 0);
    if (!(oldfl & O_NONBLOCK))
    {
    	fcntl(listen_socket, F_SETFL, oldfl | O_NONBLOCK);
    	fprintf(stderr,"setting accept socket to nonblock\n");
    }

    int optval=1;
    if (setsockopt(listen_socket, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval))==-1)
    {

    	fprintf(stderr,"error setsockopt\n");
    	exit(-1);
    }

    if (listen(listen_socket, BACKLOG) == -1) {
            fprintf(stderr, "Error listening on socket %i\n", listen_socket);
            return 1;
    }
    fprintf(stderr,"Waiting on message...%i\n",listen_socket);

    //Dprintf(D_GENERAL,"Waiting on message...%i\n",listen_socket);

    struct sockaddr_storage their_addr;
    //struct sockaddr_in their_addr;
    socklen_t tsize;
    tsize = sizeof(their_addr);
    add_socket(listen_socket, 0, ACCEPT, SERVER, CLIENT, "PROXY ENTRY", "NULL", 3334, 0);//adding only one listening socket using ACCEPT with its counter part port set to 0, which means it has no counterpart socket number

    int counter_socket, counter_index;
    int total_connected=0;
    while (1)
    {
        //fprintf(stderr,"polling\n");
        int i;
        for (i=0;i<s_pool.pool_size;i++)//iterate through the whole socket pool to set specific listening s
        {
        	if (s_pool.socket_state_list[i].mode!=ACCEPT)
        	{
				if ((s_pool.socket_state_list[i].buffer_size!=0
						&& s_pool.socket_state_list[i].buffer_tail == s_pool.socket_state_list[i].buffer_size)
						|| s_pool.socket_state_list[i].locked==1)
				{
					s_pool.poll_list[i].events=0;
					//fprintf(stderr,"!!!!!!!!!!!!!!!!!!! socket %i locked:%i, tail:%i, size:%i !!!!!!!!!!!!!\n",
						//	s_pool.poll_list[i].fd, s_pool.socket_state_list[i].locked,
						//	s_pool.socket_state_list[i].buffer_tail, s_pool.socket_state_list[i].buffer_size);
				}
				else
				{
					s_pool.poll_list[i].events=POLLIN;//remember, we will always listen for reads
					//fprintf(stderr,"ssssssssssssssss socket %i is open for read sssssssssssssssssssss\n", s_pool.poll_list[i].fd);
				}
        	}

            if (s_pool.socket_state_list[i].mode==WRITE)
            {
                int counter_index=s_pool.socket_state_list[i].counter_index;//;find_counter_part(s_pool.poll_list[i].fd);
                if (s_pool.socket_state_list[counter_index].buffer_tail -
                		s_pool.socket_state_list[counter_index].buffer_head==0)
                //if counter_index is locked, don't poll write either
                {

                    s_pool.poll_list[i].events &=(~POLLOUT);//cancel out pool out event, but keep original read
                }
                else
                {
                    s_pool.poll_list[i].events|=POLLOUT;
                }
            }

            if (s_pool.socket_state_list[i].mode!=ACCEPT && s_pool.socket_state_list[i].mode!=WRITE && s_pool.socket_state_list[i].mode!=READ)
            {
                fprintf(stderr,"error mode\n");
                s_pool.poll_list[i].events=0;
                exit(-1);
            }
        }
        //fprintf(stderr,"before polling...size %i ",s_pool.pool_size);
        //print_socket_pool();
        int num_poll=poll(s_pool.poll_list,s_pool.pool_size,poll_delay);//poll the whole pool using a wait time of 5 milliseconds at most.

        //fprintf(stderr,"eeeeeeeeeeeeeeeeend polling, got %i eeeeeeeeeeeeeeeee\n", num_poll);
        int process=0;//total processed socket for each poll

        if (num_poll>0)
        {
            //Dprintf(D_CALL,"%i detected\n", num_poll);
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
                            fprintf(stderr,"accepting...\n");
                            int client_socket = accept(listen_socket, (struct sockaddr *)&their_addr, &tsize);
                            if (client_socket <0) {
                                    fprintf(stderr,"Error accepting %i\n",client_socket);
                                    exit (-3);
                            }


                        int oldfl = fcntl(client_socket, F_GETFL, 0);
                        if (!(oldfl & O_NONBLOCK))
                        {
                            //fprintf(stderr,"setting client socket to nonblock...");
                            fcntl(client_socket, F_SETFL, oldfl | O_NONBLOCK);
                            total_connected++;
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
                        //fprintf(stderr, "Socket from IP: %ui.%ui.%ui.%ui:%i\n", addr>>24, (addr<<8)>>24,(addr<<16)>>24, (addr<<24)>>24,
                        //		((struct sockaddr_in *)&their_addr)->sin_port);

                        //Dprintf(D_CACHE,"Socket from IP: %u.%u.%u.%u:%u\n",
                        //		(addr<<24)>>24,	(addr<<16)>>24,(addr<<8)>>24, addr>>24,
                        //ntohs(((struct sockaddr_in *)&their_addr)->sin_port));
                        char * client_ip = (char*)malloc(25);
                        int l = sizeof (their_addr);
                        int client_port=ntohs(((struct sockaddr_in *)&their_addr)->sin_port);
                        int ssize = sprintf(client_ip,"%u.%u.%u.%u",(addr<<24)>>24,	(addr<<16)>>24,(addr<<8)>>24, addr>>24);

                        //fprintf(stderr,"client ip got\n");
                        int server_socket = BMI_sockio_new_sock();
                        //Dprintf(L_NOTICE,"Connecting...%i\n",server_socket);
                        char * server_ip = (char*)malloc(25);
                        int server_port;
                        if ((ret=BMI_sockio_connect_sock(server_socket,"localhost",3334,server_ip, &server_port))!=server_socket)//we establish a pairing connecting to our local server
                        {
                                fprintf(stderr,"Error connecting %i\n",ret);
                                close(client_socket);
                                close(server_socket);
                                exit (-1);
                        }
                        //Dprintf(L_NOTICE,"Connection successful\n");
                        int app_index=add_socket(client_socket,server_socket,READ,SERVER,CLIENT,client_ip, server_ip, client_port, server_port);//now that we have a pair of socket with client and server, we pair them together and add to the pool
                        //create thread
                        //fprintf(stderr,"socket added\n");
                        if (enable_first_receive && !first_receive)
                        {
                                first_receive=1;
                                gettimeofday(&first_count_time,0);

                        }
                        //fprintf(stderr,"end accepd\n");

                    }//end accept
                    else//this is a real read event of data from a socket
                    {
                        //fprintf(stderr,"real event1\n");


                        int read_socket=s_pool.poll_list[i].fd;
                        int counter_index=s_pool.socket_state_list[i].counter_index;//find_counter_part(read_socket);
                        //Dprintf(L_NOTICE,"Receiving from %i\n", read_socket);
                        s_pool.socket_state_list[counter_index].mode=WRITE;
                        //Dprintf(L_NOTICE,"socket %i at index %i has been set to write\n",s_pool.socket_state_list[counter_index].socket, counter_index);

                        //the readiness of data is the decision signal of reversing flow of data for both ports.
                        //we'll set the counter part socket port's mode to write

                        if (s_pool.socket_state_list[i].mode==WRITE)//if previously the port is being written
                        {
							s_pool.socket_state_list[i].mode=READ;

							s_pool.socket_state_list[counter_index].buffer_head=0;
							s_pool.socket_state_list[counter_index].buffer_size=0;
							s_pool.socket_state_list[counter_index].buffer_tail=0;
							if (s_pool.socket_state_list[counter_index].buffer!=NULL)
							{
								free(s_pool.socket_state_list[counter_index].buffer);
								s_pool.socket_state_list[counter_index].buffer=NULL;

							}
							/** A lot of these cleaning up are assumed by one-way talk protocol **/

							s_pool.socket_state_list[i].completed_size=0;
							s_pool.socket_state_list[i].job_size=0;
							s_pool.socket_state_list[i].source=!s_pool.socket_state_list[i].source;
							s_pool.socket_state_list[i].target=!s_pool.socket_state_list[i].target;//also reverse the source and target
							s_pool.socket_state_list[counter_index].source=!s_pool.socket_state_list[counter_index].source;
							s_pool.socket_state_list[counter_index].target=!s_pool.socket_state_list[counter_index].target;//reverse the source and target of counter part
							s_pool.socket_state_list[counter_index].mode=WRITE;
                                
                        }

                        //if the source is server, we need to check if it's a response of get_config
                        //Dprintf(L_NOTICE,"Source: %s, completed_size:%i\n", directions[s_pool.socket_state_list[i].source], s_pool.socket_state_list[i].completed_size);
                        if (s_pool.socket_state_list[i].source==SERVER && s_pool.socket_state_list[i].completed_size==0
                                        //&&s_pool.socket_state_list[i].job_size==0
                        )
                        {
                            //fprintf(stderr,"checking config\n");
                        	int resp_stat=check_response(i);
                        	if (resp_stat<0)
                        	{
								if (resp_stat==-1)
								{
									fprintf(stderr," response returning 0, closing?\n");

								}
								else if (resp_stat ==-2)
								{
									fprintf(stderr,"XXXXXXXXXXXXXXXXXXcontinuingXXXXXXXXXXXXXXXxx\n");
									continue;
								}
								else
								{
									fprintf(stderr,"critical error on resp\n");
									exit(-1);
								}
                        	}
                            //start of a response
                            //peek the whole header too
                            //if it's not an IO
                        }
                        else if (s_pool.socket_state_list[i].source==CLIENT &&
                                        s_pool.socket_state_list[i].completed_size==0
                        )
                        {
                        	int req_stat = check_request(i);
                        	if (req_stat<0)
                        	{
								if (req_stat==-1)
								{
									fprintf(stderr,"req returning 0, closing?\n");
								}
								else if (req_stat==-2)
								{
									fprintf(stderr,"XXXXXXXXXXXXXXXXXXcontinuingXXXXXXXXXXXXXXXxx\n");
									continue;
								}
								else
								{
									fprintf(stderr,"critical error on req\n");
									exit(-1);
								}
                        	}


                            //if it's dispatched, conitinue like nothing happened
                            //	current_item=get_next_request();
                            //	start counting...
                            //if not, block the socket(not the write direction)
                        }
                        else
                        {

                                /*Dprintf(D_CACHE, "Partial data encountered:%i/%i, %s:%i, tag %i\n",
                                                s_pool.socket_state_list[i].completed_size,
                                                s_pool.socket_state_list[i].job_size,
                                                s_pool.socket_state_list[i].ip,
                                                s_pool.socket_state_list[i].port,
                                                s_pool.socket_state_list[i].current_tag);*/
                        }
                        char* buffer = s_pool.socket_state_list[i].buffer + s_pool.socket_state_list[i].buffer_tail;
                        //fprintf(stderr,"buffer addr:%i\n", (int)buffer);
                        //fprintf(stderr,"real event2\n");
                        if (s_pool.socket_state_list[i].locked==0)
                        {
                            int recv_size,got_size;//bytes we wish to receive and bytes we actually receive
                            int current_size= s_pool.socket_state_list[i].buffer_tail-s_pool.socket_state_list[i].buffer_head;
                            int buffer_size = s_pool.socket_state_list[i].buffer_size;
                            int remaining_size = buffer_size-s_pool.socket_state_list[i].buffer_tail;//space left for more data
                            int job_remaining_size=s_pool.socket_state_list[i].job_size-s_pool.socket_state_list[i].completed_size;
                            if (remaining_size>=0)
                            {
                                //if (s_pool.socket_state_list[i].config_tag==1)
                                        //p_log(log_file,"config_TAG of %ith socket %i: %i\n",i,read_socket,s_pool.socket_state_list[i].config_tag);
                                if (s_pool.socket_state_list[i].ready_to_receive==1
                                                //&& s_pool.socket_state_list[i].config_tag==1 ||
                                                //s_pool.socket_state_list[i].config_tag==0
                                )  //we want to receive all the contents of a config file at a time
                                {
                                    recv_size=remaining_size;//out_buffer_size);

                                    int err_count=0;
                                    //fprintf(stderr, "trying to receive %i bytes\n", recv_size);
                                    recv_restart:
                                    errno=0;
                                    got_size=recv(read_socket, buffer, recv_size,MSG_DONTWAIT);

                                    //fprintf(stderr,"#Got size:%i\n",got_size);
                                    if (got_size == -1 && errno!=EINTR && errno!=EAGAIN || got_size==0)
                                    {
                                        if (got_size==0)
                                        {
                                                fprintf(stderr,"socket closing on %i\n",read_socket);
                                        }
                                        else
                                        {
                                                fprintf(stderr, "%s unknown error on %i\n",log_prefix, read_socket);
                                                perror("Error ");

                                        }
                                        //if read returns 0, this means the socket is closed

                                        close(read_socket);
                                        fprintf(stderr,"removing %i\n",read_socket);
                                        //remove_tag_port(s_pool.socket_state_list[i].ip,s_pool.socket_state_list[i].port);
                                        //removed the port and forget all the tags in it previously used
                                        remove_socket_by_index(i);

                                        //print_socket_pool();
                                        //Dprintf(D_CACHE, "removed socket %i at index %i\n",read_socket,i);
                                        close(s_pool.socket_state_list[counter_index].socket);
                                        //remove_tag_port(s_pool.socket_state_list[counter_index].ip,s_pool.socket_state_list[counter_index].port);
                                        //s_pool.socket_state_list[coun,ter_index].socket
                                        fprintf(stderr,"removing %i\n",s_pool.socket_state_list[counter_index].socket);
                                        //Dprintf(D_CACHE, "removed socket %i at index %i\n",s_pool.socket_state_list[counter_index].socket,counter_index);
                                        remove_socket_by_index(counter_index);
                                        //Dprintf(D_CACHE,"removed 2 sockets, %i left\n",s_pool.pool_size);
                                        //sockets are always added close with each other.first is client, then sever
                                        //if we remove 2 sockets (current and next), prevent i from increasing;
                                        //print_socket_pool();

                                        i--;
                                        continue;
                                        //p_log(log_file,"sockets removed:%i,%i\n",read_socket,csocket);

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
                                        s_pool.socket_state_list[i].buffer_tail+=got_size;
                                        s_pool.socket_state_list[i].completed_size+=got_size;
                                        
                                        //s_pool.socket_state_list[i].finished+=got_size;
                                        ////p_log(log_file, "buffer size on socket %i is now %i/%i (%i this time)\n",read_socket,
                                                //	s_pool.buffer_sizes[i], out_buffer_size, got_size);
                                    }       
                                }
                                else
                                {fprintf(stderr,"ready_to_receive error\n");}
                            }
                            else
                            {
                                    //fprintf(stderr,"Buffer is full on %i\n",read_socket);
                                    //print_socket_pool();
                            }
                            //fprintf(stderr,"real event3\n");
                            if (s_pool.socket_state_list[i].completed_size==s_pool.socket_state_list[i].job_size &&
                                            s_pool.socket_state_list[i].job_size > 0)
                            {
                                //after each receive, check if it's a total complete of the response of get_config
                                //Dprintf(L_NOTICE,"Job finished in %ith socket %i\n",i,s_pool.socket_state_list[i].socket);

                                if (s_pool.socket_state_list[i].config_tag==1)
                                {
                                    s_pool.socket_state_list[i].config_tag=0;//the sending lock can be released now
                                    int got_size=s_pool.socket_state_list[i].job_size;
                                    //Dprintf(L_NOTICE,"Changing port\n");
                                    change_port(buffer+52, got_size-52,"3334","3335");
                                }

                                if (s_pool.socket_state_list[i].op==PVFS_DATA_FLOW)
                                {

                                        //hooked by current_item->got_size+=(s_pool.socket_state_list[i].completed_size-24);
                                }

                                if (scheduler_on && s_pool.socket_state_list[i].source==CLIENT &&
                                                s_pool.socket_state_list[i].op==PVFS_DATA_FLOW
                                ) // a PVFS_WRITE
                                {

                                        //Dprintf(D_CACHE, "adding size:%i\n",s_pool.socket_state_list[i].completed_size-24);
                                    struct generic_queue_item* current_item= s_pool.socket_state_list[i].current_item;

                                    //call completion hook with current item and message if dsfq
                                    struct complete_message cmsg;
                                    cmsg.complete_size=s_pool.socket_state_list[i].completed_size-24;
                                    cmsg.current_item=current_item;
                                    //*(static_methods[scheduler_index].sch_fill_proxy_message)(cmsg.proxy_message);
                                    int ret = (*(static_methods[scheduler_index]->sch_update_on_request_completion))((void*)&cmsg);

/* write and read should all receive this coimpletion call
 * in 2l ssfqd, it just doesn't return a positive ever....because dispatch is not based on this...
 *
 *
 *
 * */


                                }
                                if (scheduler_on && s_pool.socket_state_list[i].source==SERVER &&
                                                s_pool.socket_state_list[i].op==PVFS_DATA_FLOW
                                ) // a PVFS_READ
                                {
                                    //Dprintf(D_CACHE, "adding size:%i to %ith socket %i\n",s_pool.socket_state_list[i].completed_size-24,
                                            //	counter_index, s_pool.socket_state_list[counter_index].socket);

                                    struct generic_queue_item* current_item= s_pool.socket_state_list[counter_index].current_item;
                                    //queue items are always stored at the request (client) side socket.
                                    struct complete_message cmsg;
                                    cmsg.complete_size=s_pool.socket_state_list[i].completed_size-24;
                                    cmsg.current_item=current_item;
                                    //fprintf(stderr,"%i last got size is %i\n", i, got_size);

                                    int ret = (*(static_methods[scheduler_index]->sch_update_on_request_completion))((void*)&cmsg);
                                    //return value is the whole I/O request size
                                    if (ret>0 && static_methods[scheduler_index]->sch_self_dispatch==0)
                                    {
                                    	/*cost*/
                                    	//fprintf(stderr,"read returned %i\n",ret);
                						struct timeval tv;
                					    gettimeofday(&tv, 0);
                					    int app_index=s_pool.socket_state_list[counter_index].app_index;
                					    int read_resp_time = get_response_time(counter_index, tv, PVFS_IO_WRITE);
                					    update_history(app_index, counter_index, 0, ret, read_resp_time, PVFS_IO_READ);

                					    //update to app history
                                    	/*cost*/

                                        s_pool.socket_state_list[counter_index].check_response_completion=1;
                                        s_pool.socket_state_list[counter_index].last_completion=ret;
                                        //fprintf(stderr,"[READ COMPLETE]\n");

                                        app_stats[app_index].completed_requests+=1;
                                        //first_receive=1;
                                        if (first_receive==0)
                                        {
                                            passed_completions++;
                                            app_stats[app_index].app_exist=app_stats[app_index].app_exist+1;

                                            fprintf(stderr,"client %s completed++ for app %i\n",s_pool.socket_state_list[counter_index].ip, app_index+1);
                                            int k;
                                            for (k=0;k<num_apps;k++)
                                            {
                                                    if (app_stats[k].app_exist<MESSAGE_START_THRESHOLD)
                                                    {
                                                            fprintf(stderr,"%s: Not counting, not all apps exist yet: app %i has only %i<%i items\n", log_prefix, k+1, app_stats[k].app_exist, MESSAGE_START_THRESHOLD);
                                                            //fprintf(stderr,"Not counting, not all apps exist yet: app %i has only %i<%i items\n", k+1, app_exist[k], MESSAGE_START_THRESHOLD);

                                                            break;
                                                    }
                                            }
                                            if (k>=num_apps)
                                            {
                                                    first_receive=1;
                                                    gettimeofday(&first_count_time,0);
                                            }
                                            //fprintf(stderr,"%i completion passed, not starting counter yet\n", passed_completions);
                                        }

                                        if (timer_stop || !first_receive)
                                        {
                                                //fprintf(stderr,"Timer stopped or not started\n");
                                        }
                                        else
                                        {
                                            //if (pthread_mutex_lock (&counter_mutex)!=0)
                                            //{
                                                    //Dprintf(D_CACHE, "error locking when incrementing counter\n");
                                            //}

                                            (*(static_methods[scheduler_index]->sch_add_ttl_throughput))(s_pool.socket_state_list[counter_index].last_completion, app_index);

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
														r.item=s_pool.socket_state_list[s].current_item;
														struct generic_queue_item* new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
														while (new_item!=NULL)
														{
															s_pool.socket_state_list[new_item->socket_data->unlock_index].locked=0;
															new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
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
                                        {
                                                //fprintf(stderr, "no more jobs found, depth decreased...\n");

                                        }
                                        else
                                        {
                                    	    struct timeval tv;
                                    	    gettimeofday(&tv, 0);
                                        	update_release_time(new_item->socket_data->unlock_index, tv);


                                            //if (pthread_mutex_lock (&counter_mutex)!=0)
                                            {
                                                    //Dprintf(D_CACHE, "error locking when getting counter\n");
                                            }
                                            //next->unlock_index=i;
/*											fprintf(stderr, "unlocking %i th socket %i\n",
                                                            new_item->socket_data->unlock_index,
                                                            s_pool.socket_state_list[new_item->socket_data->unlock_index].socket);*/
                                            s_pool.socket_state_list[new_item->socket_data->unlock_index].locked=0;

                                        }

                                    }
                                    else if (ret==-1)
                                    {
                                            fprintf(stderr, "error...got_size > task_size");
                                            exit(-1);

                                    }

                                }
                                //Dprintf(D_CACHE, "Job Finished on %s:%i\n",s_pool.socket_state_list[i].ip,s_pool.socket_state_list[i].port);
                                s_pool.socket_state_list[i].completed_size=0;
                                //for write:
                                //1. if we recieve from a client
                                //2. if the message is data flow
                                //3. if added got_size equals to task size
                                //then:
                                //get next queued item
                                //1. unblock socket
                            }
                            if (s_pool.socket_state_list[i].completed_size>s_pool.socket_state_list[i].job_size)
                            {
                                    fprintf(stderr, "We got more than we expected!\n");
                                    exit(-1);
                            }

                            //fprintf(stderr,"real event4\n");
                        }//end if not locked
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

                    //fprintf(stderr,"real2 event1\n");

                    //Dprintf(D_CALL, "Sending on %i\n",s_pool.poll_list[i].fd);

                    if (s_pool.socket_state_list[i].mode!=WRITE)
                    {
                        //sockets are duplex; thus, any ports might be able to be written to without blocking
                        //we use the maintained mode information to keep track of the direction of flow for a pair of sockets
                        //Dprintf(D_CALL,"This port %i is not supposed to write!\n", s_pool.socket_state_list[i].socket);
                        continue;
                    }

                    //drain the buffer on the reader's side if any
                    counter_socket=s_pool.socket_state_list[i].counter_part;
                    counter_index=s_pool.socket_state_list[i].counter_index;//find_counter_part(s_pool.poll_list[i].fd);

                    //Dprintf(D_CACHE, "i:%i fd:%i c_socket:%i index:%i\n",i, s_pool.poll_list[i].fd, counter_socket, counter_index);


                    
                    int ready_size=s_pool.socket_state_list[counter_index].buffer_tail-s_pool.socket_state_list[counter_index].buffer_head;//bytes we want to send
                    //Dprintf(D_CALL,"LEFT_SIZE:%i, CONFIG_TAG:%i, READY:%i\n", ready_size, s_pool.socket_state_list[counter_index].config_tag,
                                    //s_pool.socket_state_list[counter_index].ready_to_receive);

                    //fprintf(stderr,"real2 event2\n");
                    if ( ready_size > 0 && (!s_pool.socket_state_list[counter_index].config_tag )
                                    //||
                                    //s_pool.socket_state_list[counter_index].config_tag && s_pool.socket_state_list[counter_index].ready_to_receive==1)

                    )//this is to avoid sending partial get_config buffer without modifying it.
                    {

                        char * ready_buffer = s_pool.socket_state_list[counter_index].buffer+s_pool.socket_state_list[counter_index].buffer_head;//reader's buffer

                        //output_stream(ready_buffer, ready_size);
                        int sent_size=send(s_pool.poll_list[i].fd, ready_buffer, ready_size,0);
                        if (sent_size==0 || sent_size==-1)
                        {
                                fprintf(stderr, "Sent error on %i; returning %i\n",s_pool.poll_list[i].fd, sent_size);
                                exit(-1);

                        }
                        else
                        {
                            //fprintf(stderr,"#Sent %i bytes to %i!\n",sent_size, s_pool.poll_list[i].fd);
                            s_pool.socket_state_list[counter_index].buffer_head += sent_size;//decrease the buffer size by the amount we have successfully sent
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
                            else
                            {
                            	//fprintf(stderr,"send complete on %i...\n",i); could be partial complete!!!

                            	if (s_pool.socket_state_list[counter_index].buffer_size==s_pool.socket_state_list[counter_index].buffer_head)

                            	{
                            		//completion of message protocol declared size!
                            		//fprintf(stderr,"forwarding complete, cleaning buffer on socket %i\n",s_pool.socket_state_list[counter_index].socket);
					//				free(s_pool.socket_state_list[counter_index].buffer);

									s_pool.socket_state_list[counter_index].buffer=NULL;
									s_pool.socket_state_list[counter_index].buffer_tail=0;
									s_pool.socket_state_list[counter_index].buffer_head=0;
									s_pool.socket_state_list[counter_index].buffer_size=0;
                            	}


                            }
                        }
                    }
                    else//outgoing socket is ready to be written, but the buffer is empty
                    {
                            //p_log(log_file, "%i: outgoing socket is ready, but buffer is empty from %i/%i (i:%i, counter:%i)\n",
                                    //	s_pool.poll_list[i].fd, s_pool.poll_list[counter_index].fd,s_pool.buffer_sizes[counter_index],i,counter_index);
                    }
                    //fprintf(stderr,"real2 event3\n");
                }//end pollout
            }//end iterating revents loop
        }//end if num_poll>0
    }//end polling
    return 0;
}



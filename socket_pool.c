/*
 * socket_pool.c
 *
 *  Created on: Oct 4, 2010
 *      Author: yiqi
 */
#include "socket_pool.h"
#include "proxy2.h"
#include "logging.h"
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "scheduler_SFQD.h"
/**
 * Finds the index in the socket pool the counter part of a socket number
 * \param socket is the socket number of which the counter part will looked for
 * \return Returns -1 on failure, or the counte part socket index in the socket pool
 * */
int find_counter_part(int socket)
{
	int i=0;
	for (;i<s_pool.pool_size;i++)
	{
		if (s_pool.socket_state_list[i].counter_part==socket)
		{
			return i;
		}
	}
	Dprintf(D_CACHE,"ERROR! counterpart of %i not found\n",socket);
	//print_socket_pool();
	return -1;
}


/**
 * Removes a socket from the socket pool by its index in the pool.  Its counter part should together be removed by the caller immediately.
 *
 * \param socket is the socket to be removed
 *
 * */
void remove_socket_by_index(int i)
{
	//fprintf(stderr,"removing\n");
	s_pool.pool_size--;
    if (s_pool.socket_state_list[i].buffer!=NULL)
    {
    	free(s_pool.socket_state_list[i].buffer);
    	s_pool.socket_state_list[i].buffer_head=0;
    	s_pool.socket_state_list[i].buffer_size=0;
    	s_pool.socket_state_list[i].buffer_tail=0;
    }
	//memcpy(&s_pool.socket_state_list[i],&s_pool.socket_state_list[s_pool.pool_size],sizeof(struct socket_state));
	s_pool.socket_state_list[i]=s_pool.socket_state_list[s_pool.pool_size];
	s_pool.poll_list[i]=s_pool.poll_list[s_pool.pool_size];

	int counter_index=s_pool.socket_state_list[i].counter_index;

	s_pool.socket_state_list[counter_index].counter_index=i;

}

/**
 * Removes a socket from the socket pool.  Its counter part should together be removed by the caller immediately.
 *
 * \param socket is the socket to be removed
 *
 * */
void remove_socket(int socket)
{
	//fprintf(stderr,"removing\n");
	int i=0;
	int found=0;
	for (;i<s_pool.pool_size;i++)
	{
		if (s_pool.socket_state_list[i].socket==socket)
		{
			remove_socket_by_index(i);
			break;
		}
	}
	if (!found)
	{
		fprintf(stderr, "Socket not found:%i\n",socket);
	}

}


/**
 * prints the socket pool's socket information one by one
 * */
void increase_socket_pool()
{
	int new_size=s_pool.pool_capacity*2;

	struct socket_state * new_states= (struct socket_state *)malloc(new_size*
			sizeof(struct socket_state));
	struct pollfd * new_fds= (struct pollfd *)malloc(new_size*
				sizeof(struct pollfd));
	
	memcpy(new_states, s_pool.socket_state_list, s_pool.pool_size*sizeof(struct socket_state));
	memcpy(new_fds, s_pool.poll_list, s_pool.pool_size*sizeof(struct pollfd));

	free(s_pool.poll_list);
	free(s_pool.socket_state_list);
        
	s_pool.poll_list=new_fds;
	s_pool.socket_state_list=new_states;

}
/**
 * Adds a socket pair into the socket pool
 * Increases the pool size when necessary
 * \param socket is the socket number of a client side socket
 * \param counter_part is the socket number of a server side socket
 * \param mode is the client side operation mode
 * \param target is the target side of client's socket in the proxy
 * \param source is the source side of client's socket in the proxy
 * */
int add_socket(int socket, int counter_part,
		enum mode mode, enum node target, enum node source, char* client_ip, char* server_ip, int client_port, int server_port)
{

	if (s_pool.pool_size==s_pool.pool_capacity)
	{
		increase_socket_pool();

	}

		s_pool.socket_state_list[s_pool.pool_size].socket=socket;
		s_pool.socket_state_list[s_pool.pool_size].counter_part=counter_part;
		s_pool.socket_state_list[s_pool.pool_size].ip=client_ip;
		s_pool.socket_state_list[s_pool.pool_size].port=client_port;
		s_pool.socket_state_list[s_pool.pool_size].mode=mode;
		s_pool.socket_state_list[s_pool.pool_size].target=target;
		s_pool.socket_state_list[s_pool.pool_size].source=source;
		s_pool.socket_state_list[s_pool.pool_size].completed_size=0;
		s_pool.socket_state_list[s_pool.pool_size].job_size=0;
		s_pool.socket_state_list[s_pool.pool_size].locked=0;
		s_pool.socket_state_list[s_pool.pool_size].has_block_item=0;
		s_pool.socket_state_list[s_pool.pool_size].last_tag=-1;
		s_pool.socket_state_list[s_pool.pool_size].current_tag=-2;
		s_pool.socket_state_list[s_pool.pool_size].current_item=NULL;
		s_pool.socket_state_list[s_pool.pool_size].check_response_completion=0;
	    struct timeval tv;
	    gettimeofday(&tv, 0);
	    s_pool.socket_state_list[s_pool.pool_size].last_work_time_r=tv;
	    s_pool.socket_state_list[s_pool.pool_size].last_work_time_w=tv;
	    s_pool.socket_state_list[s_pool.pool_size].exp_smth_value_r=1000;
	    s_pool.socket_state_list[s_pool.pool_size].exp_smth_value_w=1000;
	    s_pool.socket_state_list[s_pool.pool_size].last_exp_r_app=1000;
	    s_pool.socket_state_list[s_pool.pool_size].last_exp_r_machine=1000;
	    s_pool.socket_state_list[s_pool.pool_size].last_exp_w_app=1000;
	    s_pool.socket_state_list[s_pool.pool_size].last_exp_w_machine=1000;

	    s_pool.socket_state_list[s_pool.pool_size].last_exp2_r_app=1000;
	    s_pool.socket_state_list[s_pool.pool_size].last_exp2_r_machine=1000;
	    s_pool.socket_state_list[s_pool.pool_size].last_exp2_w_app=1000;
	    s_pool.socket_state_list[s_pool.pool_size].last_exp2_w_machine=1000;


	    s_pool.poll_list[s_pool.pool_size].fd=socket;
		s_pool.poll_list[s_pool.pool_size].events=POLLIN;

		s_pool.socket_state_list[s_pool.pool_size].buffer=NULL;
		s_pool.socket_state_list[s_pool.pool_size].buffer_size=0;
		s_pool.socket_state_list[s_pool.pool_size].buffer_head=0;
        s_pool.socket_state_list[s_pool.pool_size].buffer_tail=0;
                
		s_pool.pool_size++;

		if (counter_part==0)
		{
			return -1;
		}
		s_pool.socket_state_list[s_pool.pool_size-1].counter_index=s_pool.pool_size;

		//Dprintf(D_CACHE," source:%s:%i->target:%s:%i\n",client_ip, client_port, server_ip, server_port);

		struct ip_application* ipa=ip_weight(client_ip);
		int weight=ipa->weight;
		int app_index=ipa->app_index;
		int deadline=ipa->deadline;
		s_pool.socket_state_list[s_pool.pool_size-1].weight=weight;
		s_pool.socket_state_list[s_pool.pool_size-1].deadline=deadline;
		//Dprintf(D_CACHE,"connected ip %s has a weight of %i at %i\n",client_ip,weight,app_index);
		s_pool.socket_state_list[s_pool.pool_size-1].app_index=app_index;
		if (s_pool.pool_size==s_pool.pool_capacity)
		{
			fprintf(stderr,"increasing\n");
			increase_socket_pool();
		}

		s_pool.socket_state_list[s_pool.pool_size].socket=counter_part;
		s_pool.socket_state_list[s_pool.pool_size].counter_part=socket;
		s_pool.socket_state_list[s_pool.pool_size].ip=server_ip;
		s_pool.socket_state_list[s_pool.pool_size].port=server_port;
		s_pool.socket_state_list[s_pool.pool_size].mode=!mode;//reversed mode
		s_pool.socket_state_list[s_pool.pool_size].target=target;
		s_pool.socket_state_list[s_pool.pool_size].source=source;
		s_pool.socket_state_list[s_pool.pool_size].completed_size=0;
		s_pool.socket_state_list[s_pool.pool_size].job_size=0;
		s_pool.socket_state_list[s_pool.pool_size].counter_index=s_pool.pool_size-1;
		s_pool.socket_state_list[s_pool.pool_size].locked=0;
		s_pool.socket_state_list[s_pool.pool_size].has_block_item=0;
		s_pool.socket_state_list[s_pool.pool_size].current_item=NULL;
		s_pool.socket_state_list[s_pool.pool_size].last_tag=-1;
		s_pool.socket_state_list[s_pool.pool_size].current_tag=-1;

		s_pool.poll_list[s_pool.pool_size].fd=counter_part;
		s_pool.poll_list[s_pool.pool_size].events=POLLOUT;
		s_pool.poll_list[s_pool.pool_size].revents=0;

		s_pool.socket_state_list[s_pool.pool_size].buffer=NULL;
		s_pool.socket_state_list[s_pool.pool_size].buffer_size=0;
		s_pool.socket_state_list[s_pool.pool_size].buffer_head=0;
        s_pool.socket_state_list[s_pool.pool_size].buffer_tail=0;
		s_pool.pool_size++;
		return app_index;

}
/**
 * Creates a socket pool according to the capacity specified
 * \param capacity is the initial size o the pool
 *
 * */
void create_socket_pool(int capacity)
{

	int new_size=capacity;

	struct socket_state * new_states= (struct socket_state *)malloc(new_size*sizeof(struct socket_state));
	struct pollfd * new_fds= (struct pollfd *)malloc(new_size*sizeof(struct pollfd));


	s_pool.pool_capacity=new_size;
	s_pool.poll_list=new_fds;
	s_pool.socket_state_list=new_states;

        //initial values?
}

/**
 * prints the socket pool's socket information one by one
 * */
void print_socket_pool()
{
	int i=0;
	fprintf(stderr, "Pool capacity:%i, size:%i\n",s_pool.pool_capacity,s_pool.pool_size);
	for (;i<s_pool.pool_size;i++)
	{
		fprintf(stderr, "%ith socket:%i/poll fd:%i; mask:%i\n",i,s_pool.socket_state_list[i].socket,s_pool.poll_list[i].fd, s_pool.poll_list[i].events);
/*
				);
		Dprintf(D_CACHE, "target:%i, source:%i, mode:%i\n",s_pool.socket_state_list[i].target,
				s_pool.socket_state_list[i].source,s_pool.socket_state_list[i].mode);
		Dprintf(D_CACHE, "buf_size:%i, counterpart:%i,c_index:%i\n",s_pool.buffer_sizes[i],
				s_pool.socket_state_list[i].counter_part,s_pool.socket_state_list[i].counter_index);
		Dprintf(D_CACHE, "locked:%i, ready_to_receive:%i, ip: %s",s_pool.socket_state_list[i].locked,
				s_pool.socket_state_list[i].ready_to_receive, s_pool.socket_state_list[i].ip  );
		Dprintf(D_CACHE, "---------------------\n");
*/

	}

	//Dprintf(D_CACHE, "n");
}

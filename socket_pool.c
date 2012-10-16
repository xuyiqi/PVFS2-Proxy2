/*
 * socket_pool.c
 *
 *  Created on: Oct 4, 2010
 *      Author: yiqi
 */
#include "proxy2.h"
#include "socket_pool.h"

#include "logging.h"
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "config.h"
#include "scheduler_SFQD.h"

struct request_state * find_request(int index, int tag, int rank)
{
	PINT_llist_p l_p = s_pool.socket_state_list[index].req_state_data;

    if (!l_p || !l_p->next )	/* no or empty list */
    	return (NULL);
    int count=0;
    for (l_p = l_p->next; l_p; l_p = l_p->next)
    {
    	if (tag == ((struct request_state *)(l_p->item))->current_tag)
    	{
    		count++;
    		if (count==rank)
    		{
    			return (struct request_state *)(l_p->item);
    		}
    	}
    }
    return (NULL);
}


/**
 * Removes a socket from the socket pool by its index in the pool.  Its counter part should together be removed by the caller immediately.
 *
 * \param socket is the socket to be removed
 *
 * */
void remove_socket_by_index(int socket_index)
{
	//fprintf(stderr,"removing\n");
	s_pool.pool_size--;

	PINT_llist_p app_reqs = s_pool.socket_state_list[socket_index].req_state_data;
	if (app_reqs == NULL)
	{
		fprintf(stderr, "search failed upon first pointer to the req state list\n");
		exit(-1);
	}
	PINT_llist_p queue = app_reqs->next;

	int count = 0;

	fprintf(stderr, "list has %i items\n",PINT_llist_count(app_reqs));
	while (queue!=NULL)
	{
		count++;
		struct request_state * current_item = ((struct request_state *)(queue->item));
		if (current_item == NULL)
		{
			fprintf(stderr, "search failed on a certain req state list item\n");
			exit(-1);
		}
		long tag = current_item->current_tag;
		fprintf(stderr,"removing tag %i op %s\n", tag, ops[current_item->op]);
		queue = queue->next;
		//the removal procedure will change the next pointer, so we keep it for the while loop
		//just before removing
		struct request_state * removed_item = (struct request_state *)
				PINT_llist_rem(app_reqs, (void*)tag,  list_req_state_comp_curr);
		if (removed_item == NULL)
		{
			fprintf(stderr, "removal failed returning nothing\n");
			exit(-1);
		}
		if (removed_item->buffer!=NULL)
		{
			free(removed_item->buffer);
		}
		free(removed_item);
	}
	fprintf(stderr,"%i messages removed on the socket\n", count);
	if (app_reqs!=NULL)
	{
		free(app_reqs);
	}
	else
	{
		fprintf(stderr,"app_req is null\n");
	}
	s_pool.socket_state_list[socket_index].req_state_data = NULL;

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
		exit(-1);
	}

}


/**
 * every call to this function doubles the current capacity of the socket pool
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

void initialize_request_states(int index)
{
	s_pool.socket_state_list[index].req_state_data = PINT_llist_new();
}

struct request_state * add_request_to_socket(int index, int tag)
{
	if (s_pool.socket_state_list[index].req_state_data == NULL )
	{
		fprintf(stderr,"add_req error: the socket state's req state data is not initialized yet\n");
		exit(-1);
	}

	struct request_state * new_rs = (struct request_state *)malloc(sizeof(struct request_state));
	if (new_rs == NULL)
	{
		fprintf(stderr, "malloc of new request state failed!\n");
		exit(-1);
	}
	new_rs->buffer = NULL;
	new_rs->buffer_head = 0;
	new_rs->buffer_size = 0;
	new_rs->buffer_tail = 0;

	new_rs->last_flow = 0;
	new_rs->current_item = NULL;
	new_rs->completed_size = 0;
	new_rs->job_size = 0;
	new_rs->last_tag = -1;
	new_rs->current_tag = -1;
	new_rs->locked = 0;
	new_rs->meta_response = 0;

	PINT_llist_add_to_tail(s_pool.socket_state_list[index].req_state_data, (void*)new_rs);

	return new_rs;
}

int remove_request_from_socket(int index, long tag)
{
	struct request_state * rs = PINT_llist_rem(
	    s_pool.socket_state_list[index].req_state_data,
	    (void *)tag,
	    list_req_state_comp_curr);
	if (rs == NULL)
	{
		fprintf(stderr,"request remove failed: item not found (tag %i)\n", tag);
		exit(-1);
	}

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
int add_socket_to_pool(int socket, int counter_part,
		int accept_mode, enum node target, enum node source,
		char* client_ip, char* server_ip, int client_port, int server_port)
{

	if (s_pool.pool_size==s_pool.pool_capacity)
	{
		increase_socket_pool();
	}

	s_pool.socket_state_list[s_pool.pool_size].socket=socket;
	s_pool.socket_state_list[s_pool.pool_size].counter_socket=counter_part;
	s_pool.socket_state_list[s_pool.pool_size].ip=client_ip;
	s_pool.socket_state_list[s_pool.pool_size].port=client_port;
	s_pool.socket_state_list[s_pool.pool_size].accept_mode=accept_mode;
	s_pool.socket_state_list[s_pool.pool_size].target=target;
	s_pool.socket_state_list[s_pool.pool_size].source=source;

	initialize_request_states(s_pool.pool_size);

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


	s_pool.pool_size++;

	if (counter_part==0)
	{
		return -1;
	}
	s_pool.socket_state_list[s_pool.pool_size-1].counter_socket_index=s_pool.pool_size;

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
	s_pool.socket_state_list[s_pool.pool_size].counter_socket=socket;
	s_pool.socket_state_list[s_pool.pool_size].ip=server_ip;
	s_pool.socket_state_list[s_pool.pool_size].port=server_port;
	s_pool.socket_state_list[s_pool.pool_size].accept_mode=0;

	initialize_request_states(s_pool.pool_size);

	s_pool.socket_state_list[s_pool.pool_size].counter_socket_index=s_pool.pool_size-1;
	s_pool.poll_list[s_pool.pool_size].fd=counter_part;
	s_pool.poll_list[s_pool.pool_size].events=POLLIN;
	s_pool.poll_list[s_pool.pool_size].revents=0;
	s_pool.socket_state_list[s_pool.pool_size].target=source;
	s_pool.socket_state_list[s_pool.pool_size].source=target;
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
		fprintf(stderr, "%ith socket:%i/poll fd:%i; mask:%i\n",
				i,s_pool.socket_state_list[i].socket,s_pool.poll_list[i].fd, s_pool.poll_list[i].events);
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

struct request_state * create_counter_rs(struct request_state * original_rs, int counter_index)
{
	int tag =  original_rs->current_tag;
	struct request_state * counter_rs = add_request_to_socket(counter_index, tag);
	if ( counter_rs == NULL )
	{
		fprintf(stderr,"create_counter failed because add_request returned null\n");
	}
	counter_rs->current_tag = tag;
	counter_rs->original_request = original_rs;
	counter_rs->pvfs_io_type = original_rs->pvfs_io_type;
	counter_rs->op = original_rs->op;//initial response to the request is always the same op code
	fprintf(stderr,"original op is %i tag is %i\n", counter_rs->op, tag);
	counter_rs->config_tag = 0;
	counter_rs->locked = 0;
	counter_rs->job_size = -1;
	counter_rs->buffer_size = 0;
	counter_rs->buffer_head = 0;
	counter_rs->buffer_tail = 0;
	counter_rs->meta_response = 0;
	return counter_rs;
}

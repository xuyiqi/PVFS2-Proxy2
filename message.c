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


#include "message.h"
#include "scheduler_main.h"
#include "config.h"
#include "socket_pool.h"
int PVFS_IO_READ = 1;
int PVFS_IO_WRITE = 2;
int passed_completions=0;
extern int* completed_requests;
extern char* log_prefix;
long long total_throughput=0;
extern int total_weight;


int finished[10];
int completerecv[10];
int completefwd[10];
int get_bmi_header(char * header, int eheader, int read_socket)
{

	int ret=0;
	int count=0;

peek_start:
	ret=recv(read_socket, header, eheader,MSG_PEEK|DEFAULT_MSG_FLAGS|MSG_DONTWAIT);
	if(ret == 0)
	{
		fprintf(stderr,"socket %i closing, ignore next error please\n", read_socket);
		ret=-1;
	}
	if (ret < 0)
	{
		fprintf(stderr,"socket %i returned negative\n", read_socket);
	}
	else if (ret<eheader) //socket returned larger than 0, but less than requested bytes
	{
		ret=-2;
	}
	return ret;
}

int is_IO(enum PVFS_server_op operation)
{
	return (operation == PVFS_SERV_IO || operation == PVFS_SERV_SMALL_IO || operation == PVFS_SERV_WRITE_COMPLETION);
}

int is_meta(enum PVFS_server_op operation)
{

#ifdef NEG_IFMETA
	if (
	operation != PVFS_SERV_INVALID /* 0 */
	&& operation != PVFS_SERV_TRUNCATE /* 1 */
	&& opeartion != PVFS_SERV_SETATTR /* 5 */
	&& operation != PVFS_SERV_CHDIRENT /* 9 */
	&& operation != PVFS_SERV_TRUNCATE /* 10 */
	&& operation != PVFS_SERV_FLUSH /* 15 */
	&& operation != PVFS_SERV_GETCONFIG /* 13 */
	&& operation != PVFS_SERV_MGMT_SETPARAM /* 16 */
	&& operation != PVFS_SERV_MGMT_NOOP /* 17 */
	&& operation <  PVFS_SERV_PERF_UPDATE /* 19 */

	/*
	PVFS_SERV_MGMT_PERF_MON = 20,
	PVFS_SERV_MGMT_ITERATE_HANDLES = 21,
	PVFS_SERV_MGMT_DSPACE_INFO_LIST = 22,
	PVFS_SERV_MGMT_EVENT_MON = 23,
	PVFS_SERV_MGMT_REMOVE_OBJECT = 24,
	PVFS_SERV_MGMT_REMOVE_DIRENT = 25,
	PVFS_SERV_MGMT_GET_DIRDATA_HANDLE = 26,
	&& operation >  PVFS_SERV_JOB_TIMER =  27, not a real protocol request
	&& operation != PVFS_SERV_PROTO_ERROR 28
	&& opeartion != PVFS_SERV_GETEATTR 29

	&& operation != PVFS_SERV_SETEATTR 30
	&& operation != PVFS_SERV_DELEATTR 31 */
	&& operation > PVFS_SERV_LISTEATTR /* 32 */
	&& opeartion != PVFS_SERV_BATCH_CREATE /* 35 */
	&& operation != PVFS_SERV_BATCH_REMOVE /* 36 */
	&& operation != PVFS_SERV_PRECREATE_POOL_REFILLER /* 37, not a real protocol request */
	&& operation != PVFS_SERV_UNSTUFF /* 38 */
	&& is_IO(operation) != 1
	)
		return 1;
	else
		return 0;
#else
	if (operation == PVFS_SERV_GETATTR ||
		 operation == PVFS_SERV_REMOVE ||
		 operation == PVFS_SERV_CREATE ||
		 operation == PVFS_SERV_CRDIRENT ||
		 operation == PVFS_SERV_RMDIRENT ||
		 operation == PVFS_SERV_READDIR ||
		 operation == PVFS_SERV_LISTATTR ||
		 operation == PVFS_SERV_LOOKUP_PATH ||
		 operation == PVFS_SERV_MKDIR ||
		 operation == PVFS_SERV_STATFS
	)
	{
		//fprintf(stderr, "%s is meta\n", ops[operation]);
		return 1;
	}
	else
	{
		return 0;
	}
#endif
}

void check_all_app_stat()
{
	int k;
	for (k=0;k<num_apps;k++)
	{
		if (app_stats[k].app_exist<MESSAGE_START_THRESHOLD)
		{
			fprintf(stderr,"%s: Not counting, not all apps exist yet: app %i has only %i<%i items\n", log_prefix, k+1, app_stats[k].app_exist, MESSAGE_START_THRESHOLD);
			break;
		}
	}
	if (k>=num_apps)
	{
		first_receive=1;
		gettimeofday(&first_count_time,0);
	}
}
/* index is from the reading socket's end, which means data is read from socket[index]
 * we find the peer socket and from its head of the list (ordered by receiving time, FIFO)
 * get any available item that is not sent yet and put it on this reading socket,
 * which must also do writing whenever it can
 * */
struct request_state * update_socket_current_send(int index)
{
	int counter_index = s_pool.socket_state_list[index].counter_socket_index;
	if (s_pool.socket_state_list[index].current_send_item == NULL)//only if there is no work going on for this socket
	{
		//fprintf(stderr,"updating send item for socket %i\n", index);
		struct request_state * rs = PINT_llist_search(
				s_pool.socket_state_list[counter_index].req_state_data,
				(void *)NULL,
				list_req_state_buffer_nonempty_nonlock_comp);
		if (rs == NULL)
		{
			//fprintf(stderr,"nothing is found on the socket state list to be sent!\n");
			return NULL;
		}
		s_pool.socket_state_list[index].current_send_item = rs;
		assert(s_pool.socket_state_list[index].current_send_item->locked != 1);
		/*fprintf(stderr,"item found! size %i op %i\n",
				s_pool.socket_state_list[index].current_send_item->buffer_size,
				s_pool.socket_state_list[index].current_send_item->op);
		*/
		return rs;
	}
	//otherwise, let the proxy do its job forwarding data
	//the current_send_item is a pointer to the peer socket's received data
	//they will be destroyed both only when the send buffer is depleted.
	return NULL;
}

/* whenever we start reading from a socket, it means that the content belongs
 * to an atomic operation which may not be interleaved with others (although
 * there should be concurrencies on the socket, the smallest atomic unit is a
 * single complete message). Thus, if current_receive_item is NULL, we must
 * get it right. And we take it from the tail of the working list, because it
 * must also correspods to the last-received item. The important thing is to udpate
 * the current receive item right after it is put in the tail of the working list
 * */
struct request_state * update_socket_current_receive(int index, long tag)
{

	//tag >0: specific tag
	//should also be last received item of the tag from the head of the list
	if (tag < 0 )
	{
		fprintf(stderr, "error: tag is negative: %li\n", tag);
		exit(-1);
	}
	if (s_pool.socket_state_list[index].current_receive_item == NULL)
	{
		struct request_state * rs = find_last_request(index, tag);
		if (rs == NULL)
		{
			return NULL;
		}
		s_pool.socket_state_list[index].current_receive_item = rs;
		return rs;
	}
	return NULL;
}



int get_io_type(char * buffer, int size,int small)
{
	return dump_header2(buffer,small);
}

/* function is called when data is read from the client, and
 * the current receive item on the socket is set to null,
 * which is done when the receipt of an entire item completes.
 * */
int check_request(int index)
{
	int eheader=BMI_HEADER_LENGTH;
	unsigned char header[eheader];
	int read_socket=s_pool.poll_list[index].fd;
	//fprintf(stderr,"checking from socket %i\n", read_socket);

	struct timeval peek_time;
	gettimeofday(&peek_time, 0);

	int ret = get_bmi_header(header, eheader, read_socket);
	int counter_index=s_pool.socket_state_list[index].counter_socket_index;

	long long tag = -1;
	if (ret!=eheader)
	{
		fprintf(stderr,"Error getting bmi header for request. Expected: %i, got:%i\n",eheader, ret);
		//s_pool.socket_state_list[index].incomplete_mode = 1;
	}
	else
	{
		//long long mnr=output_param(header, 0, 4 , "magic number",NULL,0);
		//output_param(header, 4, 4 , "mode",NULL,0);
		tag=output_param(header, 8, 8 , "tag",NULL,0);
		long long size=output_param(header, 16, 8 , "size",NULL,0);
		struct request_state* new_rs;

		struct timeval receive_time;
		gettimeofday(&receive_time, 0);

		if (s_pool.socket_state_list[index].incomplete_mode ==0)
		{

			new_rs = add_request_to_socket(index, tag);
			//a request is only created when we first see the whole pvfs header, so even if previous multiple tries exist
			//with bmi tag available, we cannot include the first receive to start at that time.
			new_rs->first_receive_time = receive_time;
			new_rs->last_peek_time = peek_time;
			new_rs->last_tag = -1;
			new_rs->current_tag = tag;
			new_rs->job_size = size + BMI_HEADER_LENGTH;
			new_rs->buffer = malloc((size+BMI_HEADER_LENGTH)*sizeof(char));
			new_rs->buffer_head = 0;
			new_rs->buffer_tail = 0;
			new_rs->buffer_size = size + BMI_HEADER_LENGTH;
			new_rs->completed_size = 0;
			new_rs->locked = 0;
			new_rs->pvfs_io_type = -1;
			new_rs->meta_response = 0;
			//fprintf(stderr,"new request on socket %i, tag %i\n", index, tag);
		}
		else
		{
			new_rs = find_last_request(index, tag);
			if (new_rs == NULL)
			{
				fprintf(stderr,"incomplete old request is null on %i, tag %lli\n", index, tag);
				exit(-1);
			}
			else
			{
				//fprintf(stderr,"old request not found on %i, tag %i\n", index, tag);
			}
		}
		struct request_state * old_rs = find_request(counter_index, tag, 1);
		//comparing on the server side, the last_tag, which is set by receiving a response, but initially a -1

		if (old_rs != NULL && tag == old_rs->current_tag)//is_tag_used(s_pool.socket_state_list[counter_index].ip, s_pool.socket_state_list[counter_index].port, tag))
		{
			new_rs->config_tag=0;
			new_rs->op=PVFS_DATA_FLOW;

			if (old_rs->original_request == NULL)
			{
				fprintf(stderr,"old response's original request is missing\n");
				exit(-1);
			}
			new_rs->original_request = old_rs->original_request;
			ret=1;//passed, no special processing or delaying on a data flow
			//fprintf(stderr,"dumping flow from client......content size is %lli, tag is %lli, ip is %s:%i\n",size,tag,s_pool.socket_state_list[index].ip, s_pool.socket_state_list[index].port);
		}
		else if (old_rs!=NULL && tag != old_rs->current_tag)
		{
			fprintf(stderr, "tag is %lli, current tag is %i, who don't match\n",tag, old_rs->current_tag);
			exit(-1);
		}
		else //old_rs == null
		{
			eheader=size+BMI_HEADER_LENGTH;//size of the message, does not include data flow possibility, so it's a few bytes at most
			unsigned char header2[eheader];

			peek_start2:
			ret=recv(read_socket, header2, eheader,MSG_PEEK|DEFAULT_MSG_FLAGS|MSG_DONTWAIT);

			if(ret !=eheader)
			{
				fprintf(stderr, "Error getting header of pvfs for request. Expected: %i, got:%i\n",eheader, ret);
				ret=-3;//0, -1 and -2 are occupied by get_bmi_header
				s_pool.socket_state_list[index].incomplete_mode = 1;
			}
			else
			{
				s_pool.socket_state_list[index].incomplete_mode = 0;
				long long operation = output_param(header2, 32, 4, "pvfs_operation", ops,40);

				new_rs->op = operation;
				struct request_state * counter_rs = create_counter_rs(new_rs, counter_index);
				counter_rs->pvfs_io_type = new_rs->pvfs_io_type;
				counter_rs->last_tag = -1;
				counter_rs->current_tag = tag;

				//fprintf(stderr,"tag %i op %i\n",tag, operation);
				if ( operation == PVFS_SERV_IO || operation == PVFS_SERV_SMALL_IO )
				{
					int io_type;
					int small=0;
					if ( operation == PVFS_SERV_SMALL_IO )
					{
						small=1;
					}
					io_type=get_io_type(header2,eheader,small);
					new_rs->pvfs_io_type = io_type;
					counter_rs->pvfs_io_type = io_type;

					//struct dist* dist = dump_header(header2,REQUEST,s_pool.socket_state_list[index].ip);

					//int this_size=logical_to_physical_size_dparam(dist);

					if (scheduler_on && ( io_type == PVFS_IO_WRITE || small == 1 ))
					{
						//fprintf(stderr,"W W W W W W W W W W W W W W W W W W W W W W W W W W W W W W W\n");
						struct dist* dist = dump_header(header2,REQUEST,s_pool.socket_state_list[index].ip);
						int this_size = logical_to_physical_size_dparam(dist);

						//if (add_request(index, index,this_size ,tag, io_type, NULL, size)) NULL was defined to be char* request
						struct socket_info * si = (struct socket_info *)malloc(sizeof(struct socket_info));
						struct pvfs_info * pi = (struct pvfs_info *)malloc(sizeof(struct pvfs_info));
						memset(si, 0, sizeof(struct socket_info));
						memset(pi, 0, sizeof(struct pvfs_info));
						if (small==1)
						{
							pi->current_data_size=dist->small_total;
							//fprintf(stderr,"small io size is %i\n",dist->small_total);
						}
						else
						{
							pi->current_data_size=this_size;
						}

						pi->tag=tag;
						pi->io_type=io_type;
						pi->op = operation;
						pi->req_size=size;
						pi->req_offset=dist->data_file_offset;
						pi->aggregate_size=dist->aggregate_size;
						pi->current_server=dist->current_server_number;
						pi->total_server=dist->total_server_number;

						si->app_index=s_pool.socket_state_list[index].app_index;
						si->data_socket=index;
						si->request_socket=index;
						si->rs = new_rs;
						struct timeval tv;
						gettimeofday(&tv, 0);

						update_receipt_time(index,tv, io_type);

						if (
								(*(static_methods[scheduler_index]->sch_enqueue))(si, pi)
								&& static_methods[scheduler_index]->sch_self_dispatch == 0
						)
						{
							struct dequeue_reason r;
							r.complete_size=pi->current_data_size;
							r.event=NEW_IO;
							r.last_app_index=si->app_index;
							r.item = NULL;
							new_rs->current_item=(*(static_methods[scheduler_index]->sch_dequeue))(r);
							gettimeofday(&tv, 0);
							update_release_time(index, tv);
							new_rs->locked = 0;// is set by the scheduler?
						}
						else if (static_methods[scheduler_index]->sch_self_dispatch==0)
						{
							//delayed, normal scheduler
							new_rs->locked = 1;// is set by the scheduler?
						}
						free(pi);
						free(dist->dist_name);
						free(dist);
					}
					else if (scheduler_on && io_type==PVFS_IO_READ )

					{
						//fprintf(stderr,"R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R R\n");

						struct dist* dist = dump_header(header2,REQUEST,s_pool.socket_state_list[index].ip);
						int this_size=dist->aggregate_size/dist->total_server_number;
						//an initial estimation for read; willl be smoothed out as time goes
						//but we wil fix the expected bytes once we've got the response.
						//thus far we think the average of aggregate size is a more accurate
						//estimation at the beginning
						new_rs->read_size=this_size;
						new_rs->read_offset=dist->data_file_offset;
						//fprintf(stderr,"READ size estimated to %i!\n",this_size);

						struct socket_info * si = (struct socket_info *)malloc(sizeof(struct socket_info));
						struct pvfs_info * pi = (struct pvfs_info *)malloc(sizeof(struct pvfs_info));
						memset(si, 0, sizeof(struct socket_info));
						memset(pi, 0, sizeof(struct pvfs_info));

						if (small==1)
						{
							pi->current_data_size=dist->small_total;
							//fprintf(stderr,"small io size is %i\n",dist->small_total);
						}
						else
						{
							pi->current_data_size=this_size;
						}

						pi->tag=tag;
						pi->req_offset=dist->data_file_offset;
						pi->op=operation;
						pi->io_type=io_type;
						pi->req_size=size;
						pi->aggregate_size=dist->aggregate_size;
						pi->current_server=dist->current_server_number;
						pi->total_server=dist->total_server_number;
						pi->strip_size=dist->stripe_size;

						si->data_socket=counter_index;
						si->request_socket=index;
						si->app_index=s_pool.socket_state_list[index].app_index;
						si->rs = new_rs;

						struct timeval tv;
						gettimeofday(&tv, 0);

						update_receipt_time(index,tv, io_type);

						if ((*(static_methods[scheduler_index]->sch_enqueue))(si, pi)
							&& static_methods[scheduler_index]->sch_self_dispatch==0)

						{
							//fprintf(stderr, "[dispatched immediately] %i\n",s_pool.socket_state_list[index].weight);
							struct dequeue_reason r;
							r.complete_size=pi->current_data_size;
							r.event=NEW_IO;
							r.last_app_index=si->app_index;
							r.item = NULL;
							new_rs->current_item=(*(static_methods[scheduler_index]->sch_dequeue))(r);
							struct timeval tv;
							gettimeofday(&tv, 0);
							update_release_time(index, tv);
							new_rs->locked = 0;

						}
						else if (static_methods[scheduler_index]->sch_self_dispatch==0)
						{
							//delayed, normal scheduler
							new_rs->locked = 1;
						}
						free(pi);
						free(dist->dist_name);
						free(dist);
					}//end read
				}//end io/smallio
				else if (scheduler_on == 1 && static_methods[scheduler_index]->sch_accept_meta == 1
						&& is_meta(operation) == 1
				)
					//meta data and getconfig stuff

				{
					//fprintf(stderr,"%s REQUEST OPERATION %s from socket %i (ip %s) tag %i\n",
					//		log_prefix, ops[operation], read_socket, s_pool.socket_state_list[index].ip, tag);
					//now we interact with the queue, and get feedback from the scheduler by checking
					//if it supports meta-operation (sch_accept_meta)

					struct meta* meta = dump_meta_header(header2,REQUEST,s_pool.socket_state_list[index].ip);

					struct socket_info * si = (struct socket_info *)malloc(sizeof(struct socket_info));
					struct pvfs_info * pi = (struct pvfs_info *)malloc(sizeof(struct pvfs_info));
					memset(si, 0, sizeof(struct socket_info));
					memset(pi, 0, sizeof(struct pvfs_info));

					pi->tag = tag;
					pi->op = operation;
					pi->req_size = size;
					pi->handle = meta->handle;
					pi->fsid = meta->fsid;
					pi->mask = meta->mask;

					si->app_index = s_pool.socket_state_list[index].app_index;
					si->request_socket = index;
					si->rs = new_rs;

					struct timeval tv;
					gettimeofday(&tv, 0);

					//update_receipt_time(index,tv, io_type);

					if (
							(*(static_methods[scheduler_index]->sch_enqueue))(si, pi)
							&&static_methods[scheduler_index]->sch_self_dispatch==0
					)
					{

						struct dequeue_reason r;
						r.complete_size = size;//just a place holder
						r.event = NEW_META;
						r.last_app_index = si->app_index;
						r.item = NULL;
						new_rs->current_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);//get_next_request();
						//struct timeval tv;
						gettimeofday(&tv, 0);
						update_release_time(index, tv);
						new_rs->locked = 0;
					}
					else if (static_methods[scheduler_index]->sch_self_dispatch==0)
					{
						new_rs->locked = 1;
					}
					free(pi);
					free(meta);
				}
				else //we still need to protect from mgmt set param message failures,
					//because the meta data service might not be able to recognize it
				{
					//guard against those that we don't understand....
					//fprintf(stderr, "Unscheduled operation %s, length %i\n", ops[operation], new_rs->buffer_size);
				}
				new_rs->config_tag = 0;
				ret=1;
			}//end pvfs header right
		}//end flow or other
	}//end bmi header right
	if (ret > 0)
	{
		update_socket_current_receive(index, tag);

	}
	return ret;
}

int check_response(int index)//peeking
{
	int eheader=BMI_HEADER_LENGTH;
	unsigned char header[eheader];
	int read_socket=s_pool.poll_list[index].fd;
	int counter_index=s_pool.socket_state_list[index].counter_socket_index;

	struct timeval peek_time;
	gettimeofday(&peek_time, 0);



	int ret=get_bmi_header(header, eheader, read_socket);
	int tag = -1;
	if (ret!=eheader)
	{
		fprintf(stderr, "error occured while getting bmi header for response on socket %i. Expected: %i, got:%i\n",read_socket,eheader, ret);
		perror("resp recv error: ");
		exit(-1);
	}
	else
	{
		//long long mnr=output_param(header, 0, 4 , "magic number",NULL,0);
		//output_param(header, 4, 4 , "mode",NULL,0);
		tag = output_param(header, 8, 8 , "tag",NULL,0);
		struct request_state * response_rs = find_request(index, tag, 1);
		struct request_state * request_rs = find_request(counter_index, tag, 1);
		if ( response_rs == NULL )
		{
			fprintf(stderr,"response rs of tag %i on index %i is returning null\n", tag, index);
			exit(-1);
		}
		struct timeval receive_time;
		gettimeofday(&receive_time, 0);
		long long size = output_param(header, 16, 8 , "size",NULL,0);
		int new_resp = 0;
		if (response_rs->job_size <= 0)//this is newly created by the request
		{
			response_rs->buffer = malloc( ( size + BMI_HEADER_LENGTH ) * sizeof( char ) );
			//fprintf(stderr," address of new buffer from server:%i on %ith socket, %i\n", (int)s_pool.socket_state_list[index].buffer, index, s_pool.socket_state_list[index].socket);
			response_rs->buffer_head = 0;
			response_rs->buffer_tail = 0;
			response_rs->buffer_size = size + BMI_HEADER_LENGTH;
			response_rs->completed_size = 0;
			response_rs->current_tag = tag;
			response_rs->last_tag = tag;
			response_rs->job_size = size + BMI_HEADER_LENGTH;
			response_rs->last_peek_time = peek_time;
			response_rs->first_receive_time = receive_time;

			new_resp = 1;
		}
		//mnr==0xcabf &&
		else if (response_rs->last_tag==tag
			&& response_rs->pvfs_io_type != PVFS_IO_WRITE )//this response is old.
		{		//pvfs data flow from server (read)

			//create a new response_rs here;
			struct request_state * new_response_rs;
			if (s_pool.socket_state_list[index].incomplete_mode ==0)
			{
				new_response_rs = add_request_to_socket(index, tag);
				new_response_rs->buffer = malloc( ( size + BMI_HEADER_LENGTH ) * sizeof( char ) );
				//fprintf(stderr,"new flow from item tag %i\n", tag);
				new_response_rs->buffer_head = 0;
				new_response_rs->buffer_tail = 0;
				new_response_rs->buffer_size = size + BMI_HEADER_LENGTH;
				new_response_rs->job_size = size + BMI_HEADER_LENGTH;
				new_response_rs->completed_size = 0;
				new_response_rs->last_tag = tag;
				new_response_rs->current_tag = tag;
				new_response_rs->last_flow = 0;
				new_response_rs->config_tag = 0;
				new_response_rs->op = PVFS_DATA_FLOW;
				new_response_rs->pvfs_io_type = response_rs->original_request->pvfs_io_type;
				new_response_rs->locked = 0;
				new_response_rs->original_request = response_rs->original_request;
				new_response_rs->meta_response = 0;
				new_response_rs->first_receive_time = receive_time;
				new_response_rs->last_peek_time = peek_time;
				//fprintf(stderr, "new response on %i, tag %i\n", index, tag);
			}
			else
			{
				new_response_rs = find_last_request(index, tag);
				if (new_response_rs == NULL)
				{
					fprintf(stderr,"incomplete old response is null on %i, tag %i\n", index, tag);
					exit(-1);
				}
				else
				{
					fprintf(stderr,"old response not found on %i, tag %i\n", index, tag);
				}
			}
			response_rs = new_response_rs;
			//add to the list

			ret=1;
		}
		else if (request_rs!=NULL && //which means, old response has a job_size >=0 so it falls through here
				request_rs->current_tag==tag &&
				request_rs->op == PVFS_SERV_IO)
		{
			//a write completion
			struct request_state * new_response_rs = add_request_to_socket(index, tag);
			new_response_rs->buffer = malloc( ( size + BMI_HEADER_LENGTH ) * sizeof( char ) );
			//fprintf(stderr," address of new buffer from server:%i on %ith socket, %i\n", (int)s_pool.socket_state_list[index].buffer, index, s_pool.socket_state_list[index].socket);
			new_response_rs->buffer_head = 0;
			new_response_rs->buffer_tail = 0;
			new_response_rs->buffer_size = size + BMI_HEADER_LENGTH;
			new_response_rs->completed_size = 0;
			new_response_rs->current_tag = tag;
			new_response_rs->last_tag = tag;
			new_response_rs->job_size = size + BMI_HEADER_LENGTH;
			new_response_rs->meta_response = 0;
			new_response_rs->first_receive_time = receive_time;
			new_response_rs->last_peek_time = peek_time;
			response_rs = new_response_rs;
			//add to the list
			new_resp = 1;
			ret = 1;
		}

		else
		{
			fprintf(stderr,"something's wrong, the existing same-tag server-side message is neither a read flow nor a new response\n");
			fprintf(stderr,"job size %i last tag %i current tag %i io type %i\n",
					response_rs->job_size, response_rs->last_tag,
					response_rs->current_tag, response_rs->pvfs_io_type);
			fprintf(stderr,"request_rs %p", request_rs);
			if (request_rs!=NULL)
			{
				fprintf(stderr," tag %i, op %s\n", request_rs->current_tag, ops[request_rs->op]);
			}
			else
			{
				fprintf(stderr,"\n");

			}
			exit(-1);
		}
         //it's not a read data flow; it's a response
		if ( new_resp == 1 )//we don't process any read flow explicitly because it's a consequence of an admitted request
		{
			int eheader=size + BMI_HEADER_LENGTH;
			unsigned char header2[eheader];

			int ret=recv(read_socket, header2, eheader, MSG_PEEK|DEFAULT_MSG_FLAGS|MSG_DONTWAIT);
			if (ret!=eheader)
			{
				fprintf(stderr, "Error getting pvfs header for response. Expected: %i, got:%i\n",eheader, ret);
				ret = -3;//get_bmi_header already occupied 0, -1, and -2
				s_pool.socket_state_list[index].incomplete_mode = 1;
			}
			else
			{
				s_pool.socket_state_list[index].incomplete_mode = 0;
				long long operation = output_param(header2, 32, 4, "pvfs_operation", ops,40);
				//fprintf(stderr,"SERVER RESPONSE OP:%s(%i)\n", ops[operation], operation);
				response_rs->op = operation;
				//fprintf(stderr,"RESPONSE OPERATION %s from %i\n", ops[operation], read_socket);
				struct dist * dist = NULL;
				if (operation==PVFS_SERV_IO ||
					operation==PVFS_SERV_WRITE_COMPLETION ||
					operation==PVFS_SERV_SMALL_IO)
				{
					dist = dump_header(header2,RESPONSE,s_pool.socket_state_list[index].ip);
					//fprintf(stderr,"dist pointer is %i\n", dist);
				}
				else
				{
					/*switch (operation){
					case PVFS_SERV_GETATTR:
					case PVFS_SERV_READDIR:
					case PVFS_SERV_LISTATTR:
					case PVFS_SERV_LOOKUP_PATH:
						//fprintf(stderr, "%s incurring more cost on response %i bytes\n", ops[operation], size);
						break;
					default:
						//fprintf(stderr,"%s has minimum cost on response\n", ops[operation]);
						break;
					}*/
					response_rs->meta_response = 1;
				}
				if (operation==PVFS_SERV_GETCONFIG)
				{
					ret=1;

					response_rs->config_tag=1;
					fprintf(stderr,"GET_CONFIG received feedback\n");
				}
				else
				{
					//peek using only 24 bytes above
					long long operation = output_param(header2, 32, 4, "pvfs_operation", ops,40);
					response_rs->config_tag=0;
					ret=1;

					if (operation==PVFS_SERV_IO) //scheduler check is in the branch body
					{
						long long returned_size = output_param(header2, 36, 4, "IO request returned is ", NULL,0);
						//fprintf(stderr,"item %i IO response status: %i", tag ,returned_size);
						returned_size = *(long long *)(header2+40);
						//fprintf(stderr," bstream size: %i\n", returned_size);
						//if it returned zero...that would mean this I/O complete..work like write_completion....
						//but we may need to adjust last_finish tag forward a little...

						if (response_rs->pvfs_io_type!=PVFS_IO_READ)
						{
							//fprintf(stderr,"the response is a write %i\n", response_rs->pvfs_io_type);
						}
						if (response_rs->pvfs_io_type==PVFS_IO_READ && scheduler_on == 1)
						{
							//fprintf(stderr,"total resp size is %i returned size is %llu, IO type is %i (operation returns %lli)\n",eheader,returned_size,s_pool.socket_state_list[index].pvfs_io_type,operation);
							int counter_index=s_pool.socket_state_list[index].counter_socket_index;
							//if it returns a value different than this_data_size, then modify to the returned value.
							unsigned long long current_size, original_read, original_offset;

							struct request_state * request_rs = find_request(counter_index, tag,1);
							if (request_rs == NULL)
							{
								fprintf(stderr,"could not find request state corresponding to a response\n");
								exit(-1);
							}
							original_read = request_rs->read_size;
							original_offset = request_rs->read_offset;

							current_size = (*(static_methods[scheduler_index]->sch_current_size))(request_rs,returned_size);
							/*returned size in the response indicates the size of the data file!
							 * current size reflects this server's share. //only implemented in SFQD
							 * the task_size of an item should be changed inside
							 * */
							//fprintf(stderr,"adjusting size\n");
							if (current_size <= 0)
									//eof returns -1;
							{
								//fprintf(stderr,"warning!!!, passed EOF\n");
								//fprintf(stderr,"EOF already, dispatching new items......\n");

								response_rs->last_flow = 1;

								(*(static_methods[scheduler_index]->sch_current_size))(request_rs,0);
								struct complete_message complete;
								complete.complete_size=0;
								complete.current_item= request_rs->current_item;

								(*(static_methods[scheduler_index]->sch_update_on_request_completion))
										((void*)&complete);
								request_rs->check_response_completion = 1;
								request_rs->last_completion = ret;


								int app_index = s_pool.socket_state_list[counter_index].app_index;
								app_stats[app_index].completed_requests+=1;


								if (first_receive==0)
								{
									passed_completions++;
									app_stats[app_index].app_exist=app_stats[app_index].app_exist+1;

									fprintf(stderr,"client %s completed++ for app %i\n",s_pool.socket_state_list[counter_index].ip, app_index+1);
									check_all_app_stat();
									//fprintf(stderr,"%i completion passed, not starting counter yet\n", passed_completions);
								}

								if (timer_stop || !first_receive){}
									//timer stop means dump_stat is called, so we're not scheduling non-work-conserving any more
									//first_receive means a number of applications has started to make the system more meaningful

								struct dequeue_reason r;//reason is used for non-work-conserving purposes
								r.complete_size=original_read;
								r.event=COMPLETE_IO;
								r.last_app_index=app_index;
								r.item=request_rs->current_item;
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
									struct request_state *new_dispatched = new_item->socket_data->rs;
									new_dispatched->current_item = new_item;
									assert(new_dispatched->locked!=0);
									new_dispatched->locked = 0;
								}
							}//end eof
						}//end read
					}//end serve_io


					if (scheduler_on
							&& (operation==PVFS_SERV_WRITE_COMPLETION || operation==PVFS_SERV_SMALL_IO)
							&& static_methods[scheduler_index]->sch_self_dispatch==0)
					{
						int counter_index=s_pool.socket_state_list[index].counter_socket_index;

						struct timeval tv;
					    gettimeofday(&tv, 0);
					    int write_resp_time = get_response_time(counter_index, tv, PVFS_IO_WRITE);

					    int app_index = s_pool.socket_state_list[counter_index].app_index;
					    finished[app_index]--;
					    completerecv[app_index]++;
					    response_rs->op= PVFS_SERV_WRITE_COMPLETION;
					    update_history(app_index, counter_index, 0, ret, write_resp_time, PVFS_IO_WRITE);

					    struct request_state * request_rs = find_request(counter_index, tag, 1);
						if (request_rs == NULL)
						{
							fprintf(stderr,"could not find request state corresponding to a response\n");
							exit(-1);
						}
						struct generic_queue_item* current_item = request_rs->current_item;

						app_stats[app_index].completed_requests+=1;
						if (first_receive==0)
						{
							passed_completions++;
							app_stats[app_index].app_exist=app_stats[app_index].app_exist+1;
							check_all_app_stat();

							//fprintf(stderr,"%i completion passed, not starting counter yet\n", passed_completions);
						}

						if (!timer_stop && first_receive)
						{
							//fprintf(stderr,"app %i 1increasing\n", app_index);
							(*(static_methods[scheduler_index]->sch_add_ttl_throughput))(dist->aggregate_size, app_index);
							if (!static_methods[scheduler_index]->work_conserving)
							{

								int s;
								//here we try to fix the diff change based dispatch first
								for (s=0;s<num_apps;s++)
								{
									int old_diff=app_stats[s].diff;
									app_stats[s].diff=(*(static_methods[scheduler_index]->sch_calculate_diff))(s);
											//app_stats[s].app_throughput-total_throughput*app_stats[s].app_weight/total_weight;
									if (old_diff>10240 && app_stats[s].diff<=10240)
									{

										fprintf(stderr,"triggering non-work-conservingness.1..\n");
										struct dequeue_reason r;
										r.complete_size=0;
										r.event=DIFF_CHANGE;
										r.last_app_index=s;
										//r.item=s_pool.socket_state_list[s].current_item;
										//we need to find as we dequeue
										struct generic_queue_item* new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
										while (new_item!=NULL)
										{
											struct request_state *new_dispatched = new_item->socket_data->rs;
											assert( new_dispatched->locked != 0 );
											new_dispatched->locked = 0;
											new_dispatched->current_item = new_item;
											new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
										}
									}
								}
							}
						}
						struct dequeue_reason r;
						r.complete_size=dist->aggregate_size;
						r.event=COMPLETE_IO;
						//this complete_io event of app will not overlap from the previous diff_change search,

						r.last_app_index=app_index;


						if (request_rs == NULL)
						{
							fprintf(stderr,"could not find request state corresponding to a response\n");
							exit(-1);
						}

						r.item=current_item;
						//fprintf(stderr,"dispatching because of write completion\n");
						struct generic_queue_item* new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
						if (new_item==NULL)
						{
						}
						else
						{
                    	    struct timeval tv;
                    	    gettimeofday(&tv, 0);
                        	update_release_time(new_item->socket_data->unlock_index, tv);
							struct request_state *new_dispatched = new_item->socket_data->rs;
							fprintf(stderr,"dispatching item %li\n", new_item->item_id);
							assert(new_dispatched->locked!=0);
							new_dispatched->locked = 0;
							new_dispatched->current_item = new_item;
						}
					}
					else if (scheduler_on && (operation==PVFS_SERV_SMALL_IO))//self dispatch == 1
						/* write_completion triggers the self_dispatch = 1's inside scheduling mechanism*/
					{
						//this is for two-level schedulers

						int counter_index=s_pool.socket_state_list[index].counter_socket_index;
					    struct request_state * request_rs = find_request(counter_index, tag,1);
						if (request_rs == NULL)
						{
							fprintf(stderr,"could not find request state corresponding to a response\n");
							exit(-1);
						}
						struct generic_queue_item* current_item= request_rs->current_item;
						struct complete_message cmsg;
						cmsg.complete_size=-1;
                        cmsg.current_item=current_item;
						int ret = (*(static_methods[scheduler_index]->sch_update_on_request_completion))((void*)&cmsg);

					}
					if ( scheduler_on && is_meta( operation )
							&&  static_methods[scheduler_index]->sch_self_dispatch == 0 )
						/*this last branch processes the responses of meta data operation
						 * -eager mode
						 * -response message comes with header and is part of the response.
						 * so we just dispatch upon receipt of a response from meta data
						 * */
					{
						//like getting a write completion, you should also operate on the queue and dispatch the next
						//fprintf(stderr, "hola, I've completed my meta! %i %s, dispatch?\n", operation, ops[operation]);
						//this is supposed to work like it received a write_completion response from the server.
						//assuming that the scheduling is work-conserving and proxy-dispatching, the next item
						//will have to be extracted and dispatched
						int counter_index=s_pool.socket_state_list[index].counter_socket_index;
						int app_index=s_pool.socket_state_list[counter_index].app_index;
					    struct request_state * request_rs = find_request(counter_index, tag, 1);
						if (request_rs == NULL)
						{
							fprintf(stderr,"could not find request state corresponding to a response\n");
							exit(-1);
						}
						app_stats[app_index].completed_requests+=1;
						struct generic_queue_item* current_item= request_rs->current_item;
						struct complete_message cmsg;
						cmsg.complete_size=-1;
						//all scheduler's complete_size condition should include -1 case where it should be directly skipped
                        cmsg.current_item=current_item;
						int ret = (*(static_methods[scheduler_index]->sch_update_on_request_completion))((void*)&cmsg);
						//right now, for meta-data operations, this is only recognized and implemented in sfqd_full scheduler
						//the next one to recognize this is dsfq_full
						struct dequeue_reason r;
						r.item = current_item;
						r.event = COMPLETE_IO;
						//nothing's in it yet
						struct generic_queue_item* new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
						if (new_item==NULL)
						{
						}
						else
						{
                    	    struct timeval tv;
                    	    gettimeofday(&tv, 0);
                        	update_release_time(new_item->socket_data->unlock_index, tv);
							struct request_state *new_dispatched = new_item->socket_data->rs;
							new_dispatched->current_item = new_item;
							assert( new_dispatched->locked != 0 );
							new_dispatched->locked = 0;
							//s_pool.socket_state_list[new_item->socket_data->unlock_index].locked=0;
						}
					}
					else if (scheduler_on && !is_meta(operation) && !is_IO(operation))
					{
						//fprintf(stderr, "seeing a non-io, non-meta op : %s, nothing is done on the socket\n", ops[operation]);
					}


				}//end normal responses (instead of a get_config)
				if (dist!=NULL)
					free(dist);
			}//end peeking response header
		}//end response (instead of data flow)
	}//end successful bmi header read
	if (ret > 0)
	{
		update_socket_current_receive(index, tag);

	}
	return ret;
}

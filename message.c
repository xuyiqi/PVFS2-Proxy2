/*
 * message.c
 *
 *  Created on: Oct 4, 2010
 *      Author: yiqi
 */

#include "message.h"
#include "scheduler_main.h"
#include "config.h"
int PVFS_IO_READ = 1;
int PVFS_IO_WRITE = 2;
int passed_completions=0;
extern int* completed_requests;
extern char* log_prefix;
long long total_throughput=0;
extern int total_weight;
int get_header(char * header, int eheader, int read_socket)
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
/*	else if (ret == -1 && errno == EWOULDBLOCK)
	{

	}
	else if (ret == -1 && errno == EINTR || errno==EAGAIN)
	{
		if ((count++)<1)
		goto peek_start;
	}*/
	if (ret < 0)
	{
		//failure to get header
		perror(" socket error! ");
		ret=-3;
	}
	else if (ret<eheader)
	{
		//didn't get header in a limit number of retries
		fprintf(stderr," only got %i!!!\n",ret);
		ret=-2;

	}

	return ret;
}



/**
 * Reads the BMI header of incoming socket data, and determines if there is GET_CONFIG in there.
 * If so, the response socket will wait for chance to change the contents when
 * 1. enough space is left to receive the config buffer (ready_to_receive==1)
 * 2. whole buffer has been received (job_size==complete_size)
 * \param counter_index: the counter part socket number of the current one being checked
 * \param read_socket:   the socket number of currently checked socket
 * \return -1 if error occurs.
 * 1 if not a get_conf response
 * 0 if it is a get_conf response
 *
 * */
int check_response(int index)//peeking
{
	int eheader=24;
	unsigned char header[eheader];
	int read_socket=s_pool.poll_list[index].fd;
	int ret=get_header(header, eheader, read_socket);
	if (ret!=eheader)
	{
		fprintf(stderr, "error occured while getting bmi header for response on socket %i. Expected: %i, got:%i\n",read_socket,eheader, ret);
		perror("resp recv error: ");


	}
	else
	{
		//try to extract header
		//output_stream(header, eheader);

		//long long mnr=output_param(header, 0, 4 , "magic number",NULL,0);
		//Dprintf(D_CACHE,"mnr: %x\n",mnr);
		//output_param(header, 4, 4 , "mode",NULL,0);
		long long tag = output_param(header, 8, 8 , "tag",NULL,0);
		//s_pool.socket_state_list[index].last_tag = s_pool.socket_state_list[index].current_tag;
		s_pool.socket_state_list[index].current_tag=tag;
		//IO/response
		//tag record

		//record tag
		long long size=output_param(header, 16, 8 , "size",NULL,0);
		s_pool.socket_state_list[index].job_size=size+24;
		//fprintf(stderr,"malloc response of %lli bytes\n", size+24);
		s_pool.socket_state_list[index].buffer=malloc((size+24)*sizeof(char));

		//fprintf(stderr," address of new buffer from server:%i on %ith socket, %i\n", (int)s_pool.socket_state_list[index].buffer, index, s_pool.socket_state_list[index].socket);

		s_pool.socket_state_list[index].buffer_head=0;
		s_pool.socket_state_list[index].buffer_tail=0;
		s_pool.socket_state_list[index].buffer_size=size+24;


		//Dprintf(D_CACHE,"RAW: %lli,%lli,test1:%i,test2:%i\n",mnr,operation,mnr==0xcabf,operation==PVFS_SERV_GETCONFIG );
		//mnr==0xcabf &&
		if (
				s_pool.socket_state_list[index].last_tag==tag
				/*is_tag_used(s_pool.socket_state_list[index].ip,
				s_pool.socket_state_list[index].port,
				tag)*/
				&& s_pool.socket_state_list[index].pvfs_io_type !=PVFS_IO_WRITE)
		{
			//pvfs data flow from server (read)
			//otherwise it might be a write completion
			//fprintf(stderr,"both tags are now %i\n",tag);
			s_pool.socket_state_list[index].ready_to_receive=1;
			s_pool.socket_state_list[index].config_tag=0;
			s_pool.socket_state_list[index].op=PVFS_DATA_FLOW;
			ret=1;

			s_pool.socket_state_list[index].needs_output=0;
		}
        else //it's not a read data flow; it's a response
		{
			s_pool.socket_state_list[index].needs_output=1;
			int eheader=size+24;
			unsigned char header2[eheader];

			int ret=get_header(header2, eheader, read_socket);
			if (ret!=eheader)
			{
				Dprintf(D_CACHE, "Error getting pvfs header for response. Expected: %i, got:%i\n",eheader, ret);
				ret=-1;
			}

			else
			{

				long long operation = output_param(header2, 32, 4, "pvfs_operation", ops,40);

				struct dist * dist;
				if (operation==PVFS_SERV_IO ||
						operation==PVFS_SERV_WRITE_COMPLETION || operation==PVFS_SERV_SMALL_IO)
				{
					dist=	dump_header(header2,RESPONSE,s_pool.socket_state_list[index].ip);

				}
				if (operation==PVFS_SERV_GETCONFIG)
				{
					ret=0;
					s_pool.socket_state_list[index].config_tag=1;

					s_pool.socket_state_list[index].ready_to_receive=1;

				}

				else
				{
					//peek using only 24 bytes above
					//peek again using more fields
					//look at IO type->read or write
					//if io is write, record write and
					long long operation = output_param(header2, 32, 4, "pvfs_operation", ops,40);
					s_pool.socket_state_list[index].config_tag=0;
					ret=1;

					if (operation==PVFS_SERV_IO)
					{
						//print_delay(s_pool.socket_state_list[index].counter_index, "IO RESPONSE");
						//record_tag(s_pool.socket_state_list[index].ip, s_pool.socket_state_list[index].port, tag);//record a response of pvfs_serv_io for later flow data processing
						s_pool.socket_state_list[index].last_tag=tag;

						unsigned long long returned_size = output_param(header2, 36, 4, "IO request returned is ", NULL,0);

						returned_size = *(long long *)(header2+40);

						//if it returned zero...that would mean this I/O complete..work like write_completion....
						//but we may need to adjust last_finish tag forward a little...
						if (s_pool.socket_state_list[index].pvfs_io_type==PVFS_IO_READ)
						{
							//fprintf(stderr,"total resp size is %i returned size is %llu, IO type is %i (operation returns %lli)\n",eheader,returned_size,s_pool.socket_state_list[index].pvfs_io_type,operation);

							int counter_index=s_pool.socket_state_list[index].counter_index;
							//if it returns a value different than this_data_size, then modify to the returned value.

							unsigned long long current_size, original_read, original_offset;
							if (scheduler_on==1)
							{
								original_read = s_pool.socket_state_list[counter_index].read_size;
								original_offset = s_pool.socket_state_list[counter_index].read_offset;

								current_size = (*(static_methods[scheduler_index]->sch_current_size))(counter_index,returned_size);
								/*returned size in the response indicates the size of the data file!
								 * current size reflects this server's share. //only implemented in SFQD
								 * the task_size of an item should be changed inside
								 * */

								if (current_size >0)
								{

								}
								else	//eof returns -1;
								{
									fprintf(stderr,"warning!!!, passed EOF\n");
									fprintf(stderr,"EOF already, dispatching new items......\n");
									(*(static_methods[scheduler_index]->sch_current_size))(counter_index,0);

									s_pool.socket_state_list[counter_index].check_response_completion=1;
									s_pool.socket_state_list[counter_index].last_completion=ret;
									//fprintf(stderr,"[READ COMPLETE]\n");

									int app_index = s_pool.socket_state_list[counter_index].app_index;
									app_stats[app_index].completed_requests+=1;
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
														fprintf(stderr,"%s: ANot counting, not all apps exist yet: app %i has only %i<%i items\n", log_prefix, k+1, app_stats[k].app_exist, MESSAGE_START_THRESHOLD);
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
									struct dequeue_reason r;//reason is used for non-work-conserving purposes
									r.complete_size=original_read;
									r.event=COMPLETE_IO;
									r.last_app_index=app_index;
									r.item=s_pool.socket_state_list[counter_index].current_item;
									struct generic_queue_item* new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
									(*(static_methods[scheduler_index]->sch_get_scheduler_info))();
									if (new_item==NULL)
									{
											fprintf(stderr, "no more jobs found, depth decreased...");
									}
									else
									{
										struct timeval tv;
										gettimeofday(&tv, 0);
										update_release_time(new_item->socket_data->unlock_index, tv);

										s_pool.socket_state_list[new_item->socket_data->unlock_index].locked=0;
									}
									//////////////////////////////////////////////
								}

							}
						}

					}

					if (scheduler_on && (operation==PVFS_SERV_WRITE_COMPLETION || operation==PVFS_SERV_SMALL_IO)
							&& static_methods[scheduler_index]->sch_self_dispatch==0)
					{
						//print_delay(s_pool.socket_state_list[index].counter_index, "WRITE COMPLETION");
						/*cost*/

						//update write response time at this moment, pretty much like print_delay.
						//put code in cost_model_history.c as a function

						int counter_index=s_pool.socket_state_list[index].counter_index;

						struct timeval tv;
					    gettimeofday(&tv, 0);
					    int write_resp_time = get_response_time(counter_index, tv, PVFS_IO_WRITE);
					    //update to apphistory
					    int app_index = s_pool.socket_state_list[counter_index].app_index;
					    update_history(app_index, counter_index, 0, ret, write_resp_time, PVFS_IO_WRITE);
						/*cost*/
						//write completion is the indication of server write completion

						struct generic_queue_item* current_item= s_pool.socket_state_list[counter_index].current_item;

						app_stats[app_index].completed_requests+=1;
						if (first_receive==0)
						{
							passed_completions++;
							app_stats[app_index].app_exist=app_stats[app_index].app_exist+1;
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
							//fprintf(stderr,"%i completion passed, not starting counter yet\n", passed_completions);
						}

						if (timer_stop || !first_receive)
						{
							//fprintf(stderr,"Timer stopped or not started\n");
						}
						else
						{

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
										struct dequeue_reason r;
										r.complete_size=0;
										r.event=DIFF_CHANGE;
										r.last_app_index=s;
										//r.item=s_pool.socket_state_list[s].current_item;
										//we need to find as we dequeue

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
						r.complete_size=dist->aggregate_size;
						r.event=COMPLETE_IO;//this complete_io event of app will not overlap from the previous diff_change search,

						r.last_app_index=app_index;
						r.item=s_pool.socket_state_list[counter_index].current_item;
						struct generic_queue_item* new_item = (*(static_methods[scheduler_index]->sch_dequeue))(r);
						if (new_item==NULL)
						{
						}
						else
						{

                    	    struct timeval tv;
                    	    gettimeofday(&tv, 0);
                        	update_release_time(new_item->socket_data->unlock_index, tv);

							s_pool.socket_state_list[new_item->socket_data->unlock_index].locked=0;
						}
					}
					s_pool.socket_state_list[index].ready_to_receive=1;
				}
			}
		}
	}
	return ret;
}

int get_io_type(char * buffer, int size,int small)
{
	return dump_header2(buffer,small);
}

int check_request(int index)
{
	int eheader=24;
	unsigned char header[eheader];
	int read_socket=s_pool.poll_list[index].fd;

	int ret = get_header(header, eheader, read_socket);
	if (ret!=eheader)
	{
		fprintf(stderr,"Error getting bmi header for request. Expected: %i, got:%i\n",eheader, ret);
		perror("req error");
		ret=-1;
	}
	else
	{
		//try to extract header
		//output_stream(header, eheader);

		//long long mnr=output_param(header, 0, 4 , "magic number",NULL,0);
		//output_param(header, 4, 4 , "mode",NULL,0);
		long long tag=output_param(header, 8, 8 , "tag",NULL,0);
		//if this tag is already used by a response, then the subsequent messages are considered flow.

		//if (s_pool.socket_state_list[index].current_tag !=tag)

		//s_pool.socket_state_list[index].last_tag=s_pool.socket_state_list[index].current_tag;
		s_pool.socket_state_list[index].current_tag=tag;

		long long size=output_param(header, 16, 8 , "size",NULL,0);
		s_pool.socket_state_list[index].job_size=size+24;
		int counter_index=s_pool.socket_state_list[index].counter_index;


		if (s_pool.socket_state_list[index].has_block_item==1)
		{

			//when it is dispatched. has_block_item is 1
			//don't try to allocate memory again.

		}
		else
		{
			//fprintf(stderr,"malloc request of %lli bytes on socket %i \n", size+24,s_pool.socket_state_list[index].socket);
			s_pool.socket_state_list[index].buffer=malloc((size+24)*sizeof(char));
			//fprintf(stderr," address of new buffer from client:%i on %ith socket, %i\n", (int)s_pool.socket_state_list[index].buffer, index, s_pool.socket_state_list[index].socket);
			s_pool.socket_state_list[index].buffer_head=0;
			s_pool.socket_state_list[index].buffer_tail=0;
			s_pool.socket_state_list[index].buffer_size=size+24;
			//it means as soon as the request comes, the buffer is allocated.
			//if queueing occurs, the next time it is here, has_block_item == 1 is true
		}
		//fprintf(stderr,"tag %lli and last tag %lli\n", tag , s_pool.socket_state_list[counter_index].last_tag);
		if (tag==s_pool.socket_state_list[counter_index].last_tag)//is_tag_used(s_pool.socket_state_list[counter_index].ip, s_pool.socket_state_list[counter_index].port, tag))
		{
			//pvfs data flow from client
			//print_delay(index, "DATA FLOW");
			//fprintf(stderr,"tag and last tag are both %i\n", tag);
			s_pool.socket_state_list[index].ready_to_receive=1;
			s_pool.socket_state_list[index].config_tag=0;
			ret=1;//passed
			s_pool.socket_state_list[index].op=PVFS_DATA_FLOW;

			//fprintf(stderr,"dumping flow from client......content size is %lli, tag is %lli, ip is %s:%i\n",size,tag,s_pool.socket_state_list[index].ip, s_pool.socket_state_list[index].port);
			//Dprintf(D_CALL,"dumping flow from client...content size is %lli, tag is %lli, ip is %s:%i\n",size,tag,s_pool.socket_state_list[index].ip,s_pool.socket_state_list[index].port);

			s_pool.socket_state_list[index].needs_output=0;
			//lock
		}
		else
		{

			s_pool.socket_state_list[counter_index].op=-1;//critical for read!
			s_pool.socket_state_list[index].op=-1;//critical for write!
			s_pool.socket_state_list[index].needs_output=1;
			eheader=size+24;//size of the message, does not include data flow possibility, so it's a few bytes at most
			unsigned char header2[eheader];
		peek_start2:

			ret=recv(read_socket, header2, eheader,MSG_PEEK|DEFAULT_MSG_FLAGS|MSG_DONTWAIT);
			if(ret !=eheader)
			{
				fprintf(stderr, "Error getting header of pvfs for request. Expected: %i, got:%i\n",eheader, ret);
				ret=-1;
			}

			else
			{

				long long operation = output_param(header2, 32, 4, "pvfs_operation", ops,40);

				//Dprintf(D_CACHE,"RAW: %lli,%lli,test1:%i,test2:%i\n",mnr,operation,mnr==0xcabf,operation==PVFS_SERV_GETCONFIG );
				//mnr==0xcabf &&
				s_pool.socket_state_list[index].op=operation;
				//fprintf(stderr,"REQUEST OPERATION %s\n", ops[operation]);

				//fprintf(stderr,"operation is %lli\n",operation);
				//fprintf(stderr,"dumping contents from %s...%s, %s:%i, tag %i\n",directions[s_pool.socket_state_list[index].source],
				//		ops[operation], s_pool.socket_state_list[index].ip,s_pool.socket_state_list[index].port,tag );
				if (operation==PVFS_SERV_IO || operation == PVFS_SERV_SMALL_IO)
				{
					//fprintf(stderr,"=======msg:%s,%s,%i,%i======\n", ops[operation],s_pool.socket_state_list[index].ip,s_pool.socket_state_list[index].port,tag );
					//Dprintf(D_CACHE,"=======msg:%s,%s,%i,%i======\n", ops[operation],s_pool.socket_state_list[index].ip,s_pool.socket_state_list[index].port,tag );

					int io_type;
					int small=0;
					if (operation==PVFS_SERV_SMALL_IO)
					{
						small=1;
					}
					io_type=get_io_type(header2,eheader,small);
					s_pool.socket_state_list[counter_index].pvfs_io_type=io_type;

					if (scheduler_on && (io_type==PVFS_IO_WRITE || small==1))/*@do we need to differentiate read and write?@*/
					{							/*@
					 * we can combine these two branches if we don't
					 *
					 * @*/

						if (s_pool.socket_state_list[index].has_block_item==1)
						{

							s_pool.socket_state_list[index].has_block_item=0;
							//prevent the poll from adding to queue twice.
							//this time the unblocked socket is performing I/O whatever
							//fprintf(stderr, "previously blocked request, passing\n");
						}
						else
						{
							//print_delay(index, "IO REQUEST");
							struct dist* dist = dump_header(header2,REQUEST,s_pool.socket_state_list[index].ip);
							//Dprintf(D_CACHE,"io request size is %i\n",eheader);

							int this_size=logical_to_physical_size_dparam(dist);

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
							pi->req_size=size;
							pi->req_offset=dist->data_file_offset;
							pi->aggregate_size=dist->aggregate_size;
							pi->current_server=dist->current_server_number;
							pi->total_server=dist->total_server_number;

							si->app_index=s_pool.socket_state_list[index].app_index;
							si->data_socket=index;
							si->request_socket=index;
							struct timeval tv;
	                  	    gettimeofday(&tv, 0);

							update_receipt_time(index,tv, io_type);

							if ((*(static_methods[scheduler_index]->sch_enqueue))(si, pi)
									&& static_methods[scheduler_index]->sch_self_dispatch==0)
							{
								//Dprintf(D_CACHE, "[dispatched immediately] %i\n",s_pool.socket_state_list[index].weight);
								//Dprintf(D_CACHE,"[CLOCK] virtual time is updated to %i\n",virtual_time);
								struct dequeue_reason r;
								r.complete_size=pi->current_data_size;
								r.event=NEW_IO;
								r.last_app_index=si->app_index;
								s_pool.socket_state_list[index].current_item=(*(static_methods[scheduler_index]->sch_dequeue))(r);//get_next_request();
								//struct timeval tv;
		                  	    gettimeofday(&tv, 0);
		                       	update_release_time(index, tv);

								//assert(s_pool.socket_state_list[index].current_item!=NULL);
								//fprintf(stderr,"index %i has current item %i\n",index, s_pool.socket_state_list[index].current_item);
								//fprintf(stderr,"current:%i inserted:%i\n",s_pool.socket_state_list[r_socket_index].current_item, generic_item);
								//assert(s_pool.socket_state_list[index].current_item==generic_item);
							}
							else if (static_methods[scheduler_index]->sch_self_dispatch==0)
							{
								//fprintf(stderr, "[[[[[[[[[[[[[[[[[[[[service delayed]]]]]]]]]]]]]]]]]]]]\n");
	//								Dprintf(D_CACHE, "current item is %s:%i, tag %i\n",
	//									current_item->data_ip, current_item->data_port, current_item->socket_tag);
								//if (pthread_mutex_lock (&counter_mutex)!=0)
								{
									//Dprintf(D_CACHE, "error locking when getting counter\n");
								}


								s_pool.socket_state_list[index].locked=1;
								s_pool.socket_state_list[index].has_block_item=1;
								//if (pthread_mutex_unlock (&counter_mutex)!=0)
								{
								//	Dprintf(D_CACHE, "error locking when getting counter\n");
								}
							}
						}

					}
					else if (scheduler_on && io_type==PVFS_IO_READ )

					{
						if (s_pool.socket_state_list[index].has_block_item==1)
						{

							s_pool.socket_state_list[index].has_block_item=0;
							//prevent the poll from adding to queue twice.
							//Dprintf(D_CACHE, "previously blocked request, passing\n");
							//in next recv, it should really receive something
						}
						else
						{
							struct dist* dist = dump_header(header2,REQUEST,s_pool.socket_state_list[index].ip);
							//fprintf(stderr,"Read requested %lli bytes stripped by %i\n",dist->aggregate_size,dist->stripe_size);
							int this_size=dist->aggregate_size/dist->total_server_number;
							//an initial estimation for read; willl be smoothed out as time goes
							//but we wil fix the expected bytes once we've got the response.
							//thus far we think the average of aggregate size is a more accurate
							//estimation at the beginning
							s_pool.socket_state_list[index].read_size=this_size;
							s_pool.socket_state_list[index].read_offset=dist->data_file_offset;

							//fprintf(stderr,"READ size estimated to %i!\n",this_size);


							/*@
							 * we have a request for read
							 *
							 * @*/


							int counter_index=s_pool.socket_state_list[index].counter_index;


							//if (add_request(index, counter_index,this_size ,tag, io_type, NULL, size))
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
							pi->io_type=io_type;
							pi->req_size=size;
							pi->aggregate_size=dist->aggregate_size;
							pi->current_server=dist->current_server_number;
							pi->total_server=dist->total_server_number;
							pi->strip_size=dist->stripe_size;

							si->data_socket=counter_index;
							si->request_socket=index;
							si->app_index=s_pool.socket_state_list[index].app_index;

							if ((*(static_methods[scheduler_index]->sch_enqueue))(si, pi))


							{
								//fprintf(stderr, "[dispatched immediately] %i\n",s_pool.socket_state_list[index].weight);
								struct dequeue_reason r;
								r.complete_size=pi->current_data_size;
								r.event=NEW_IO;
								r.item=s_pool.socket_state_list[index].current_item;
								r.last_app_index=si->app_index;
								s_pool.socket_state_list[index].current_item=(*(static_methods[scheduler_index]->sch_dequeue))(r);//get_next_request();
								//assert(s_pool.socket_state_list[index].current_item!=NULL);
								//fprintf(stderr,"index %i has current item %i\n",index, s_pool.socket_state_list[index].current_item);
								//Dprintf(D_CACHE,"[CLOCK] virtual time is updated to %i\n",virtual_time);
								struct timeval tv;
		                  	    gettimeofday(&tv, 0);
		                       	update_release_time(index, tv);


							}
							else
							{
								//fprintf(stderr, "[service delayed] %i\n",s_pool.socket_state_list[index].weight);
								s_pool.socket_state_list[index].locked=1;
								s_pool.socket_state_list[index].has_block_item=1;
								//fprintf(stderr,"READ IO is blocked on %i\n",index);
							}
						}
					}


				}
				s_pool.socket_state_list[index].config_tag=0;
				ret=1;

/*
				if (size+24>out_buffer_size-s_pool.buffer_sizes[index])
				{
					s_pool.socket_state_list[index].ready_to_receive=0;
					//Dprintf(L_NOTICE, "NOT ready to receive yet\n");
				}
				else
				{
					s_pool.socket_state_list[index].ready_to_receive=1;
					//Dprintf(L_NOTICE, "READY to receive\n");
				}
 * 
*/
               s_pool.socket_state_list[index].ready_to_receive=1;
			}
		}


	}
	return ret;
 }


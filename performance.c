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

#include "performance.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include "proxy2.h"
#include "logging.h"
#include <pthread.h>
#include "config.h"
#include "scheduler_main.h"
extern long long small_io_size, large_io_size;
int performance_interval;
pthread_mutex_t counter_mutex;
extern int num_apps;
struct timeval last_count_time, first_count_time;
extern int first_receive, current_depth;
int first_time=0;
extern char* log_prefix;
int broadcast_amount;
FILE* broadcast_file;
int* average_resp_time;
void print_delay(int socket_index, char* event)
{
    struct timeval tv,diff;
/*
    char bptr[20];
    time_t tps,tpu,tps_diff, tpu_diff;
    gettimeofday(&tv, 0);
    timersub(&tv, &s_pool.socket_state_list[socket_index].last_work_time, &diff);
    tps_diff=diff.tv_sec;
    strftime(bptr, 9, "%H:%M:%S", localtime(&tps_diff));
    sprintf(bptr+8, ".%06ld", (long)diff.tv_usec);
    Dprintf(D_CACHE,"Interval:Delayed %s seconds before %s at %s\n",bptr,event,s_pool.socket_state_list[socket_index].ip);
    s_pool.socket_state_list[socket_index].last_work_time=tv;*/

}

struct performance_sta counter_output(int index, float seconds)
{
	//critical
	if (pthread_mutex_lock (&counter_mutex)!=0)
	{
		fprintf(stderr, "error locking when getting counter\n");
	}
	//Dprintf(D_CACHE,"Performance: %i / 1048576.0 / %2.5f \n",counters[index], seconds);
	struct performance_sta sta;
	app_stats[index].req_delay+=(app_stats[index].req_come-app_stats[index].req_go);
	sta.throughput=app_stats[index].byte_counter/1048576.0f/seconds;
	sta.req_go=app_stats[index].req_go;
	sta.req_come=app_stats[index].req_come;
	sta.req_delay=app_stats[index].req_delay;
	sta.block_count=get_block_count(index);
	app_stats[index].byte_counter=0;
	app_stats[index].req_go=0;
	app_stats[index].req_come=0;
	//exit critical
	if (pthread_mutex_unlock (&counter_mutex)!=0)
	{
		fprintf(stderr, "error locking after clearing counter\n");
	}
	return sta;
}

void* work_report(void * arg)
{
	int i;
	if (depthtrack==NULL)
		depthtrack=stderr;
	while(1)

	{
		if (first_receive)
		{
			if (!first_time)
			{
			    gettimeofday(&first_count_time,0);
				first_time=1;
			}

		}
		sleep(performance_interval);

	    struct timeval tv,diff,diffall;
	    time_t tp, tps,tpu,tps_all, tpu_all;
	    gettimeofday(&tv, 0);


	    timersub(&tv,&last_count_time,&diff);
	    last_count_time=tv;

	    tps = diff.tv_sec;
	    tpu = diff.tv_usec;
	    float seconds, seconds_all;

		char bptr[20];


	    tp = tv.tv_sec;
	    strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
	    sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);
	    seconds=(tps*1000000+tpu)/1000000.0f;
	    if (first_time)
	    {
			timersub(&tv,&first_count_time,&diffall);

			tps_all=diffall.tv_sec;
			tpu_all=diffall.tv_usec;
			seconds_all=(tps_all*1000000+tpu_all)/1000000.0f;
			//fprintf(stderr,"Seconds_All:%10.10f\n",seconds_all);
			if (seconds_all<0)
			{
				char bptr2[20];
			    time_t tp2 = first_count_time.tv_sec;
			    strftime(bptr2, 10, "[%H:%M:%S", localtime(&tp2));
			    sprintf(bptr2+9, ".%06ld]", (long)first_count_time.tv_usec);
				fprintf(stderr,"error! now:%s first:%s\n",bptr,bptr2);
			}
	    }


	    if (broadcast_file!=NULL)
	    {
			int temp_amount=0;
			//Dprintf(D_AUTH,"%s Performance update starts after %2.5f seconds\n",bptr, seconds);
			if (pthread_mutex_lock (&counter_mutex)!=0)
			{
				fprintf(stderr, "error locking when getting counter on throughput\n");
			}
			temp_amount=broadcast_amount;
			broadcast_amount=0;
			if (pthread_mutex_unlock (&counter_mutex)!=0)
			{
				fprintf(stderr, "error locking when unlocking counter throughput\n");
			}
			fprintf(broadcast_file, "%s Broadcasted messages %i bytes\n", log_prefix, temp_amount);
	    }
		for (i=0;i<num_apps;i++)
		{

			struct performance_sta sta=counter_output(i,seconds);//locked function
			fprintf(depthtrack, "%s %s Performance Result for App %i: %3.9f, %i\n",
					log_prefix, bptr,i+1, sta.throughput, sta.req_go);
			//fprintf(stderr,"small->%lli; large->%lli\n",small_io_size, large_io_size);
			//fprintf(stderr,"%s Performance Result for App %i: %3.9f MB/S, come: %i, go:%i, delay:%i, blocked:%i, current_depth:%i\n", bptr,i+1, sta.throughput,sta.req_come,sta.req_go,sta.req_delay,sta.block_count,current_depth);
			//Dprintf(D_AUTH,"%s Performance Result for App %i: %3.9f MB/S, come: %i, go:%i, delay:%i, blocked:%i, current_depth:%i\n", bptr,i+1, sta.throughput,sta.req_come,sta.req_go,sta.req_delay,sta.block_count,current_depth);
			if (first_time)
			{
				if (pthread_mutex_lock (&counter_mutex)!=0)
				{
					fprintf(stderr, "error locking when getting counter on throughput\n");
				}
				float app_th = app_stats[i].app_throughput/seconds_all/1048576.0f;

				//Dprintf(D_CACHE,"%s Performance Average for App %i: %3.9f MB/S (%lli bytes)\n",bptr, i+1,app_th,app_throughput[i]);
				if (pthread_mutex_unlock (&counter_mutex)!=0)
				{
					fprintf(stderr, "error locking when unlocking counter throughput\n");
				}
			}
		}
	}
}

pthread_t performance_thread;

void start_counter()
{
	gettimeofday(&last_count_time, 0);
	int rc = pthread_create(&performance_thread, NULL, work_report, NULL);
	if (rc){
		fprintf(stderr,"ERROR; return code from pthread_create() is %d\n", rc);
		exit(-1);
	}
}

int get_block_count(int app_index)
{
	int i;
	int block_count=0;
	for (i=0;i<s_pool.pool_size;i++)
	{
		if (s_pool.socket_state_list[i].app_index==app_index )
		{

			PINT_llist_p head = s_pool.socket_state_list[i].req_state_data->next;
			while (head!=NULL)
			{
				Dprintf(D_CACHE,"%s is blocking\n", s_pool.socket_state_list[i].ip);
				struct request_state* rs = (struct request_state *)(head->item);
				if (rs->locked)
				{block_count++;}
				head = head->next;
			}
		}
	}
	return block_count;
}

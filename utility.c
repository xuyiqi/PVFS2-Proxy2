/*
 * utility.c
 *
 *  Created on: Mar 22, 2010
 *      Author: yiqi
 */
#define START_THRESHOLD 10
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "sockio.h"
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "proxy2.h"
#include <poll.h>
#include "logging.h"
#include "limits.h"
#include "dump.h"
#include "llist.h"
#include "scheduler_SFQD.h"
#include "scheduler_DSFQ.h"
#include <pthread.h>
#include "performance.h"

#include "socket_pool.h"
#include "message.h"
#include "config.h"
#include "cost_model_history.h"

struct hashtable *h;
extern sfqd_current_depth;
extern char *directions[2];
extern char * log_prefix;

extern FILE* broadcast_file;
int  active_port;
char *ops[40]={"PVFS_SERV_INVALID","PVFS_SERV_CREATE","PVFS_SERV_REMOVE",
"PVFS_SERV_IO","PVFS_SERV_GETATTR","PVFS_SERV_SETATTR","PVFS_SERV_LOOKUP_PATH",
"PVFS_SERV_CRDIRENT","PVFS_SERV_RMDIRENT","PVFS_SERV_CHDIRENT","PVFS_SERV_TRUNCATE",
"PVFS_SERV_MKDIR","PVFS_SERV_READDIR","PVFS_SERV_GETCONFIG","PVFS_SERV_WRITE_COMPLETION",
"PVFS_SERV_FLUSH","PVFS_SERV_MGMT_SETPARAM","PVFS_SERV_MGMT_NOOP","PVFS_SERV_STATFS",
"PVFS_SERV_PERF_UPDATE","PVFS_SERV_MGMT_PERF_MON","PVFS_SERV_MGMT_ITERATE_HANDLES",
"PVFS_SERV_MGMT_DSPACE_INFO_LIST","PVFS_SERV_MGMT_EVENT_MON","PVFS_SERV_MGMT_REMOVE_OBJECT",
"PVFS_SERV_MGMT_REMOVE_DIRENT","PVFS_SERV_MGMT_GET_DIRDATA_HANDLE","PVFS_SERV_JOB_TIMER",
"PVFS_SERV_PROTO_ERROR","PVFS_SERV_GETEATTR","PVFS_SERV_SETEATTR","PVFS_SERV_DELEATTR",
"PVFS_SERV_LISTEATTR","PVFS_SERV_SMALL_IO","PVFS_SERV_LISTATTR","PVFS_SERV_BATCH_CREATE",
"PVFS_SERV_BATCH_REMOVE","PVFS_SERV_PRECREATE_POOL_REFILLER","PVFS_SERV_UNSTUFF",
/* leave this entry last */
"PVFS_SERV_NUM_OPS"};


char *modes[3]={"1","2","TCP_MODE_UNEXP"};
char *encodings[3]={"1","2","ENCODING_LE_BFIELD"};//enumeration names for debugging output
char *io_type[3]={"NULL","READ","WRITE"};

extern struct socket_pool  s_pool;
extern int logging;
extern struct heap *heap_queue;


/**
 *\param FILE* stream: the stream you want the log to be written to
 *\param char* fmt: format string
 *\param ... : what's needed to be filled into your format string
 *
 * same parameters as fprintf, just the logic checks if logging is enabled or not.
 * If logging is disabled, the function returns immediately; otherwise, it prints the message to the logging stream.
 * */
void p_log(FILE* stream, char* fmt, ...)

{
	if (!logging)
	{
		return;
	}
	  va_list args;
	  va_start (args, fmt);
	  vfprintf (stream,fmt, args);

	  va_end (args);
	  fflush(stream);

}


/**
 * Pulls several bytes from the buffer and transform it to an integer by the endianness specified
 * \param s is the byte buffer in which our value locates
 * \param length is the number of bytes from the start of the buffer s you want to transform
 * \param endian is the enumeration value of PVFS_BIG_ENDIAN or PVFS_LITTLE_ENDIAN
 * \return Returns the transformed value in long long (64 bit integer).  The caller can cast this to smaller values if necessary.
 *
 * */

//should use void*?
//this is a veryvery basic function affecting the protocl! be careful!
/////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////


long long convert(unsigned char s[] , int length, enum endian e)
{
	if (length>8)
	{
		Dprintf(D_CACHE,"Error: convert at most long long type of 8 bytes: %i\n",length);
		return 0;
	}
	long long ret=0;
	//length is 8 at most.

	if (e==PVFS_BIG_ENDIAN){//most significance first
			int i=0;
		while (i<length)
		{
			//printf("i:%i,s[i]:%i\n",i,s[i]);
			ret=(ret<<8)+s[i];
			//printf("ret:%lli",ret);
			i++;
		}
		fprintf(stderr,"big endinan!\n");
		exit(-1);
	}

	else if(e==PVFS_LITTLE_ENDIAN) {//least significance first
		if (length==8)
		{
			//fprintf(stderr,"before swapping64:");
			//fprintf(stderr, "%x%x%x%x%x%x%x%x",s[0],s[1],s[2],s[3],s[4],s[5],s[6],s[7]);
			//fprintf(stderr,"after swapping64:%#010x\n",*((long long * )s));

			//unsigned long long s7,s6,s5,s4,s3,s2,s1,s0;
			//s7=s[7];s6=s[6];s5=s[5];s4=s[4];s3=s[3];s2=s[2];s1=s[1];s0=s[0];
			//unsigned long long result = s7<<56 | s6<<48 | s5<<40 |s4<< 32 | s3 << 24 |s2 <<16 |s1<<8 | s0;

			//fprintf(stderr,"after swapping64:%#llx\n",result);



			//__bswap_constant_64_my2(*((uint64_t * )s));
			return *((long long int* )s);

			//return __bswap_constant_64(*((uint64_t * )s));
		}
		else if (length==4)
		{
/*			unsigned long long s3,s2,s1,s0;
			s3=s[3];s2=s[2];s1=s[1];s0=s[0];
			unsigned long long result = s3 << 24 |s2 <<16 |s1<<8 | s0;
			fprintf(stderr,"result:%lli\n",result);
			//s[1]=12;
			fprintf(stderr,"u32:%i\n",*((uint32_t *)s));
			fprintf(stderr, "s[0]:%#x\n",s[0]);
			fprintf(stderr, "s[1]:%#x\n",s[1]);
			fprintf(stderr, "s[2]:%#x\n",s[2]);
			fprintf(stderr, "s[3]:%#x\n",s[3]);
			return __bswap_constant_32(*((uint32_t *)s));*/
			return *((unsigned int * )s);
		}
		else if (length==2)
		{
/*			unsigned long long s1,s0;
			s1=s[1];s0=s[0];
			unsigned long long result = s1<<8 | s0;
			return result;*/
			//return  __bswap_constant_16(*((uint16_t *)s));
			return *((unsigned short int * )s);
		}
		else if (length==1)
		{
/*			unsigned long long s0;
			s0=s[0];

			return s0;*/
			return * ((unsigned char* )s);
		}

		else
		{
			fprintf(stderr,"fatal: length is %i\n", length);
			exit(-1);
		}
	}
	else
	{
		fprintf(stderr,"Error: unknown Endian info: %i",e);
		exit(-1);
		return 0;
	}
	return ret;
}
/**
 * Prints the byte stream byte by byte onto the screen
 * \param s is the buffer pointer of the byte stream
 * \param length is the count of bytes you want to print
 *
 * */
void output_stream(unsigned char s[], int length)
{
	int i=0;
	for (;i<length;i++)
	{

		Dprintf(D_CACHE, "%02x ",s[i]);

		if ((i+1) %16==0 && i+1<length)
		{Dprintf(D_CACHE,"%s","\n");
		}
		else if ((i+1) % 8 ==0 && i+1<length)
		{Dprintf(D_CACHE,"%s","|| ");}

	}
	Dprintf(D_CACHE,"%s","\n");
}
/**
 * Prints the raw byte stream of a specific range (usually a defined section in the stream)
 * Furthermore, it returns the value represented by the range.
 * It has the logic to print the value's meaning for debugging, however it's currently being muffled for performance.
 * \param s is the buffer pointer of the byte stream
 * \param length is the count of bytes you want to print
 *
 * */
long long output_param(unsigned char s[],
		int pos, int length, char* title, char ** tops, int limit)
{
	//printf("before parsing\n");
	long long temp=convert(s+pos,length,PVFS_LITTLE_ENDIAN);
	//if (temp<0 || temp>=limit)
	{
		//fprintf(stderr, "%s - %#010x\n",title, temp);
		//fprintf(stderr, " - look up limit exceeded, %lli\n",temp);
		return temp;
	}
/*	char *appendix=":%lli, %#llx\n";
	char* newtitle = (char*)malloc(strlen(title)+strlen(appendix)+1);

	strcpy(newtitle, title);
	strcat(newtitle, appendix);
	fprintf(stderr,newtitle,temp,temp);*/

	if (tops!=NULL)
	{
		//char *appendix2=":%s (lookup)\n";
		//char* newtitle2 = (char*)malloc(strlen(title)+strlen(appendix2)+1);

		//strcpy(newtitle2, title);
		//strcat(newtitle2, appendix2);

		//else
		{
		//Dprintf(D_CACHE,newtitle2, tops[temp]);
		}
	}
	return temp;
}


/**
 * Prints the difference of two time stamps in sec.nanosec format
 * \param end is the description string of the end of the time range
 * \param start is the description string of the start of the time range
 * \param tstart is the starting time stamp
 * \param tend is the ending time stamp
 * */
void print_time_diff(char * end, char* start, struct timespec tstart, struct timespec tend)
{
	int sec_diff=(tend.tv_sec)-(tstart.tv_sec);
	long nsec_diff=tend.tv_nsec-tstart.tv_nsec;
	if (tstart.tv_nsec>tend.tv_nsec)
	{
		sec_diff--;
		nsec_diff+=1000000000;
	}
	Dprintf(D_CACHE, "The time took between %s and %s is %i seconds %09li nanoseconds\n",start, end, sec_diff, nsec_diff);

}
int logical_to_physical_size_param (int current_server,
	int total_server, long long aggregate_size, int strip_size, long long offset, long long actual_datafile_size)

{
	//this version does not check eof, eof is to be checked by
	//getting physical size of offset+physical size of requested size
	// >= data file size!



}

int logical_to_physical_size_dparam (struct dist* dparam)
{
	int strip_size=dparam->stripe_size;
	int server_nr = dparam->current_server_number;
	int server_count= dparam->total_server_number;
	long long offset=dparam->data_file_offset;
	long long ask_size= dparam->aggregate_size;

	int my_shared_size = get_my_share(strip_size, server_count, offset, ask_size, server_nr);
    return my_shared_size;
}

int get_my_share(int strip_size, int server_count,
		long long  offset, long long  ask_size, int server_nr)
{
    int whole_strip_size = strip_size * server_count;
    int offset_nr_whole_strip = offset / whole_strip_size;//how many whole stripe the offset has passed
    int left_over_first = offset % whole_strip_size;//the passed bytes (where we skipped)
    int left_over_second;//the remaining part of the first stripe (where we have actual data)

    fprintf(stderr,"strip_size %i server_count %i offset %i ask_size %i server_nr %i\n",
    		strip_size, server_count, offset, ask_size, server_nr);
    fprintf(stderr,"%i + %i <= (%i +1) * %i ? %i\n",
    		ask_size, offset, offset_nr_whole_strip, strip_size,
    		ask_size + offset <= (offset_nr_whole_strip + 1) * strip_size
    );
    if ( ask_size + offset <= (offset_nr_whole_strip + 1) * whole_strip_size)
    	//the start and end of the file access stays in the same whole stripe
    {
    	if (left_over_first > (server_nr+1) * strip_size)
    	{
    		//this should not happen, only one whole stripe is occupied, but the head
    		//margin has not reached this server!
    		fprintf(stderr,"case 1, failure, %i, %i, %i\n", left_over_first, server_nr, strip_size);
    		exit(-1);
    	}
    	else
    	{
    		int small_first_left_over = left_over_first % strip_size;//the piece that pads ahead in a single strip/chunk
    		int first_preceeding_strips = left_over_first / strip_size;
    		server_nr -= first_preceeding_strips;
    		int number_strips = (small_first_left_over + ask_size) / strip_size + 1;
    		if (number_strips == 1 && small_first_left_over + ask_size <= strip_size)
    		{
    			return ask_size;
    		}
    		if (number_strips == 1 && small_first_left_over + ask_size > strip_size)
    		{
    			fprintf(stderr,"case 2, failure, %i, %i, %i\n",
    					small_first_left_over, ask_size, strip_size);
    			exit(-1);
    		}
    		//num_strip > 1
    		if (ask_size + small_first_left_over >= server_nr  * strip_size)
    		{
    			if (server_nr ==0)
    			{
    				return strip_size - small_first_left_over;
    			}
    			else
    			{
    				return strip_size;
    			}
    		}
    		else
    		{
    			return (ask_size + small_first_left_over) % strip_size;
    		}
    	}
    }


    if(left_over_first > 0){
        left_over_second = whole_strip_size - left_over_first;
    }else{
        left_over_second = 0;
    }
    //fprintf(stderr,"whole strip:%i, offset strips:%i, leftover left:%i, leftover right:%i\n",
    //whole_strip_size, offset_nr_whole_strip, left_over_first, left_over_second
    //);
    int data_nr_whole_strip = (ask_size - left_over_second) / whole_strip_size; //larger than 0
    int left_over_third = (ask_size - left_over_second) % whole_strip_size;
    int left_over_current_top = 0;
    int left_over_current_bottom = 0;

    if(left_over_third >= server_nr * strip_size){
        //third left over passed my start boundary
        if(left_over_third < (server_nr + 1) * strip_size)
        {
        	//but ends in my end boundary
            left_over_current_bottom += left_over_third - (server_nr * strip_size);
        }
        else
        {
        	//and passed to further stripes
            left_over_current_bottom += strip_size;
        }
    }

    if(left_over_first >= server_nr * strip_size){
        //first left over passed my start boundary
        if(left_over_first < (server_nr + 1) * strip_size)
            left_over_current_bottom -= left_over_first - (server_nr * strip_size);

        else
            left_over_current_bottom -= strip_size;

    }
    //fprintf(stderr,"left over bottom2:%i\n",left_over_current_bottom);
    int all_shared_size = data_nr_whole_strip * strip_size;

    if ( left_over_first > 0){
        all_shared_size += strip_size;
    }
    int my_shared_size = all_shared_size + left_over_current_bottom;
    return my_shared_size;
}

void dump_stat(int sig)
{

	timer_stop=1;

	struct timeval tv,diff,diffall;
    time_t tp, tps,tpu,tps_all, tpu_all;
    gettimeofday(&tv, 0);

    fprintf(stderr, "%s: got signal\n", log_prefix);
    float seconds, seconds_all;

	char bptr[20];

	fclose(actual_output);
	fclose(expected_output_gpa);
	fclose(expected_output_exp);
	if (broadcast_file!=NULL)
	{
		fclose(broadcast_file);
	}
	FILE* stat_file = fopen(dump_file,"a");
	if (stat_file==NULL)
	{
		fprintf(stderr, "%s: File cannot be opened\n", log_prefix);
		exit(0);
	}


	int j;
	fprintf(stat_file, "%s: expected:", log_prefix);
	for (j=0;j<num_apps;j++)
		{
			fprintf(stat_file, "%i:",app_stats[j].app_weight);

		}
	fprintf(stat_file, "\n");

	if (scheduler_index>=0)
	{

		if (!strcmp(static_methods[scheduler_index]->method_name, SFQD_SCHEDULER))
		{
			fprintf(stat_file, "%s: SFQD depth:%i\n", log_prefix,sfqd_depth);
		}
		if (!strcmp(static_methods[scheduler_index]->method_name, DSFQ_SCHEDULER))
		{
			fprintf(stat_file, "%s: DSFQ:%i\n", log_prefix,dsfq_depth);
		}
		if (!strcmp(static_methods[scheduler_index]->method_name, SFQD_FULL_SCHEDULER))
		{
			fprintf(stat_file, "%s: FULL_SFQD:%i\n", log_prefix,sfqdfull_depth);
		}
	}



    tp = tv.tv_sec;
    strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
    sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);
    seconds=(tps*1000000+tpu)/1000000.0f;
    if (first_receive)
    {
		timersub(&tv,&first_count_time,&diffall);

		tps_all=diffall.tv_sec;
		tpu_all=diffall.tv_usec;
		seconds_all=(tps_all*1000000+tpu_all)/1000000.0f;
		if (seconds_all<0)
		{
			char bptr2[20];
		    time_t tp2 = first_count_time.tv_sec;
		    strftime(bptr2, 10, "[%H:%M:%S", localtime(&tp2));
		    sprintf(bptr2+9, ".%06ld]", (long)first_count_time.tv_usec);
		    fprintf(stat_file,"%s: now:%s, first:%s\n", log_prefix, bptr, bptr2);

		}
		fprintf(stat_file,"%s: %s ran for %5.5f seconds\n", log_prefix, bptr, seconds_all);
    }
    else
    {
    	fprintf(stderr,"%s: %s counter not yet started\n", log_prefix, bptr);
    	fclose(stat_file);
    	exit(0);
    }



    float smallest=INT_MAX;int smallest_i=0;
    float th[num_apps];
    int i;
	for (i=0;i<num_apps;i++)
	{

		//struct performance_sta sta=counter_output(i,seconds);//locked function

//		Dprintf(D_CACHE,"%s Performance Result for App %i: %3.9f MB/S\n", bptr,i+1, sta.throughput);
		if (first_receive)
		{
			if (pthread_mutex_lock (&counter_mutex)!=0)
			{
				fprintf(stat_file, "%s: error locking when getting counter on throughput\n", log_prefix);
			}
			float app_th = app_stats[i].app_throughput/seconds_all/1048576.0f;
			if (smallest>app_th)
			{
				smallest=app_th;
				smallest_i=i;
			}
			th[i]=app_th;
			fprintf(stat_file,"%s: %s Performance Average for App %i: %3.9f MB/S (%lli bytes)\n", log_prefix, bptr, i+1,app_th,app_stats[i].app_throughput);
			if (pthread_mutex_unlock (&counter_mutex)!=0)
			{
				fprintf(stat_file, "%s: error locking when unlocking counter throughput\n", log_prefix);
			}
		}
		fprintf(stderr,"%s: app %i received %i, dispatched %i, completed %i\n", log_prefix, i, app_stats[i].received_requests, app_stats[i].dispatched_requests, app_stats[i].completed_requests);
		fprintf(stderr,"%s: average resp time is %i ms\n", log_prefix, average_resp_time[i]/app_stats[i].completed_requests);
	}


	if (smallest<0.001)
	{
		fprintf(stderr,"%s: baseline throughput too small %3.4f, no ratio output\n", log_prefix, smallest);
		exit(0);
	}
	float ratio[num_apps];
	ratio[smallest_i]=1;
	char ratio_str[100];
	char* tmp=ratio_str;
	fprintf(stat_file,"%s: %s ratio:", log_prefix, bptr);
	for (i=0;i<num_apps;i++)
	{
		if (i!=smallest_i)
		{
			ratio[i]=th[i]/smallest;

		}
		sprintf(tmp, "%02.3f:", ratio[i]);

		fprintf(stat_file,"%s", tmp);
	}
	fprintf(stat_file,"\n");
	fprintf(stat_file,"machine read/write error: %i %i\n", error_r_machine, error_w_machine);
	fprintf(stat_file,"app read/write error: %i %i\n", error_r_app, error_w_app);
	fprintf(stat_file,"client read/write error: %i %i\n", error_r_client, error_w_client);
	fprintf(stat_file,"ending depth:%i\n",sfqd_current_depth);
	fclose(stat_file);
	if (depthtrack!=NULL)
		fclose(depthtrack);
	exit(0);
}

/**
 * Changes the input string replacing a specified substring with another target string
 * \param str is the original string
 * \param start is the starting point within the buffer
 * \param orig is the original substring to be replaced
 * \param rep is the target string to be replace with
 * \param orig_len is the original string length
 * \return Returns the altered character string pointer
 * */
char *replace_str(char *str, int start, char *orig, char *rep, int orig_len)
{

  int diff=strlen(orig)-strlen(rep);
  //Dprintf(D_CACHE, "diff:%i\n",diff);
  int size=orig_len-diff;
  char* buffer=(char*)malloc(size+1);
  char *p;
  if ((p=strstr(str+start, orig))==NULL)
  {
	  Dprintf(D_CACHE, "original string %s not found!\n",orig);
	  return str;
  }


  memcpy(buffer, str, start);

  memcpy(buffer+start, rep, strlen(rep));

  memcpy(buffer+start+strlen(rep), str+start+strlen(orig),

		  orig_len-start-strlen(orig));

  buffer[size] = '\0';

  //Dprintf(D_CACHE, "%s\n",buffer);
  return buffer;
}

/**
 *\param char* buffer: the buffer containing the text you want to change
 *\param int got_size: the length of the buffer
 *\param char* str_from: the original string to replace
 *\param char* str_to: the target string to be replaced with
 *
 * Changes the port number for all the servers listed in the config file buffer.
 * */
void change_port(char * buffer, int got_size, char * str_from,char * str_to)
{

	char * temp = strstr(buffer, "tcp://");

	while( temp!=NULL)
	{

		//fprintf(stderr, "trying to change the config file...\n");
		char * temp2 = strstr(temp+7, ":");//skip tcp://, and we start with a port number symbol ":" after that
		if (temp2==NULL)
		{
			//Dprintf(D_CACHE, "no colon found!\n");

		}
		else
		{
			char * temp3=strstr(temp2, "\n");
			if (temp3==NULL)
			{
				//Dprintf(D_CACHE, "no ending found!\n");
			}
			else
			{
				char * p = buffer;
				//fprintf(stderr, "Start position:%i\n",temp2-p+1);
				memcpy(&buffer[temp2-p+1],"3335",4);//hard-coded here
			}
		}
		temp = strstr(temp2, "tcp://");//look for the next server string
		//fprintf(stderr, "finished changing the config file...\n");
	}

	//fprintf(stderr, "no tcp string found any more!\n");
	//fprintf(stderr, "%s\n", buffer);
}

void get_time_string(struct timeval *value, char* timestring)
{
	//timestring should be char[20]
	time_t tp;
	tp=value->tv_sec;
	strftime(timestring, 10, "[%H:%M:%S", localtime(&tp));
	sprintf(timestring+9, ".%06ld]", (long)(value->tv_usec));

}

void get_time_diff(struct timeval * last, struct timeval * diff)
{
	struct timeval nowtime;
    gettimeofday(&nowtime, 0);
   // fprintf(stderr,"last:%i.%06i, now:%i.%06i\n",
    //		(int)last->tv_sec, (int)last->tv_usec, (int)nowtime.tv_sec, (int)nowtime.tv_usec);
    timersub(&nowtime,last,diff);
}

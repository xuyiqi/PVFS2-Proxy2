/*
 * (C) 2009-2012 Florida International University
 *
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * See COPYING in top-level directory.
 *
 */

#include "proxy2.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "cost_model_history.h"
#include <time.h>
#include <sys/time.h>
#include "message.h"
extern char * log_prefix;
// cost model specific arrays
/*
 * app_num
 * most_recent, mid_recent, most_distant: 10, 20, 30l
 * */

int ** history_response_times_r, ** history_response_times_w, ** history_response_times;//for each app, we have a history recording the #short, medium, long term requests
//Resp_times are in milliseconds
int ** history_locations_r, ** history_locations_w, ** history_locations;//stdev...
int ** history_service_lengths_r, ** history_service_lengths_w, ** history_service_lengths;
//int * resp_time_snapshots;
int ** history_period_averages_r, ** history_period_averages_w, ** history_period_averages;
int error_r_machine, error_w_machine;
int error_r_app, error_w_app;
int error_r_client, error_w_client;

int * max_resp_r, *max_resp_w;
int exp_smth_rm, exp_smth_wm;
struct timeval * last_request; //used to track long term idleness used with #define IDLE_THRESHOLD 10 //seconds

int * resp_time_valid_r, * resp_time_valid_w, * expected_resp_time_r, * expected_resp_time_w;
int * resp_time_valid, * expected_resp_time;
//moving average

int last_history_index_r = 0;
int last_history_index_w = 0;

int exp2_r, exp2_w;

int cost_model=COST_MODEL_NONE;
FILE *actual_output, *expected_output_gpa, *expected_output_exp;
int * expsmth_exp_w, * expsmth_exp_r, * expsmth_exp;
//exponential smoothing

int output_only=1;//test flag

int s_last_w, s_current_w;
int b_last_w, b_current_w;

int s_last_r, s_current_r;
int b_last_r, b_current_r;

int get_average(int * array_start, int start_index, int end_index, int io_type)
{
	int* start_addr = array_start+start_index;
	int sum=0;
	int i;
	for (i=0;i<end_index-start_index+1; i++)
	{
		sum+=start_addr[i];
		fprintf(stderr," value %i\n", start_addr[i]);
	}

	fprintf(stderr,"%i items' sum is %i, average is %i\n",(end_index-start_index+1), sum, sum/(end_index-start_index+1));

	return sum/(end_index-start_index+1);
}
//print logs necessary for initial tests

void update_average(int app_index, int new_value1, int new_value2, int new_value3, int io_type)
{

	if (io_type==PVFS_IO_READ)
	{
		history_period_averages = history_period_averages_r;
	}
	else
	{
		history_period_averages = history_period_averages_w;
	}
	history_period_averages[app_index][0]=(history_period_averages[app_index][0]*(COST_MOST_RECENT_SLOT-1)+new_value1)/COST_MOST_RECENT_SLOT;
	history_period_averages[app_index][1]=(history_period_averages[app_index][1]*(COST_MEDIUM_RECENT_SLOT-1)+new_value2)/COST_MEDIUM_RECENT_SLOT;
	history_period_averages[app_index][2]=(history_period_averages[app_index][2]*(COST_MOST_DISTANT_SLOT-1)+new_value3)/COST_MOST_DISTANT_SLOT;
}


int cost_model_history_init ()
{

	error_r_machine = 0;
	error_w_machine = 0;
	error_r_app = 0;
	error_w_app = 0;
	error_r_client = 0;
	error_w_client = 0;

	b_last_w=-1048576;//we're testing with write only!
	b_last_r=-1048576;

	s_last_r=1024;
	s_last_w=1024;

	//this implementation holds both moving average and exponential smoothing
	//as it is much simpler than implementing different schedulers, they're put inside a single file/or even function
	char* act_resp, *exp_exp, *exp_gpa;
	act_resp=(char*)malloc(sizeof(char)*40);
	exp_exp=(char*)malloc(sizeof(char)*40);
	exp_gpa=(char*)malloc(sizeof(char)*40);

	snprintf(act_resp, 50, "%s.act_resp.txt", log_prefix);
	snprintf(exp_exp, 50, "%s.exp_resp_gpa.txt", log_prefix);
	snprintf(exp_gpa, 50, "%s.exp_resp_exp.txt", log_prefix);

	fprintf(stderr,"%s\n", act_resp);
	fprintf(stderr,"%s\n", exp_exp);
	fprintf(stderr,"%s\n", exp_gpa);

	actual_output=fopen(act_resp,"w");
	expected_output_gpa=fopen(exp_exp,"w");
	expected_output_exp=fopen(exp_gpa,"w");
	setbuf(actual_output, (char*)NULL);
	setbuf(expected_output_gpa, (char*)NULL);
	setbuf(expected_output_exp, (char*)NULL);

	if (cost_model==COST_MODEL_NONE)
	{
		return 0;
	}
	fprintf(stderr,"Initializing %i applications' history array...each with %i slots\n", num_apps,COST_TOTAL_HISTORY);

	last_request= (struct timeval *)malloc(sizeof(struct timeval)*num_apps);
	memset(last_request, 0, sizeof(struct timeval)*num_apps);

	max_resp_w = (int *)malloc(sizeof(int)*num_apps);
	memset(max_resp_w, 0, sizeof(int)*num_apps);

	max_resp_r = (int *)malloc(sizeof(int)*num_apps);
	memset(max_resp_r, 0, sizeof(int)*num_apps);



	//resp_time_snapshots = (int*)malloc(sizeof(int)*num_apps);
	//memset(resp_time_snapshots, 0, sizeof(int)*num_apps);

	expsmth_exp_r = (int*)malloc(sizeof(int)*num_apps);
	memset(expsmth_exp_r, 0, sizeof(int)*num_apps);

	expsmth_exp_w = (int*)malloc(sizeof(int)*num_apps);
	memset(expsmth_exp_w, 0, sizeof(int)*num_apps);
	//when the simple average value is used, this snapshot can reduce the running time of the new expected response from average.

	resp_time_valid_w = (int*)malloc(sizeof(int)*num_apps);
	resp_time_valid_r = (int*)malloc(sizeof(int)*num_apps);
	int i;
	for (i=0;i<num_apps;i++)
	{
		resp_time_valid_r[i]=-1;
		expsmth_exp_r[i]=1024;
		resp_time_valid_w[i]=-1;
		expsmth_exp_w[i]=1024;

	}
	expected_resp_time_r = (int*)malloc(sizeof(int)*num_apps);
	memset(expected_resp_time_r, 1, sizeof(int)*num_apps);
	expected_resp_time_w = (int*)malloc(sizeof(int)*num_apps);
	memset(expected_resp_time_w, 1, sizeof(int)*num_apps);

	history_period_averages_r = (int**)malloc(sizeof(int*)*num_apps);
	memset(history_period_averages_r, 0, sizeof(int*)*num_apps);
	history_period_averages_w = (int**)malloc(sizeof(int*)*num_apps);
	memset(history_period_averages_w, 0, sizeof(int*)*num_apps);

	history_response_times_r = (int**)malloc(sizeof(int*)*num_apps);
	memset(history_response_times_r, 0, sizeof(int*)*num_apps);
	history_response_times_w = (int**)malloc(sizeof(int*)*num_apps);
	memset(history_response_times_w, 0, sizeof(int*)*num_apps);

	int j;
	for ( i=0; i<num_apps; i++)
	{
		history_response_times_r[i] = (int*)malloc(sizeof(int)*COST_TOTAL_HISTORY);
		history_response_times_w[i] = (int*)malloc(sizeof(int)*COST_TOTAL_HISTORY);
		for ( j=0; j<COST_TOTAL_HISTORY; j++)
		{history_response_times_r[i][j]=1024;history_response_times_w[i][j]=1024;}

		history_period_averages_r[i] = (int*)malloc(sizeof(int)*COST_NUM_PERIODS);
		history_period_averages_w[i] = (int*)malloc(sizeof(int)*COST_NUM_PERIODS);
		memset(history_period_averages_r[i], 0, sizeof(int)*COST_NUM_PERIODS);
		memset(history_period_averages_w[i], 0, sizeof(int)*COST_NUM_PERIODS);
		//IMPORTANT: we initialize the startup response times to be some (same) value instead of 0 because
		//as such, each application will have an equal weight in regard to their initial "cost" history when there
		history_period_averages_r[i][0]=get_average(history_response_times_r[i], 0, COST_MOST_RECENT_SLOT-1, PVFS_IO_READ);
		history_period_averages_r[i][1]=get_average(history_response_times_r[i], COST_MOST_RECENT_SLOT, COST_MOST_RECENT_SLOT+COST_MEDIUM_RECENT_SLOT-1, PVFS_IO_READ);
		history_period_averages_r[i][2]=get_average(history_response_times_r[i], COST_MOST_RECENT_SLOT+COST_MEDIUM_RECENT_SLOT, COST_TOTAL_HISTORY-1, PVFS_IO_READ);
		//no value available.
		history_period_averages_w[i][0]=get_average(history_response_times_w[i], 0, COST_MOST_RECENT_SLOT-1, PVFS_IO_WRITE);
		history_period_averages_w[i][1]=get_average(history_response_times_w[i], COST_MOST_RECENT_SLOT, COST_MOST_RECENT_SLOT+COST_MEDIUM_RECENT_SLOT-1, PVFS_IO_WRITE);
		history_period_averages_w[i][2]=get_average(history_response_times_w[i], COST_MOST_RECENT_SLOT+COST_MEDIUM_RECENT_SLOT, COST_TOTAL_HISTORY-1, PVFS_IO_WRITE);
	}

	history_locations_r = (int**)malloc(sizeof(int*)*num_apps);
	memset(history_locations_r, 0, sizeof(int*)*num_apps);
	history_locations_w = (int**)malloc(sizeof(int*)*num_apps);
	memset(history_locations_w, 0, sizeof(int*)*num_apps);

	for (i=0; i<num_apps; i++)
	{
		history_locations_r[i] = (int*)malloc(sizeof(int)*COST_TOTAL_HISTORY);
		memset(history_locations_r[i], 0, sizeof(int)*COST_TOTAL_HISTORY);
		history_locations_w[i] = (int*)malloc(sizeof(int)*COST_TOTAL_HISTORY);
		memset(history_locations_w[i], 0, sizeof(int)*COST_TOTAL_HISTORY);
	}

	history_service_lengths_r = (int**)malloc(sizeof(int*)*num_apps);
	memset(history_service_lengths_r, 0, sizeof(int*)*num_apps);
	history_service_lengths_w = (int**)malloc(sizeof(int*)*num_apps);
	memset(history_service_lengths_w, 0, sizeof(int*)*num_apps);

	for (i=0; i<num_apps; i++)
	{
		history_service_lengths_r[i] = (int*)malloc(sizeof(int)*COST_TOTAL_HISTORY);
		memset(history_service_lengths_r[i], 0, sizeof(int)*COST_TOTAL_HISTORY);
		history_service_lengths_w[i] = (int*)malloc(sizeof(int)*COST_TOTAL_HISTORY);
		memset(history_service_lengths_w[i], 0, sizeof(int)*COST_TOTAL_HISTORY);
	}

	return 0;
}

int update_history(int app_index, int sock_index, int location, int length, int resp_time, int io_type)
{

	if (cost_model==COST_MODEL_NONE)
	{
		return 0;
	}

	//fprintf (stderr,"%s [ACT] [io %i] [Client %i] [App %i] Resp time is %i ms\n",log_prefix, io_type, sock_index, app_index, resp_time);
	fprintf (actual_output,"%s [ACT] [io %i] [Client %i] [App %i] Resp time is %i ms\n",log_prefix, io_type, sock_index, app_index, resp_time);
	fflush(actual_output);
	if (cost_model==COST_MODEL_GPA
			//|| output_only==1
			)
	{
		if (io_type==PVFS_IO_READ)
		{
			history_response_times = history_response_times_r;
			history_locations = history_locations_r;
			history_service_lengths = history_service_lengths_r;
			resp_time_valid = resp_time_valid_r;
		}
		else
		{
			history_response_times = history_response_times_w;
			history_locations = history_locations_w;
			history_service_lengths = history_service_lengths_w;
			resp_time_valid = resp_time_valid_w;
		}

		/* should be updated when history is updated/ will slower app get
		 *  last_average * (num_slots -1) + last_updated_value = new total
		 *  new_total / num_slots = new average
		 *  at the end, we may need to update the snapshot of the average, or last calculated value.
		 */
		int new_value1, new_value2, new_value3;
		new_value1=resp_time;//namely, history_response_times[app_index][last_history_index]


		if (io_type==PVFS_IO_READ)
		{
			history_response_times[app_index][last_history_index_r]=resp_time;//for each app, we have a history recording the #short, medium, long term requests
			//Resp_times are in milliseconds
			history_locations[app_index][last_history_index_r]=location;//stdev...
			history_service_lengths[app_index][last_history_index_r]=length;

			new_value2=history_response_times[app_index][(last_history_index_r+ COST_MOST_RECENT_SLOT)%COST_TOTAL_HISTORY];
			new_value3=history_response_times[app_index][(last_history_index_r+ COST_MOST_RECENT_SLOT+COST_MEDIUM_RECENT_SLOT)%COST_TOTAL_HISTORY];

			update_average(app_index,  new_value1, new_value2, new_value3, io_type);

			last_history_index_r++;
			last_history_index_r = last_history_index_r % COST_TOTAL_HISTORY; //go back to the top of array
		}
		else
		{
			history_response_times[app_index][last_history_index_w]=resp_time;//for each app, we have a history recording the #short, medium, long term requests
			//Resp_times are in milliseconds
			history_locations[app_index][last_history_index_w]=location;//stdev...
			history_service_lengths[app_index][last_history_index_w]=length;

			new_value2=history_response_times[app_index][(last_history_index_w+ COST_MOST_RECENT_SLOT)%COST_TOTAL_HISTORY];
			new_value3=history_response_times[app_index][(last_history_index_w+ COST_MOST_RECENT_SLOT+COST_MEDIUM_RECENT_SLOT)%COST_TOTAL_HISTORY];

			update_average(app_index,  new_value1, new_value2, new_value3, io_type);

			last_history_index_w++;
			last_history_index_w = last_history_index_w % COST_TOTAL_HISTORY; //go back to the top of array

		}
		resp_time_valid[app_index]=-1;//new expected time needs to be calculated...

	}
	else
	if (cost_model==COST_MODEL_EXPSMTH
		//||output_only==1
	)
	{
		int smth_value;
		if (io_type == PVFS_IO_READ)
		{
			smth_value = s_pool.socket_state_list[sock_index].exp_smth_value_r;
			error_r_client+=abs(smth_value-resp_time);
			s_pool.socket_state_list[sock_index].exp_smth_value_r=(COST_EXPSMTH_ALPHA)*resp_time + (1-(COST_EXPSMTH_ALPHA))*smth_value;
			//fprintf(stderr,"expected value changing from %i by %f * %i + (1- %f) * %i to %i\n",smth_value,
				//	COST_EXPSMTH_ALPHA, resp_time, COST_EXPSMTH_ALPHA, smth_value,
					//s_pool.socket_state_list[sock_index].exp_smth_value_r);
			error_r_app+=abs(expsmth_exp_r[app_index]-resp_time);
			expsmth_exp_r[app_index]=(COST_EXPSMTH_ALPHA)*resp_time + (1-(COST_EXPSMTH_ALPHA))*expsmth_exp_r[app_index];
			error_r_machine+=abs(exp_smth_rm-resp_time);
			exp_smth_rm=(COST_EXPSMTH_ALPHA)*resp_time + (1-(COST_EXPSMTH_ALPHA))*exp_smth_rm;


			if (b_last_r==-1048576)b_last_r=resp_time-s_last_r;//if this is requesting for b0...use x1-x0/1=x1-s0
			int s_last_r_temp=s_current_r;
			int b_last_r_temp=b_current_r;
			s_current_r = COST_EXPSMTH_ALPHA * resp_time + (1-COST_EXPSMTH_ALPHA)* (s_last_r + b_last_r);
			b_current_r = COST_EXPSMTH_BETA  * (s_current_r - s_last_r) + (1-COST_EXPSMTH_BETA) * b_last_r;

			s_last_r=s_last_r_temp;
			b_last_r=b_last_r_temp;

			exp2_r=s_current_r + b_current_r;



		}
		else
		{
			smth_value = s_pool.socket_state_list[sock_index].exp_smth_value_w;
			error_w_client+=abs(smth_value-resp_time);
			s_pool.socket_state_list[sock_index].exp_smth_value_w=(COST_EXPSMTH_ALPHA)*resp_time + (1-(COST_EXPSMTH_ALPHA))*smth_value;
			//fprintf(stderr,"expected value changing from %i by %f * %i + (1- %f) * %i to %i\n",smth_value,
				//	COST_EXPSMTH_ALPHA, resp_time, COST_EXPSMTH_ALPHA, smth_value,
					//s_pool.socket_state_list[sock_index].exp_smth_value_w);
			error_w_app+=abs(s_pool.socket_state_list[sock_index].last_exp_w_app-resp_time);
			expsmth_exp_w[app_index]=(COST_EXPSMTH_ALPHA)*resp_time + (1-(COST_EXPSMTH_ALPHA))*expsmth_exp_w[app_index];
			error_w_machine+=abs(s_pool.socket_state_list[sock_index].last_exp_w_machine-resp_time);
			exp_smth_wm=(COST_EXPSMTH_ALPHA)*resp_time + (1-(COST_EXPSMTH_ALPHA))*exp_smth_wm;


			if (b_last_w==-1048576)b_last_w=resp_time-s_last_w;//if this is requesting for b0...use x1-x0/1=x1-s0
			int s_last_w_temp=s_current_w;
			int b_last_w_temp=b_current_w;
			s_current_w = COST_EXPSMTH_ALPHA * resp_time + (1-COST_EXPSMTH_ALPHA)* (s_last_w + b_last_w);
			b_current_w = COST_EXPSMTH_BETA  * (s_current_w - s_last_w) + (1-COST_EXPSMTH_BETA) * b_last_w;

			s_last_w=s_last_w_temp;
			b_last_w=b_last_w_temp;

			exp2_w=s_current_w + b_current_w;
		}

		//update sock index here with a
	}
	else
	{
		fprintf(stderr,"[upd_history]unknown cost model: %i\n", cost_model);
		exit(-1);
	}
	return 0;
}


int get_expected_resp(int app_index, int sock_index, int io_type)
{
	/*
	#define COST_MOST_RECENT_SLOT 10
	#define COST_MEDIUM_RECENT_SLOT 20
	#define COST_MOST_DISTANT_SLOT 30
	#define COST_TOTAL_HISTORY COST_MOST_RECENT_SLOT+COST_MEDIUM_RECENT_SLOT+COST_MOST_DISTANT_SLOT

	#define COST_MOST_RECENT_WEIGHT 50
	#define COST_MOST_MEDIUM_RECENT_WEIGHT 30
	#define COST_MOST_DISTANT_WEIGHT 20
	*/

	/* calculation begins, the most important part of the algorithm
     * if idleness >= IDLE_THRESHOLD in seconds, then reversed calculation is used to ensure historical average is used
	 *
	 * */

	int new_expected_resp_time=1;
	if (cost_model==COST_MODEL_NONE){
		return -1;
	}
	else if (cost_model==COST_MODEL_GPA
			//||output_only==1
	)
	{
		//simple model, like calculating GPA....//use smarter way to update

		if (io_type==PVFS_IO_READ)
		{
			resp_time_valid = resp_time_valid_r;
			expected_resp_time = expected_resp_time_r;
		}
		else
		{
			resp_time_valid = resp_time_valid_w;
			expected_resp_time = expected_resp_time_w;
		}
		if (resp_time_valid[app_index]!=1)
		{
			struct timeval tv2, diff;
      	    gettimeofday(&tv2, 0);
      	    timersub(&tv2, &last_request[app_index], &diff);

      	    if (diff.tv_sec>=IDLE_THRESHOLD)
      	    {
    			expected_resp_time[app_index] = (COST_MOST_DISTANT_WEIGHT*history_period_averages[app_index][0] +
    			COST_MEDIUM_RECENT_WEIGHT*history_period_averages[app_index][1] +
    			COST_MOST_RECENT_WEIGHT* history_period_averages[app_index][2])/COST_TOTAL_WEIGHT;
    			fprintf(stderr,"the next calculation weight sequence is %i, %i, %i\n", COST_MOST_DISTANT_WEIGHT, COST_MEDIUM_RECENT_WEIGHT, COST_MOST_RECENT_WEIGHT);
      	    }
      	    else
      	    {
    			expected_resp_time[app_index] = (COST_MOST_RECENT_WEIGHT*history_period_averages[app_index][0] +
    			COST_MEDIUM_RECENT_WEIGHT*history_period_averages[app_index][1] +
    			COST_MOST_DISTANT_WEIGHT* history_period_averages[app_index][2])/COST_TOTAL_WEIGHT;
    			fprintf(stderr,"the next calculation weight sequence is %i, %i, %i\n", COST_MOST_RECENT_WEIGHT, COST_MEDIUM_RECENT_WEIGHT, COST_MOST_DISTANT_WEIGHT);
      	    }
			fprintf(stderr,"[app %i] average1: %i, average2: %i, average3: %i, final: %i\n", app_index, history_period_averages[app_index][0],history_period_averages[app_index][1],history_period_averages[app_index][2], new_expected_resp_time);
			resp_time_valid[app_index]=1;
		}

		fprintf(stderr, "%s [GPA] [%i] [app %i] expected time is %i\n", log_prefix, io_type, app_index, expected_resp_time[app_index]);
		fprintf(expected_output_gpa, "%s [GPA] [%i] [app %i] expected time is %i\n", log_prefix, io_type, app_index, expected_resp_time[app_index]);
		return expected_resp_time[app_index];

	}
	else
	if (cost_model==COST_MODEL_EXPSMTH
		//||output_only==1
		)
	{
		int smth_value_c, smth_value_a, smth_value_m;
		if (io_type==PVFS_IO_READ)
		{
			smth_value_c=s_pool.socket_state_list[sock_index].exp_smth_value_r;
			smth_value_a=expsmth_exp_r[app_index];
			smth_value_m=exp_smth_rm;
			s_pool.socket_state_list[sock_index].last_exp_r_app=smth_value_a;



			fprintf(expected_output_exp, "%s [EXP2] [io %i] [Client %i] [app %i] [per machine] expected time is %i\n",
					log_prefix, io_type, sock_index, app_index, exp2_r);
			s_pool.socket_state_list[sock_index].last_exp_r_machine=smth_value_m;
			s_pool.socket_state_list[sock_index].last_exp2_r_machine=exp2_r;//s_current_r+b_current_r;

			fprintf(expected_output_exp, "%s [EXP2] [io %i] [Client %i] [app %i] [per machine] expected time is %i\n", log_prefix, io_type, sock_index, app_index, exp2_r);

		}
		else
		{
			smth_value_c=s_pool.socket_state_list[sock_index].exp_smth_value_w;
			smth_value_a=expsmth_exp_w[app_index];
			smth_value_m=exp_smth_wm;
			s_pool.socket_state_list[sock_index].last_exp_w_app=smth_value_a;
			s_pool.socket_state_list[sock_index].last_exp_w_machine=smth_value_m;


			fprintf(expected_output_exp, "%s [EXP2] [io %i] [Client %i] [app %i] [per machine] expected time is %i\n",
					log_prefix, io_type, sock_index, app_index, exp2_w);
			s_pool.socket_state_list[sock_index].last_exp_w_machine=smth_value_m;
			s_pool.socket_state_list[sock_index].last_exp2_w_machine=exp2_w;

			fprintf(expected_output_exp, "%s [EXP2] [io %i] [Client %i] [app %i] [per machine] expected time is %i\n", log_prefix, io_type, sock_index, app_index, exp2_w);

		}
		//fprintf(stderr, "%s [EXP] [io %i] [Client %i] [app %i] [per client] expected time is %i\n", log_prefix, io_type, sock_index, app_index, smth_value_c);
		fprintf(expected_output_exp, "%s [EXP] [io %i] [Client %i] [app %i] [per client] expected time is %i\n", log_prefix, io_type, sock_index, app_index, smth_value_c);

		//fprintf(stderr, "%s [EXP] [io %i] [Client %i] [app %i] [per app] expected time is %i\n", log_prefix, io_type, sock_index, app_index, smth_value_a);
		fprintf(expected_output_exp, "%s [EXP] [io %i] [Client %i] [app %i] [per app] expected time is %i\n", log_prefix, io_type, sock_index, app_index, smth_value_a);

		//fprintf(stderr, "%s [EXP] [io %i] [Client %i] [app %i] [per machine] expected time is %i\n", log_prefix, io_type, sock_index, app_index, smth_value_m);
		fprintf(expected_output_exp, "%s [EXP] [io %i] [Client %i] [app %i] [per machine] expected time is %i\n", log_prefix, io_type, sock_index, app_index, smth_value_m);




		fflush(expected_output_exp);




		return 0;//smth_value;

	}
	else
	{
		//medium model using length info
		//hard model, using stdev to consider locality
		fprintf(stderr,"[get_exp] unknown cost model: %i\n", cost_model);
		exit(-1);
	}

	return -1;
}
void update_receipt_time(int sock_index, struct timeval tv, int io_type)
{
	//called when the request will be forwarded and served. (on the hook of release control)

	if (cost_model==COST_MODEL_NONE)
	{
		return ;
	}

/*	time_t tp;

	tp = tv.tv_sec;
	    //strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
	    //sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);
	int app_index = s_pool.socket_state_list[sock_index].app_index;

	last_request[app_index]=tv;*/

	//update sock_state_list
}


void update_release_time(int sock_index, struct timeval tv, int io_type)
{
	//called when the request will be forwarded and served. (on the hook of release control)

	time_t tp;

	tp = tv.tv_sec;
	    //strftime(bptr, 10, "[%H:%M:%S", localtime(&tp));
	    //sprintf(bptr+9, ".%06ld]", (long)tv.tv_usec);
	if (io_type==PVFS_IO_READ)
	{
		s_pool.socket_state_list[sock_index].last_work_time_r=tv;
	}else
	{
		s_pool.socket_state_list[sock_index].last_work_time_w=tv;
	}

	//update sock_state_list
}

int get_response_time(int sock_index, struct timeval tv2, int io_type)
{
	//called when the action is complete // hook of I/O request completion

	//get value from sock_state_list
	//calculate difference
	//return difference


	struct timeval tv,diff;
	if (io_type==PVFS_IO_READ)
	{
		tv = s_pool.socket_state_list[sock_index].last_work_time_r;
	}
	else
	{
		tv = s_pool.socket_state_list[sock_index].last_work_time_w;
	}
	   // fprintf (stderr, "last work time is %i.%i\n", tv.tv_sec, tv.tv_usec);

	time_t tps,tpu,tps_diff, tpu_diff;
	gettimeofday(&tv2, 0);
	timersub(&tv2, &tv, &diff);
	tps_diff=diff.tv_sec;
		//fprintf (stderr, "this work time is %i.%i\n", tv2.tv_sec, tv2.tv_usec);

		//strftime(bptr, 9, "%H:%M:%S", localtime(&tps_diff));
		//sprintf(bptr+8, ".%06ld", (long)diff.tv_usec);
	int final_diff = tps_diff*1000+diff.tv_usec/1000; //1 second = 1000 milli second, 1 microsecond = 1/1000 milli second
	//fprintf(stderr,"Got Response time [io %i] [Client ]%i ms from %li.%li\n", io_type, sock_index, final_diff, diff.tv_sec, diff.tv_usec);
		//fprintf(stderr,"%i-%i=%i, %i-%i=%i\n", tv2.tv_sec, tv.tv_sec, tv2.tv_sec-tv.tv_sec, tv2.tv_usec, tv.tv_usec,tv2.tv_usec-tv.tv_usec);

    return final_diff;

}


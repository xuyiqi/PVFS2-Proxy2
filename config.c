/*
 * config.c
 *
 *  Created on: Oct 4, 2010
 *      Author: yiqi
 */
#include "llist.h"
#include "heap.h"

#include "scheduler_SFQD.h"
#include "config.h"
#include "scheduler_DSFQ.h"

#ifdef __STATIC_SCHEDULER_SFQD__

#endif
#ifdef __STATIC_SCHEDULER_FIFO__

#endif
#ifdef __STATIC_SCHEDULER_DSFQ__

#endif
#ifdef __STATIC_SCHEDULER_TEST__

#include "scheduler_test.h"

#endif

#include "logging.h"
#include <string.h>
#include "dictionary.h"
#include "performance.h"
#include "iniparser.h"
struct ip_application* default_ip_application;
//PINT_llist_p *application_locations;
char* config_s="weights.ini";
struct app_statistics * app_stats;
struct timeval *iedf_deadlines_timeval;
int* iedf_deadlines;
int* apps;
int num_apps=0;
int total_weight=0;
extern int scheduler_on;
extern int* received_requests;
extern int* dispatched_requests;
extern int* completed_requests;

void initialize_hashtable()
{



	default_ip_application = (struct ip_application*)malloc(sizeof(struct ip_application));
	default_ip_application->supplied_value="default";
	default_ip_application->weight=1;//default_weight;

}

struct ip_application* ip_weight(char* ip)
{
	//app index iteration
	//find appindex-ip
	//return weight

	//
	int i;
	for (i=0;i<num_apps;i++)
	{
		PINT_llist_p p = app_stats[i].application_locations;
		p=p->next;
		//Dprintf(D_CACHE,"%i th entry\n",i);
		int j=0;
		while (p!=NULL)
		{
			//get value
			//Dprintf(D_CACHE,"%i th item\n",j);
			struct ip_application* curr_ipa = (struct ip_application*)(p->item);
			char* ip2 = (char*)malloc(strlen(ip)+1);
			strcpy(ip2,ip);
			//Dprintf(D_CACHE,"comparing %s against %s\n",ip2,curr_ipa->supplied_value);
			if (!in_range(ip2, curr_ipa))
			{

				return curr_ipa;
			}
			p = p->next;
			j++;
		}
		//do a is_in_range match in the list
		//if returns true, return the index value
	}
	fprintf(stderr, "error:range not found, returning default(last) item not available yet!\n");
	exit(-1);
	return default_ip_application;
}


int app_index_weight(int app)
{
    return app_stats[app].app_weight;

}

int set_start_end(char* range, char* splitter, struct ip_application* ip_range)
{
	int i=0;
	char* tmp_str=NULL;

	tmp_str=strtok(range,splitter);

	if (tmp_str==NULL)
	{
		fprintf(stderr,"cannot start first splitting range %s with -\n",range);
		return -1;
	}

	char* range1=(char*)malloc(strlen(tmp_str)+1);
	strcpy(range1, tmp_str);
	//Dprintf(D_CACHE,"range1:%s\n",range1);
	//Dprintf(D_CACHE,"range:%s\n", range);


	tmp_str=strtok(NULL,splitter);
	if (tmp_str==NULL)
	{
		Dprintf(D_CACHE,"cannot start second splitting range %s with -\n",range);
		return -1;
	}
	//Dprintf(D_CACHE,"range2:%s\n",tmp_str);
	//Dprintf(D_CACHE,"range:%s\n",range);
	if (set_IP_numbers(range1,&ip_range->start1,&ip_range->start2,&ip_range->start3,&ip_range->start4)==-1)
	{
		return -1;
	}
	if (set_IP_numbers(tmp_str,&ip_range->end1,&ip_range->end2,&ip_range->end3,&ip_range->end4)==-1)
	{
		return -1;
	}
	return 0;

}

int print_ip_app_item(void* item)
{
	fprintf(stderr,"%s\n",((struct ip_application*)item)->supplied_value);
	return 0;
}
int split_into_ranges(int app_index, char* value, PINT_llist_p current_list)
{

	char *tmp_str=strtok(value,",");

//find the applicatio nitem accorcing to app_index, cast to llist again

	int added_head=0;
	PINT_llist_p tail_head=NULL;
	while (tmp_str !=NULL)
	{
		struct ip_application* ipa=(struct ip_application*)malloc(sizeof (struct ip_application));

		ipa->supplied_value=tmp_str;
		Dprintf(D_CACHE,"processing %s\n",tmp_str);
		ipa->app_index=app_index;
		if (strlen(tmp_str)>=16)
		{
			Dprintf(D_CACHE,"range\n");
			ipa->range=RANGE;
		}
		else
		{
			Dprintf(D_CACHE,"ip\n");
			ipa->range=IP;
			//ipa->end1=255;
			//ipa->end2=255;
			//ipa->end3=255;
			//ipa->end4=255;
		}

		PINT_llist_add_to_tail(current_list, ipa);
		//if (added_head==0)
		{
			//tail_head=PINT_llist_tail(current_list);
		}
		added_head++;
		tmp_str=strtok(NULL,",");
	}

   return 0;

}
int set_IP_numbers(char* ip2,  short* i1,short* i2, short* i3, short* i4)
{
	int i=0;
	//char ip[]="192.168.0.1";
	/*first location*/
	//Dprintf(D_CACHE,"starting with %s\n",ip2);
	char* tmp_str;

	char * ip = (char*)malloc(strlen(ip2)+1);
	strcpy(ip,ip2);
	tmp_str=strtok(ip,".");

	if (tmp_str !=NULL)
	{

	}
	else
	{
		//Dprintf(D_CACHE,"cannot start first splitting ip %s with .\n",ip);
		return -1;
	}
	int tmp_int=atoi(tmp_str);
	if (tmp_int>255 || tmp_int <0)
	{
		//Dprintf(D_CACHE,"first location has %s\n",tmp_str);
		return -1;
	}
	*i1=tmp_int;

	/*second location*/
	tmp_str=strtok(NULL,".");

	if (tmp_str!=NULL)
	{

	}
	else
	{
		//Dprintf(D_CACHE,"cannot start second splitting ip %s with .\n",ip);

		return -1;
	}
	tmp_int=atoi(tmp_str);
	if (tmp_int>255 || tmp_int <0)
	{
		//Dprintf(D_CACHE,"second location has %s\n",tmp_str);
		return -1;
	}
	*i2=tmp_int;
	/*third location*/
	tmp_str=strtok(NULL,".");
	if (tmp_str==NULL)

	{		//Dprintf(D_CACHE,"cannot start third splitting ip %s with .\n",ip);

		return -1;
	}
	 tmp_int=atoi(tmp_str);
	if (tmp_int>255 || tmp_int <0)

	{
		//Dprintf(D_CACHE,"third location has %s\n",tmp_str);
		return -1;
	}

	*i3=tmp_int;
	/*fourth location*/
	tmp_str=strtok(NULL,".");
	if (tmp_str==NULL)
	{		//Dprintf(D_CACHE,"cannot start fourth splitting ip %s with .\n",ip);

		return -1;
	}
	 tmp_int=atoi(tmp_str);
	if (tmp_int>255 || tmp_int <0)

	{
		//Dprintf(D_CACHE,"fourth location has %s\n",tmp_str);
		return -1;
	}

	*i4=tmp_int;
	return 0;

}
enum edge{TOP,BOTTOM};
int check_edge(int i, enum edge e,short* against, short* source)
{
	if (i>=4)
	{
		return 0;
	}
	if (e==TOP)
	{
		for (;i<4;i++)
		{
			if (against[i]>source[i])
			{
				return -1;
			}
			if (against[i]<source[i])
			{
				return 0;
			}
			if (against[i]==source[i])
			{
				return check_edge(i+1,e,against,source);
			}
		}
	}
	else
	{
		for (;i<4;i++)
		{
			if (against[i]>source[i])
			{
				return 0;
			}
			if (against[i]>source[i])
			{
				return -1;
			}
			if (against[i]==source[i])
			{
				return check_edge(i+1,e,against,source);
			}
		}

	}
	return 0;
}

int in_range(char* ip_from, struct ip_application* ipa_range)
{
	short ip1,ip2,ip3,ip4;
	if (set_IP_numbers(ip_from,  &ip1,&ip2, &ip3, &ip4)==-1)
	{
		Dprintf(D_CACHE, "ip source error:%s\n",ip_from);
		return -1;
	}
	//Dprintf(D_CACHE,"comparing %i.%i.%i.%i\n",ip1,ip2,ip3,ip4);
	if (ipa_range->range==IP)
	{
/*		Dprintf(D_CACHE," to %i.%i.%i.%i\n",ipa_range->start1,
				ipa_range->start2,
				ipa_range->start3,
				ipa_range->start4);*/

		return !(ipa_range->start1==ip1&&
				ipa_range->start2==ip2&&
				ipa_range->start3==ip3&&
				ipa_range->start4==ip4);
	}
	short ip[4]={ip1,ip2,ip3,ip4};
	short ip_start[4]={ipa_range->start1,ipa_range->start2,ipa_range->start3,ipa_range->start4};
	short ip_end[4]={ipa_range->end1,ipa_range->end2,ipa_range->end3,ipa_range->end4};
	/*Dprintf(D_CACHE,"%i.%i.%i.%i->%i.%i.%i.%i\n", ip_start[0],ip_start[1],ip_start[2],ip_start[3],
			ip_end[0],ip_end[1],ip_end[2],ip_end[3]
	);*/
	int range_start;
	if (ipa_range->start1!=ipa_range->end1)
	{
		range_start=0;
	}
	else if (ipa_range->start2!=ipa_range->end2)
	{
		range_start=1;
	}
	else if (ipa_range->start3!=ipa_range->end3)
	{
		range_start=2;
	}
	else if (ipa_range->start4!=ipa_range->end4)
	{
		range_start=3;
	}
	else
	{
		range_start=4;
	}
	//Dprintf(D_CACHE,"range_start:%i\n",range_start);
	int i=range_start;

	int j;
	for (j=0;j<range_start;j++)
	{
		if (ip[j]!=ip_start[j])
		{
			//range's first same bits must be the same as the requested ip
			return -1;
		}
	}

	for (;i<4;i++)
	{
		if (ip_start[i]>ip[i])
		{
			return -1;
		}
		if (ip_end[i]<ip[i])
		{
			return -1;
		}//out of range in the current bit
		if (ip_start[i]<ip[i] && ip_end[i]>ip[i])
		{
			return 0;
		}//in the middle
		if (ip_start[i]==ip[i])//on the edge...
		{
			//check next bit
			//Dprintf(D_CACHE,"checking top\n");
			return check_edge(i+1,TOP,ip_start,ip);
		}
		if (ip_end[i]==ip[i])
		{
			//Dprintf(D_CACHE,"checking bottom\n");
			return check_edge(i+1,BOTTOM,ip_end,ip);
		}
		//break;
	}
	return 0;

}
dictionary * dict;


void load_configuration()
{
	//read file

	dict = iniparser_load(config_s);
        
	if (dict==NULL)
	{
		fprintf(stderr,"Cannot load configuration file %s\n",config_s);
		exit(-1);
	}

	num_apps = iniparser_getint(dict, "apps:count" ,0);




	
	//default_weight=iniparser_getint(dict,"proxy:performance_interval",5);
	performance_interval= iniparser_getint(dict,"proxy:performance_interval",5);


	fprintf(stderr, "app_count=%i\n",num_apps);
	fprintf(stderr, "interval=%i\n",performance_interval);
        app_stats = (struct app_statistics *) malloc(num_apps*sizeof(struct app_statistics));
	memset(app_stats,0, sizeof(struct app_statistics)*num_apps);

	//default
	int i;
	iedf_deadlines = (int*) malloc(num_apps*sizeof(int));
	memset(iedf_deadlines,0,num_apps*sizeof(int));

	iedf_deadlines_timeval = (struct timeval*) malloc(num_apps*sizeof(struct timeval));
	memset(iedf_deadlines_timeval, 0, num_apps*sizeof(struct timeval));

	average_resp_time = (int*) malloc(num_apps*sizeof(int));
	memset(average_resp_time, 0, num_apps*sizeof(int));
	for (i=0;i<num_apps;i++)
	{

		app_stats[i].stream_id=0;
		//prepare section strings
		char* curr_app=(char*)malloc(6);//'appxx\0'

		sprintf(curr_app,"app%i",i+1);

		char* app_name_string = (char*)malloc(strlen(curr_app)+6);//'apps:appxx\0'

		sprintf(app_name_string,"apps:%s",curr_app);
		app_stats[i].app_name=iniparser_getstring(dict, app_name_string ,NULL);
		Dprintf(D_CACHE,"current name:%s\n",app_stats[i].app_name);
		int app_len=strlen(curr_app);
		int weights_len=strlen("weights");
		char* curr_weight=(char*)malloc(2+weights_len+app_len);//'weights:appxx\0'
		sprintf(curr_weight,"weights:%s",curr_app);


		Dprintf(D_CACHE,"processing %s\n",curr_app);
		app_stats[i].app_weight=iniparser_getint(dict, curr_weight ,0);

		if (app_stats[i].app_weight==0)
		{
			fprintf(stderr,"Error app%i weight:0\n",i);
			exit(-1);
		}
		total_weight+=app_stats[i].app_weight;
		Dprintf(D_CACHE,"weight:%i\n",app_stats[i].app_weight);
		int location_len=strlen("locations");
		char* curr_location=(char*)malloc(app_len+2+location_len);//'appxx_locations\0'
		sprintf(curr_location,"%s_locations",curr_app);

		char* location_count=(char*)malloc(strlen(curr_location)+2+5);//'appxx_locations:count\0'
		char* location_string,*location_value;
		sprintf(location_count, "%s:count",curr_location);

		int num_locations=iniparser_getint(dict, location_count ,0);
		Dprintf(D_CACHE,"location count for %s:%i\n",curr_location, num_locations);
		int j;

		PINT_llist_p current_application_item = PINT_llist_new();
		app_stats[i].application_locations=current_application_item;

		for (j=0;j<num_locations;j++)
		{
			location_string=(char*)malloc(strlen(curr_location)+12);//'appxx_locations:locationxx\0'
			sprintf(location_string,"%s:location%i",curr_location,j+1);
			Dprintf(D_CACHE,"%s\n",location_string);
			location_value=iniparser_getstring(dict, location_string ,NULL);
			Dprintf(D_CACHE,"location%i:%s\n",j+1,location_value);
			split_into_ranges(i,location_value,current_application_item);

		}
		int count=0;
		PINT_llist_p p = current_application_item->next;
		while (p!=NULL)
		{
			count++;
			Dprintf(D_CACHE,"%i item\n",count);
			struct ip_application* ipa=p->item;
			char* tmp_str=ipa->supplied_value;

			char* tmp_str2 = (char*)malloc(strlen(tmp_str)+1);
			strcpy(tmp_str2,tmp_str);
			if (ipa->range==RANGE)
			{
				set_start_end(tmp_str2,"-",ipa);
			}
			else
			{
				set_IP_numbers(tmp_str2,&ipa->start1,&ipa->start2,&ipa->start3,&ipa->start4);
			}
			ipa->weight=app_stats[i].app_weight;
			p=p->next;
		}
		PINT_llist_doall(current_application_item,print_ip_app_item);

	}
	fprintf(stderr,"total_weight:%i\n", total_weight);

	if (scheduler_on)
	{
		(*(static_methods[scheduler_index]->sch_load_data_from_config))(dict);
	}

	iniparser_freedict(dict);

	/*#
	 * set initial scheduler/specific values
	 * #*/



}


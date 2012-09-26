/*
 * config.h
 *
 *  Created on: Oct 4, 2010
 *      Author: yiqi
 */

#ifndef CONFIG_H_
#define CONFIG_H_
#include "llist.h"
#include <time.h>
extern int num_apps;
extern struct app_statistics * app_stats;
struct app_statistics
{
    int app_index;
    int app_weight;
    int app_response;
    int app_rate;
    int app_nr;

    int received_requests;
    int completed_requests;
    int dispatched_requests;
    int stream_id;

    char * app_name;
    PINT_llist_p application_locations;
    int byte_counter;
    int req_go;
    int req_come;
    int req_delay;
    long long app_throughput;
    int diff;
    int rest;
    int app_exist;

};

enum range_type{IP,RANGE,HOSTNAME};
struct ip_application
{
	int app_index;
	enum range_type range;
	char* supplied_value;
	short start1,start2,start3,start4,end1,end2,end3,end4;
	int weight;
	int deadline;
	struct timeval deadline_timeval;
};

struct ip_application* ip_weight(char* ip);
#endif /* CONFIG_H_ */

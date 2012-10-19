/*
 * (C) 2009-2012 Florida International University
 *
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * See COPYING in top-level directory.
 *
 */


#ifndef PERFORMANCE_H_
#define PERFORMANCE_H_
#include <pthread.h>


extern int performance_interval;
extern pthread_mutex_t counter_mutex;
extern int first_time;
extern struct timeval last_count_time, first_count_time;
struct performance_sta
{
	float throughput;
	int req_go;
	int req_come;
	int req_delay;
	int block_count;
};
extern int* average_resp_time;
#endif /* PERFORMANCE_H_ */

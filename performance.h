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
	long iops;
	int req_go;
	int req_come;
	int req_delay;
	int block_count;
};
extern int* average_resp_time;
#endif /* PERFORMANCE_H_ */

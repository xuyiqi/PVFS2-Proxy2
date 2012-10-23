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

#include "scheduler_main.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define __STATIC_SCHEDULER_SFQD__ 1
#define __STATIC_SCHEDULER_DSFQ__ 1
#define __STATIC_SCHEDULER_VDSFQ__ 1
#define __STATIC_SCHEDULER_SFQD2__ 1
#define __STATIC_SCHEDULER_SFQD3__ 1
#define __STATIC_SCHEDULER_VDSFQ2__ 1
#define __STATIC_SCHEDULER_2LSFQD__ 1
#define __STATIC_SCHEDULER_SFQD_FULL__ 1
//define a default scheduler to use
char* chosen_scheduler=SCHEDULER_DEFAULT;
int scheduler_index=0;
int io_purecost=1;
/*this interface describes a generic scheduler implementation that does not distinguish between reads and writes*/

FILE* depthtrack;

int scheduler_main_init()
{
	FILE* depthtrack = stderr;
}
/*
 * Static list of defined schedulers.  These are pre-compiled into
 * the server side proxy. put your own implemented scheduler inside.
 */
#ifdef __STATIC_SCHEDULER_SFQD__
extern struct scheduler_method sch_sfqd;

#endif
#ifdef __STATIC_SCHEDULER_FIFO__
extern struct scheduler_method sch_fifo;
#endif
#ifdef __STATIC_SCHEDULER_DSFQ__
extern struct scheduler_method sch_dsfq;
#endif
#ifdef __STATIC_SCHEDULER_TEST__
extern struct scheduler_method sch_test;
#endif

#ifdef __STATIC_SCHEDULER_VDSFQ__
extern struct scheduler_method sch_vdsfq;
#endif

#ifdef __STATIC_SCHEDULER_SFQD2__
extern struct scheduler_method sch_sfqd2;
#endif

#ifdef __STATIC_SCHEDULER_SFQD3__
extern struct scheduler_method sch_sfqd3;
#endif

#ifdef __STATIC_SCHEDULER_VDSFQ2__
extern struct scheduler_method sch_vdsfq2;
#endif

#ifdef __STATIC_SCHEDULER_2LSFQD__
extern struct scheduler_method sch_2lsfqd;
#endif

#ifdef __STATIC_SCHEDULER_SFQD_FULL__
extern struct scheduler_method sch_sfqd_full;
#endif


/*
 * registering compiled/supported scheduler_methods into a list
 * so that we can choose one from them before the proxy starts
 *
 * */

struct scheduler_method * static_methods[] = {
#ifdef __STATIC_SCHEDULER_SFQD__
    &sch_sfqd,
#endif
#ifdef __STATIC_SCHEDULER_FIFO__
    &sch_fifo,
#endif
#ifdef __STATIC_SCHEDULER_DSFQ__
    &sch_dsfq,
#endif
#ifdef __STATIC_SCHEDULER_TEST__
    &sch_test,
#endif
#ifdef __STATIC_SCHEDULER_VDSFQ__
    &sch_vdsfq,
#endif

#ifdef __STATIC_SCHEDULER_SFQD2__
    &sch_sfqd2,
#endif
#ifdef __STATIC_SCHEDULER_SFQD3__
    &sch_sfqd3,
#endif
#ifdef __STATIC_SCHEDULER_VDSFQ2__
    &sch_vdsfq2,
#endif

#ifdef __STATIC_SCHEDULER_2LSFQD__
    &sch_2lsfqd,
#endif

#ifdef __STATIC_SCHEDULER_SFQD_FULL__
    &sch_sfqd_full,
#endif

    NULL
};




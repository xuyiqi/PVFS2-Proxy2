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

#ifndef COST_MODEL_HISTORY_H
#define	COST_MODEL_HISTORY_H

#define COST_MODEL_NAME "WEIGHTED HISTORY"
#define COST_MOST_RECENT_SLOT 10
#define COST_MEDIUM_RECENT_SLOT 20
#define COST_MOST_DISTANT_SLOT 30
#define COST_TOTAL_HISTORY (COST_MOST_RECENT_SLOT+COST_MEDIUM_RECENT_SLOT+COST_MOST_DISTANT_SLOT)
//known as the window size
#define COST_NUM_PERIODS 3

#define COST_MOST_RECENT_WEIGHT 85
#define COST_MEDIUM_RECENT_WEIGHT 10
#define COST_MOST_DISTANT_WEIGHT 5
#define COST_TOTAL_WEIGHT (COST_MOST_RECENT_WEIGHT+COST_MEDIUM_RECENT_WEIGHT+COST_MOST_DISTANT_WEIGHT)

#define IDLE_THRESHOLD 10000 //seconds

#define COST_MODEL_NONE		0
#define COST_MODEL_GPA		1
#define COST_MODEL_STDEV	2
#define COST_MODEL_EXPSMTH	3
#define COST_MODEL_DEFAULT COST_MODEL_NONE

#define COST_EXPSMTH_ALPHA 0.5
#define COST_EXPSMTH_BETA 0.5

extern int * max_resp_r, *max_resp_w;
extern FILE *actual_output, *expected_output_gpa, *expected_output_exp;
extern int cost_model;
extern struct timeval * last_request;
extern int error_r_machine, error_w_machine;
extern int error_r_app, error_w_app;
extern int error_r_client, error_w_client;
#endif	/* COST_MODEL_HISTORY_H */


/*
 * (C) 2009-2012 Florida International University
 *
 * Laboratory of Virtualized Systems, Infrastructure and Applications (VISA)
 *
 * See COPYING in top-level directory.
 *
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


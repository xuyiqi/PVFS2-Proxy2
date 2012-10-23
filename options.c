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


#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <getopt.h>
#include "proxy2.h"
#include "logging.h"
#include "scheduler_SFQD.h"
#include "scheduler_main.h"
#include "cost_model_history.h"

char * log_prefix = NULL;
extern char* pvfs_config;
extern int enable_first_receive;
int parse_options (int argc, char **argv, struct proxy_option* option_p) {
    int c;
    int digit_optind = 0;
    static int flag=0;
    active_port=atoi(default_port);
    char* log_str=NULL;
    while (1) {
        int this_option_optind = optind ? optind : 1;
        //printf("optind: %i, this_option_optind: %i\n",optind, this_option_optind);
        int option_index = 0;
        static struct option long_options[] = {
            {"port", required_argument, 0, 'p'},
            {"buffer", required_argument, 0, 'b'},
            {"delay", required_argument, 0, 'd'},
            {"logging", required_argument, 0, 'l'},
            {"scheduler", required_argument, 0, 's'},
            {"config",required_argument,0,'c'},
            {"stat",required_argument,0,'t'},
            {"cost_method",required_argument,0,'o'},
            {"proxy_config", required_argument, 0, 'x'},
            {"log_prefix", required_argument, 0, 'e'},
            {"cost_model", required_argument, 0, 'm'},
            {"periodical_counter",no_argument,0,'r'},
            {"app_counter",no_argument,0,'a'},
            {"help",no_argument, 0, 'h'},
            {"flag1",0,&flag,11},
            {"flag2",0,&flag,22},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "p:b:d:l:s:c:t:o:x:e:m:rah",
                 long_options, &option_index);
        if (c == -1)
            break;

        fprintf(stderr, "processing %c\n", c);
        switch (c) {
        case 0:
            Dprintf(D_CACHE,"option %s", long_options[option_index].name);
            if (optarg)
                Dprintf(D_CACHE," with arg %s", optarg);
            Dprintf(D_CACHE,"\n");
            break;
        case 'e':
            fprintf(stderr,"option %s", long_options[option_index].name);
            if (optarg)
		{
                fprintf(stderr," with arg %s\n", optarg);
			log_prefix=optarg;
		}
            break;
        case 'm':

        	fprintf(stderr, "option model with value %s\n", optarg);
        	cost_model=atoi(optarg);
        	if (cost_model<0 || cost_model>3)
        	{
        		fprintf(stderr, "model index out of bounds\n");
        		exit(0);
        	}

        	break;
        case 'p':
            Dprintf(D_CACHE,"option port with value '%s'\n", optarg);
    		int this_port=atoi(optarg);
    		if (this_port<=1024 || this_port >=65535)
    		{
    			Dprintf(D_CACHE,"Error port number: %i (1024,65535)\n",this_port);
    			exit(0);
    		}
    		else
    		{
    			active_port=this_port;
    		}
            break;
        case 'c':
        	Dprintf(D_CACHE,"option config with value '%s'\n", optarg);
        	config_s=optarg;
        	break;
        case 'x':
        	Dprintf(D_CACHE,"option proxy-config with value '%s'\n", optarg);
        	pvfs_config=optarg;
        	break;
        case 'b':
            Dprintf(D_CACHE,"option buffer with value '%s'\n",optarg);

    		int this_buffer=atoi(optarg);
    		if (this_buffer<1024 || this_buffer >10485760)
    		{
    			Dprintf(D_CACHE,"Error buffer size: %i [1024,10485760]\n",this_buffer);
    			exit(0);

    		}
    		else
    		{
    			//out_buffer_size=this_buffer;//out buffer is the only buffer we're using now.,
    		}

            break;

       case 'l':
            Dprintf(D_CACHE,"option logging with value '%s'\n", optarg);



        	log_str=optarg;
            break;
       case 't':
           Dprintf(D_CACHE,"option dump with value '%s'\n", optarg);
    	   dump_file=optarg;
    	   break;

       case 's':
    	   fprintf(stderr,"option scheduler with value '%s'\n",optarg);
    	   if (!strcasecmp(optarg,"none"))
    	   {
    		   scheduler_on=0;
    		   scheduler_index=-1;
    		   Dprintf(D_CACHE,"Scheduler disabled\n");
    		   break;
    	   }

    	   //go through static_methods, compare optarg with method[i]->methodname
    	   int i;
    	   int got=0;
    	   scheduler_index=-1;
    	   fprintf(stderr,"scheduler count is %i\n",SCHEDULER_COUNT);
    	   for (i=0;i<SCHEDULER_COUNT;i++)
    	   {
    		   fprintf(stderr, "comparing with %s\n", static_methods[i]->method_name);
    		   if (!strcasecmp(static_methods[i]->method_name, optarg))
    		   {
    			   got=1;
    			   scheduler_index=i;
        		   Dprintf(D_CACHE,"Scheduler is %i, %s\n",scheduler_index, optarg);
    			   break;
    		   }
    	   }
    	   if (!got)
    	   {
    		   fprintf(stderr, "Selected scheduler not supported:%s\n",optarg);
    		   exit(0);
    	   }

    	   break;

       case 'd':
            Dprintf(D_CACHE,"option delay with value '%s'\n", optarg);


    		int this_delay=atoi(optarg);
    		if (this_delay<-1 || this_delay >1000)
    		{
    			fprintf(stderr,"Error delay (milliseconds): %i [-1,1000]\n",this_delay);

    		}
    		else
    		{
   				poll_delay=this_delay;//out buffer is the only buffer we're using now.,
    		}
            break;
       case 'o':
    	   Dprintf(D_CACHE,"option pure_cost with value '%s'\n", optarg);
    	   if (!strcmp(optarg, "LENGTH"))
    	   {
    		   	  cost_model=COST_MODEL_NONE;
    	   }
    	   else if  (!strcmp(optarg, "GPA"))
    	   {
				cost_model=COST_MODEL_GPA;
    	   }
    	   else if (!strcmp(optarg, "STDEV"))
    	   {
    		   cost_model=COST_MODEL_STDEV;
    	   }
    	   else if (!strcmp(optarg, "COMPLEX"))
    	   {
    		   fprintf(stderr,"Complex cost not supported!\n");
    		   exit(-1);
    	   }
    	   else
    	   {
    		   fprintf (stderr,"Not recognized cost method!\n");
    		   exit(-1);
    	   }


       	   break;
       case 'h':
    	    fprintf(stderr,"Usage: %s --port=%s --buffer=%i --logging=%s --delay=%i --scheduler=SFQD|DSFQ|none --config='%s' --counter=%i --stat='%s' --cost_method=%s --proxy_config=/etc/pvfs2-fs.conf\n",
    	    		argv[0],default_port, -1,"none",5, config_s, counter_start,dump_file, "LENGTH/GPA/STDEV/COMPLEX");
    	    exit(0);
    	    break;
       case 'r':
    	    counter_start=1;
            fprintf(stderr,"option counter enabled\n");

    	    break;
       case 'a':
    	    enable_first_receive=1;


    	    break;
       case '?':
            break;

       default:
            Dprintf(D_CACHE,"?? getopt returned character code 0%o ??\n", c);
        }
    }

   if (optind < argc) {
        Dprintf(D_CACHE,"non-option ARGV-elements: ");
        while (optind < argc)
            Dprintf(D_CACHE,"%s ", argv[optind++]);
        Dprintf(D_CACHE,"\n");
    }
   fprintf(stderr,"========================\n");
	fprintf(stderr,"Using port number: %i\n",active_port);
	fprintf(stderr,"Using buffer size: %i\n",-1);
	fprintf(stderr,"Using logging: %s\n",log_str);//logging defines whether debugging output is enabled, currently this feature is disabled
	if (log_str){
		log_open("proxy2",0);
		enable_logging(log_str);
		//enable_logging(more_str);
	}
	fprintf(stderr,"Using polling delay: %i\n",poll_delay);
	fprintf(stderr,"Using scheduler:%i\n",scheduler_on);
	if (scheduler_on)
		fprintf(stderr,"Scheduler index is %i, %s\n",scheduler_index, static_methods[scheduler_index]->method_name);
	fprintf(stderr,"Using config file: %s\n",config_s);
	fprintf(stderr,"Using dump file: %s\n",dump_file);
	fprintf(stderr,"Using cost model: %i\n",cost_model);
	fprintf(stderr,"Using counter: %i\n",counter_start);
	fprintf(stderr,"Using server config:%s\n",pvfs_config);
	fprintf(stderr,"Counting all apps immediately:%i\n",enable_first_receive);
	fprintf(stderr,"Alpha for exponential smoothing: %f\n", COST_EXPSMTH_ALPHA);
	fprintf(stderr,"========================\n");

}

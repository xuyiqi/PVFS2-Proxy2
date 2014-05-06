/*
 * metarates
 *
 * A program that measures aggregate metadata transaction rates.
 *
 * Bill Anderson
 * 18Oct2004
 *
 * metarates
 * Copyright 2004, Scientific Computing Division, University Corporation 
 * for Atmospheric Research
 */
#include "mpi.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <utime.h>
#include "metarates.h"
//#include <timer.h>
#define MAXPATHLENGTH 256
//#define MPI_WTIME_IS_GLOBAL 1
char**	pname;
char    tmpName[MAXPATHLENGTH];

int 
main(int argc, char* argv[])
{
	int		numfiles = 0;
	int		numfilesPerProc;
	int		npes;
	int		node, npe;
	int		i, j;
	int		ival, fd, c;
	int		unique = 0;
	int		removeFiles = 1;
	int		doCreates = 0;
	int		doStats   = 0;
	int		doUtimes  = 0;
	double		time0, time1, timen1, ctime, ctmax, ctmin, ctsum, ctavg, cttot, sctmax, sctmin;
	char*		dirname;
	extern char	*optarg;
	extern int	optind, opterr, optopt;
	char 		opts[] = "d:un:CSUksr:o:p:g:";
	char*		progname = argv[0];
	struct stat	statbuf;
	int		fsyncOnClose = 0;
	int		repetitions = 1; 
	int		gnumber = 0;
	int		ngroups = 2;
	int		processes = 64;
	int		nnodes = 4;
	MPI_Group gmain,g1,g2,gmy;
	MPI_Init(&argc,&argv);
	MPI_Comm_group(MPI_COMM_WORLD, &gmain);
	MPI_Comm_size(MPI_COMM_WORLD,&npe);
	MPI_Comm_rank(MPI_COMM_WORLD,&node);
	MPI_Comm mycomm;
	processes = npe;
//	MPI_Group * g1 = (MPI_Group *)malloc(sizeof(MPI_Group));
//	MPI_Group * g2 = (MPI_Group *)malloc(sizeof(MPI_Group)); 

		
 
	/*
	 * get input parameters
	 */
	if ( argc <= 2 ) {
		describe(progname);
		exit(1);
	}
  
	while ( ( c = getopt(argc,argv,opts) ) != -1 ) {
		switch (c) {
		case 'r':
			repetitions = atoi(optarg);
			break;
		case 'd':
			dirname = optarg;
			break;
		case 'u':
			unique = 1;
			break;
		case 'n':
			numfiles = atoi(optarg);
			break;
		case 's':
			fsyncOnClose = 1;
			break;
		case 'k':
			removeFiles = 0;
			break;
		case 'C':
			doCreates = 1;
			break;
		case 'S':
			doStats = 1;
			break;
		case 'U':
			doUtimes = 1;
			break;
		case 'g':
			ngroups = atoi(optarg);
			break;
		case 'p':
			processes = atoi(optarg);
			if (node == 0)
	                        printf("processes overridden from %d by %d\n",npe, processes);
			break;
		case 'o':
			nnodes = atoi(optarg);
			break;
		case '?':
		case ':':
			describe(progname);
			exit(4);
		}
	}

	int** ranks = (int**)malloc(ngroups*sizeof(int*));
	int * startranks = (int*)malloc(ngroups*sizeof(int));
	int ranksize = processes/ngroups;
	for (j=0; j<ngroups; j++)
	{
		ranks[j] = (int *)malloc(ranksize*sizeof(int));
		int step = nnodes/ngroups;
		startranks[j] = j*step;
		int startrank = startranks[j];
		int k;
		for (k=0;k<ranksize;k+=step)
		{
			int l;
			
			for (l=0;l<step;l++)
			{
				ranks[j][k+l]= startrank++;
			}
			startrank+=(nnodes-step);
			
		}
		if (node == 0)
		{
			for (k=0;k<ranksize;k++)
			{
				printf("%d ", ranks[j][k]);
			}
			printf("\n");
		}
	}
	
//        int ranks1[] = {0,1,2,3,8,9,10,11,16,17,18,19,24,25,26,27,32,33,34,35,40,41,42,43,48,49,50,51,56,57,58,59};
//        int ranks2[] = {        4,5, 6, 7,12,13,14,15,20,21,22,23,28,29,30,31,36,37,38,39,44,45,46,47,52,53,54,55,60,61,62,63};
        //if ()
        //{

  
	npes = npe;
  
	numfilesPerProc = numfiles / npes;
	if (node == 0) {
		if ((numfilesPerProc*npes) != numfiles) {
			fprintf(stderr, "error: number of files, %d, not "
				"divisible by number of processes %d\n", 
				numfiles, npes);
			MPI_Finalize();
			exit(1);
		}
	}
  
	if (node == 0) {	
		printf("\n\nrun parameters:\n");
		printf("dirname = %s\n", dirname);
		printf("total number of files used in tests = %d\n", numfiles);
		printf("number of files used per process in tests = %d\n", 
			numfilesPerProc);
		printf("repetitions = %d\n", repetitions);
		printf("groups = %d\n", ngroups);
		printf("nodes = %d\n", nnodes);
		printf("processes = %d\n", processes);
		if (unique)
			printf("unique subdirectories will be used by each "
				"process\n");
		else
			printf("a common directory will be used by all "
				"processes\n");
		if (removeFiles)
			printf("test files will be removed between tests and "
				"at the end\n");
		else
			printf("test files will NOT be removed between tests "
				"or at the end\n");
		if (doCreates) {
			printf("file creation rates will be measured\n");
			if (fsyncOnClose) {
				printf("\tfsync will be called prior to close "
					"call\n");
			} else {
				printf("\tfsync will not be called prior to "
					"close call\n");
			}
		}
		if (doStats)
			printf("file stat rates will be measured\n");
		if (doUtimes)
			printf("file utime rates will be measured\n");
	}

                int ierr = 0;
                ierr = MPI_Group_incl(gmain, ranksize, ranks[0], &g1);
                if (ierr != MPI_SUCCESS)
                {
                        printf("joining group1 failed\n");
                        return;
                }
                if (ngroups == 2)
                {
                        ierr = MPI_Group_incl(gmain, ranksize, ranks[1], &g2);
                        if (ierr != MPI_SUCCESS)
                        {
                                printf("joining group2 failed\n");
                                return;
                        }
                }
        if (node % nnodes <= nnodes/ngroups-1 )
        {
                gmy = g1;
                gnumber = 1;
        }
        else
        {
                gmy = g2;
                gnumber = 2;
        }

        ierr = MPI_Comm_create(MPI_COMM_WORLD, gmy, &mycomm);
        if (ierr != MPI_SUCCESS)
        {
                printf("creating my comm failed\n");
        }

	/*
	 * allocate space for path names
	 */
	pname = (char**)malloc(numfilesPerProc*sizeof(char*));
	if (pname == NULL) {
		perror("malloc error");
		MPI_Finalize();
		exit(1);
	}
	for (i = 0; i < numfilesPerProc; i++) 
		pname[i] = NULL;

	/*
	 * if a unique subdirectory should be used for each process,
	 * create them
	 */
	if (node == 0) 
		if (unique)
		{
			printf("Creating Unique Subdirs...\n");
			createSubdirs(npes, dirname);
		}

	/*
	 * obtain file list
	 */
	if (node == 0) printf("Getting filelist...\n");
	getFileList(pname, node, unique, numfilesPerProc, dirname);

	if (doCreates) {
	        MPI_Barrier(MPI_COMM_WORLD);
		if (node == 0 ) printf("Starting Create...\n");	
		/*
		 * measure file creation rates
		 */
		ival  = initsec();

		for (j = 0; j < repetitions; j++)
		{
			MPI_Barrier(mycomm);
			time0 = secondr();
			//printf ("node %i starting at %f, %f\n", node, time0, timen1);
			for (i = 0; i < numfilesPerProc; i++) {
				if ((fd = creat(pname[i], 0666)) < 0) {
					fprintf(stderr, "return value = %d "
						"when trying to creat file "
						"%s.\n", fd, pname[i]);
					perror("");
					MPI_Finalize();
					exit(1);
				}
				
				if (fsyncOnClose) {
					if ((ival = fsync(fd)) < 0) {
						fprintf(stderr, "return value "
							"= %d when trying to "
							"fsync file %s.\n",
							ival, pname[i]);	
						perror("");
						MPI_Finalize();
						exit(1);
					}
				}
				if ((ival = close(fd)) < 0) {
					fprintf(stderr, "return value = %d "
						"when trying to close file "
						"%s.\n", ival, pname[i]);
					perror("");
					MPI_Finalize();
					exit(1);
				}
                                /*timen1 = secondr();
                                if (timen1-time0 > 30)
				{
					printf ("exiting...\n");
				}*/


			}
   
			time1 = secondr();
			ctime = time1 - time0;
   		
			/*
			 * communicate times 
			 */
   
			MPI_Reduce(&ctime, &ctmax, 1, MPI_DOUBLE, MPI_MAX, 0, 
				mycomm);
			MPI_Reduce(&ctime, &ctmin, 1, MPI_DOUBLE, MPI_MIN, 0, 
				mycomm);
			MPI_Reduce(&ctime, &ctsum, 1, MPI_DOUBLE, MPI_SUM, 0, 
				mycomm);
   
			if (node == startranks[0] || node == startranks[1]) {
				sctmin = ctmin;
				ctmin = (double)(numfilesPerProc)/ctmin;
				sctmax = ctmax;
				ctmax = (double)(numfilesPerProc)/ctmax;
				ctavg = ((double)numfilesPerProc)*npes/ctsum;
				cttot = ctmax*npes;
				printf("node %i max = %f ccps, min = %f ccps, avg %f "
					"ccps, total = %f ccps (total time %f, "
					"average time %f, l time %f, s time %f, diff %f)\n",
					node,ctmin, ctmax, ctavg, cttot, ctsum,
					ctsum/npes, sctmax, sctmin, sctmax-sctmin);
	    		}
   		}
		if (removeFiles) {
			/*
			 * remove files
			 */
			for (i = 0; i < numfilesPerProc; i++) {
				if ((ival = unlink(pname[i])) < 0) {
					fprintf(stderr, "return value "
						"= %d when trying to "
						"unlink file %s.\n",
						ival, pname[i]);
					perror("");
					MPI_Finalize();
					exit(1);
				}
			}
    		}
	}

  
	if (doStats) {

		/* 
		 * process 0 will create all the files and then all 
		 * processes will stat their files
		 */
		if (node == 0) {
			printf("Preparing Stat...\n");
			for (i = npes-1; i >= 0; i--) {
				getFileList(pname, i, unique, 
						numfilesPerProc, 
						dirname);
				/*
				 * create the files to be used for 
				 * the stat tests
				 */
				for (j = 0; j < numfilesPerProc; j++) {
					if ((ival = creat(pname[j], 
							0666)) < 0) {
						fprintf(stderr, 
							"return value "
							"= %d when "
							"trying to "
							"creat file "
							"%s.\n",
							ival, pname[j]);
						perror("");
						MPI_Finalize();
						exit(1);
					}
					if ((ival = close(ival)) < 0) {
						fprintf(stderr, 
							"return value "
							"= %d when "
							"trying to "
							"close file %s.\n",
							ival, pname[i]);
						perror("");
						MPI_Finalize();
						exit(1);
					}
				}
			}
		}
   
		/*
		 * measure stat rates
		 */

		MPI_Barrier(MPI_COMM_WORLD);
		if (node == 0 ) printf("Starting Stat...\n");
		for (j = 0; j < repetitions; j++)
		{
			ival  = initsec();
			MPI_Barrier(mycomm);
			time0 = secondr();
			for (i = 0; i < numfilesPerProc; i++) {
				if ((ival = stat(pname[i], &statbuf)) < 0) {
					fprintf(stderr, "ival = %d when trying to "
						"stat a file %s.\n", ival, pname[i]);
					perror("");
					MPI_Finalize();
					exit(1);
				}
			}
			time1 = secondr();
			ctime = time1 - time0;
  	 
			/*
			 * communicate times 
			 */
  	 
                        MPI_Reduce(&ctime, &ctmax, 1, MPI_DOUBLE, MPI_MAX, 0,
                                mycomm);
                        MPI_Reduce(&ctime, &ctmin, 1, MPI_DOUBLE, MPI_MIN, 0,
                                mycomm);
                        MPI_Reduce(&ctime, &ctsum, 1, MPI_DOUBLE, MPI_SUM, 0,
                                mycomm);

                        if (node == startranks[0] || node == startranks[1]) {
				sctmin = ctmin;
				ctmin = (double)(numfilesPerProc)/ctmin;
				sctmax = ctmax;
				ctmax = (double)(numfilesPerProc)/ctmax;
				ctavg = ((double)numfilesPerProc)*npes/ctsum;
				cttot = ctmax*npes;
				printf("node %d max = %f sps, min = %f sps, avg = %f sps, total = %f sps "
					"(total time %f, average time %f, l time %f, s time %f, diff %f)\n",
					node, ctmin, ctmax, ctavg, cttot, ctsum, ctsum/npes, sctmax, sctmin, sctmax-sctmin);
 				
 	   		}
		}
		if (removeFiles) {
			/*
			 * remove files
			 */
			for (i = 0; i < numfilesPerProc; i++) {
				if ((ival = unlink(pname[i])) < 0) {
					fprintf(stderr, "return value = %d "
						"when trying to unlink file "
						"%s.\n", ival, pname[i]);
					perror("");
					MPI_Finalize();
					exit(1);
				}
	
			}
		}
	}
	
	if (doUtimes) {
		/* 
		 * process 0 will create all the files and then all processes 
		 * will utime their files 
		 */
		
		if (node == 0) {
			printf("Preparing Utime...\n");
			for (i = npes-1; i >= 0; i--) {
				getFileList(pname, i, unique, numfilesPerProc, 
						dirname);
				/*
				 * create the files to be used for the utime 
				 * tests
				 */
				for (j = 0; j < numfilesPerProc; j++) {
					if ((ival = creat(pname[j], 0666)) < 0) {
						fprintf(stderr, "return value "
							"= %d when trying to "
							"creat file %s.\n",
							ival, pname[j]);
						perror("");
						MPI_Finalize();
						exit(1);
					}
					if ((ival = close(ival)) < 0) {
						fprintf(stderr, "return value "
							"= %d when trying to "
							"close file %s.\n",
							ival, pname[i]);
						perror("");
						MPI_Finalize();
						exit(1);
					}
				}
			}
		}

		/*
		 * measure utime rates
		 */
		MPI_Barrier(MPI_COMM_WORLD);
		if (node == 0) printf("Starting Utime...\n");
		for (j = 0; j < repetitions; j++)
		{
			ival  = initsec();
			MPI_Barrier(mycomm);
			time0 = secondr();
			for (i = 0; i < numfilesPerProc; i++) {
			
				if ((ival = utime(pname[i], NULL)) < 0) {
					fprintf(stderr, "ival = %d when trying to "
						"utime a file %s.\n", ival, pname[i]);
					perror("");
					MPI_Finalize();
					exit(1);
				}
			}
			time1 = secondr();
			ctime = time1 - time0;
    
			/*
			 * communicate times 
			 */
    
                        MPI_Reduce(&ctime, &ctmax, 1, MPI_DOUBLE, MPI_MAX, 0,
                                mycomm);
                        MPI_Reduce(&ctime, &ctmin, 1, MPI_DOUBLE, MPI_MIN, 0,
                                mycomm);
                        MPI_Reduce(&ctime, &ctsum, 1, MPI_DOUBLE, MPI_SUM, 0,
                                mycomm);

                        if (node == startranks[0] || node == startranks[1]) {
				sctmin = ctmin;
				ctmin = (double)(numfilesPerProc)/ctmin;
				sctmax = ctmax;
				ctmax = (double)(numfilesPerProc)/ctmax;
				ctavg = ((double)numfilesPerProc)*npes/ctsum;
				cttot = ctmax*npes;
				printf("node %d max rate = %f ups, min = %f ups, avg  %f ups, total = %f ups "
					"(total time %f, average time %f, l time %f, s time %f, diff %f)\n",
					node, ctmin, ctmax, ctavg, cttot, ctsum, ctsum/npes, sctmax, sctmin, sctmax-sctmin);
			}
		}

		if (removeFiles) {
			/*
			 * remove files
			 */
			for (i = 0; i < numfilesPerProc; i++) {
				if ((ival = unlink(pname[i])) < 0) {
					fprintf(stderr, "return value = %d "
						"when trying to unlink file "
						"%s.\n", ival, pname[i]);
					perror("");
					MPI_Finalize();
					exit(1);
				}
			}
		}
	}
	/*
	 * if unique directories were created, remove them 
	 */
	if (node == 0) 
		if (unique && removeFiles) 
			removeSubdirs(npes, dirname);

	MPI_Finalize();
	return 0;
}

/*
 * describe usage of program
 */
void describe(char* progname)
{

	fprintf(stderr, "\t%s -d dirname -n numfiles [-k] [-u] [-C][-U][-S]\n",
		progname);
	fprintf(stderr, "\t measures metadata transaction rates. Options "
		"are:\n");
	fprintf(stderr, "\t-d dirname    specifies the directory where the "
		"test\n\t              files should be written\n");
	fprintf(stderr, "\t-n            specifies the total number of files "
		"\n\t              on which the metadata operation(s) will "
		"be\n\t              performed\n");
	fprintf(stderr, "\t-k            specifies that test files should\n\t"
		"              not be removed between tests or at the end\n");
	fprintf(stderr, "\t-u            specifies that each process will "
		"use\n\t              files in a unique directory; otherwise "
		"all files\n\t              will be created in the same "
		"directory\n");
	fprintf(stderr, "\t-C            specifies that the number of file\n\t"
		"              creates per second should be measured\n");
	fprintf(stderr, "\t-U            specifies that the number of utime\n\t"
		"              calls per second should be measured\n");
	fprintf(stderr, "\t-S            specifies that the number of stat\n\t"
		"              calls per second should be measured\n");
	fprintf(stderr, "\t-s            specifies that fsync should be "
		"called\n\t              called prior to close call when file "
		"creation rates\n\t              are being measured\n");

}

/*------------------------------------------------------------------------
 * getFileList   obtains file list for a particular process 
 *----------------------------------------------------------------------*/
void
getFileList(char** pname, int node, int unique, int numfilesPerProc, 
	    char* dirname)
{
	int	i;

	if (!unique) {
		for (i = 0; i < numfilesPerProc; i++) {
			snprintf(tmpName, sizeof(tmpName), "%s/n%df%d", 
				dirname, node, i);
			if (pname[i] != NULL)
				free(pname[i]);
			pname[i] = (char*)strdup(tmpName);
			if (pname[i] == NULL) {
				perror("strdup error");
				MPI_Finalize();
				exit(1);
			}
		} 
	} else {
		for (i = 0; i < numfilesPerProc; i++) {
			snprintf(tmpName, sizeof(tmpName), "%s/%d/n%df%d", 
				dirname, node, node, i);
			if (pname[i] != NULL)
				free(pname[i]);
			pname[i] = (char*)strdup(tmpName);
			if (pname[i] == NULL) {
				perror("strdup error");
				MPI_Finalize();
				exit(1);
			}
		} 
	}

}

/*------------------------------------------------------------------------
 * createSubdirs   creates a subdirectory for each process
 *----------------------------------------------------------------------*/
void
createSubdirs(int npes, char* dirname)
{
	int	i;

	for (i = 0; i < npes; i++) { 
		snprintf(tmpName, sizeof(tmpName), "%s/%d", dirname, i);
		if (mkdir(tmpName, 0755) != 0) {
			if (errno != EEXIST) {
				fprintf(stderr, "error making directory %s.\n",
					 tmpName);
				perror("");
				MPI_Finalize();
				exit(1);
			}
		}
	}    
}


/*------------------------------------------------------------------------
 * removeSubdirs   removes unique subdirectories
 *----------------------------------------------------------------------*/
void
removeSubdirs(int npes, char* dirname)
{
	int	i;

	for (i = 0; i < npes; i++) { 
		snprintf(tmpName, sizeof(tmpName), "%s/%d", dirname, i);
		if (rmdir(tmpName) != 0) {
			fprintf(stderr, "error removing directory %s.\n", 
				tmpName);
			perror("");
			//MPI_Finalize();
			//exit(1);
		}
	}    
	return;
}

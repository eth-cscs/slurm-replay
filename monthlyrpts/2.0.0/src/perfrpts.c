
/**
 * @file  perfrpt.c
 * @brief perfrpt main.
 */

/**
@mainpage crrpt
@author Nicholas P. Cardo
@author Centro Svizzero di Calcolo Scientifico
@author Swiss National Supercomputer Centre

@version 1.0.0
@date January xx, 2017

@section desc_sec Description:
	perfrpt is designed to run all the monthly performance reports
	for Piz Daint.  

@section ack_sec Acknowledgment: 
        Centro Svizzero di Calcolo Scientifico
        Swiss National Supercomputer Center

@verbatim
                                         ###
                                        ##        ____
                                       _n_n_n_____I_I _________
~~~~~~~~~~~~~~~~~~~~                  (_____________I_I_______I
Modification History                  /ooOOOO OOOOoo   oo  oooo
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Date       Who             What
---------- --------------- -------------------------------------------------
xx.xx.2017 N.P. Cardo      Original Program
@endverbatim
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include "perfrpts.h"

char	*ProgName;	/* Global Program Name     */
char	*ReportDir;	/* Global Report Directory */
int	Flags = 0;	/* Global Flags            */

int 
main(int argc, char **argv)
{
extern	char *optarg;
extern	int  optind;
int	optchr;
time_t	tms,tme,tmt;
struct	tm tmr,*tmp;
int	month,year,dim,dom;

	/*
	 *  The name of this program
	 */
	ProgName = strrchr(argv[0],'/');
	if (ProgName == NULL)
		ProgName = argv[0];
	else
		ProgName++;

	/*
	 *  Initialize the month and year to not being set
	 */
	month = year = -1;

	/*
	 * Parse command-line options
	 */
	while ((optchr=getopt(argc,argv,"y:m:DPGLMTZvVh")) != EOF) {
		switch(optchr) {
			case 'y': year = atoi(optarg);      break;   /* report name          */
			case 'm': month = atoi(optarg) - 1; break;   /* number of records    */

			case 'D': Flags |= PR_DATA;         break;   /* only reproduce data  */
			case 'P': Flags |= PR_PLOT;         break;   /* only reproduce plots */
			case 'G': Flags |= PR_GENERAL;      break;   /* only reproduce plots */
			case 'L': Flags |= PR_LARGE;        break;   /* only reproduce plots */
			case 'M': Flags |= PR_MAIL;         break;   /* email reports        */
			case 'T': Flags |= PR_TIMERPT;      break;   /* UZH Job Time Report  */
			case 'Z': Flags |= PR_UZH;          break;   /* only reproduce plots */
			case 'v': Flags |= PR_VERBOSE;      break;   /* display verbose msgs */

			case 'V': version(ProgName);        exit(0); /* version string       */
			default :                                    /* bad option           */
			case 'h':				     /* help                 */
				usage(ProgName);
				exit(1);
		}
	}

	/*
	 *  Read the configuration file
	 */
	if (rdconfig())
		exit(1);

	/*
	 *  Make sure environment variables are properly set
	 */
	if (getenv("MONTHLYRPTS_RPTS") == NULL) {
		fprintf(stderr,"%s: please load the monthlyrpts module file\n",ProgName);
		exit(1);
	}
	if (getenv("MONTHLYRPTS_PLOTS") == NULL) {
		fprintf(stderr,"%s: please load the monthlyrpts module file\n",ProgName);
		exit(1);
	}

	/*
	 *  If no selection made for data or plotting,
	 *  then do both.
	 */
	if (!(Flags&PR_DATA) & !(Flags&PR_PLOT)) {
		Flags |= PR_DATA;
		Flags |= PR_PLOT;
	}

	/*
	 *  If no grouping selected, then do them all
	 */
	if (!(Flags&PR_GENERAL) & !(Flags&PR_LARGE) & !(Flags&PR_UZH) & !(Flags&PR_TIMERPT)) {
		Flags |= PR_GENERAL;
		Flags |= PR_LARGE;
		Flags |= PR_UZH;
	}

	/*
	 *  If year is not set then use the current year
	 */
	if ((month == -1) || (year == -1)) {
		time(&tms);
		tmp = localtime(&tms);

		if (month == -1)
			month = tmp->tm_mon;

		if (year == -1)
			year = tmp->tm_year + 1900;
	}

	/*
	 *  Build the output report directory path
	 */
	ReportDir = reportdir(year,month);

	/*
	 *  Determine the starting and ending date
	 *  timestamps which are used for searching
	 *  the databases.
	 */
	bzero(&tmr,sizeof(tmr));

	tmr.tm_year = year - 1900;
	tmr.tm_mday = 1;
	tmr.tm_mon  = month;

	tms         = mktime(&tmr);

	tmr.tm_mon += 1;
	tme         = mktime(&tmr);


	/*
	 *  Calculate the number of reporting
	 *  days is the month.  This is used for
	 *  report upper bound days.
	 */
	time(&tmt);

	/* 
	 *  Days In Month (dim)
	 */
	dim  = (tme - tms) / 86400;

	/*
	 *  Day Of Month (dom)
	 */
	if (tmt < tme)
		dom = (tmt - tms)/ 86400;
	else
		dom = dim;

/*
	if (tmt < tme) {
		dim  = (tmt - tms) / 86400;
		dim  = (tme - tms) / 86400;
	} else {
		dim  = (tme - tms) / 86400;
	}
*/

	tmp = localtime(&tmt);

	/*
	 *  Can't run on the first day of the current month
	 *  due to insufficient data.  Need at least 1 full
	 *  day in the month.
	 */
	if ((tmp->tm_mday == 1) && (tmp->tm_mon == month) && ((tmp->tm_year+1900) == year))  {
		fprintf(stderr,"%s: Insufficient data to run on the first day of the month\n",ProgName);
		exit(1);
	}

	/*
	 *  Can't run for future months
	 */
	if ((year > (tmp->tm_year+1900)) || ((year == tmp->tm_year+1900) && month > tmp->tm_mon)) {
		fprintf(stderr,"%s: Insufficient data to run for the future\n",ProgName);
		exit(1);
	}

	/*
	 *  Report header, well sortof
	 */
	VERBOSE {
		printf("%s: Producing report ", ProgName);
		if (Flags & PR_DATA)
			printf("data");

		if ((Flags & PR_DATA) && (Flags & PR_PLOT))
			printf(" and ");

		if (Flags & PR_PLOT)
			printf("plots");

		printf(" for %s %d\n",MONTHS[month+1],year);
	}

	/*
	 *  Get all the jobs
	 */
	if (Flags & PR_DATA) GetJobs(month,year,tms,tme,"ALL");
	if (Flags & PR_DATA) GetProjects(month,year,tms,tme);

	/*
	 *  Produce all the general reports (-G)
	 */
	if (Flags & PR_GENERAL) {
		AvailabilityRpt(month,year,tms,tme,dim,dom);
		UtilizationRpt (month,year,tms,tme,dim,dom);
		AllocationRpt  (month,year,tms,tme,dim,dom);
		JobProfileRpt  (month,year,tms,tme,dim,dom,NULL);
		QueueWaitRpt   (month,year,tms,tme,dim,dom);
		StartCountsRpt (month,year,tms,tme,dim,dom);
		SubmitCountsRpt(month,year,tms,tme,dim,dom);
		EndCountsRpt   (month,year,tms,tme,dim,dom);
	}

	/*
	 *  Large memory node usage (-L)
	 */
	if (Flags & PR_LARGE) {
		LargeMemSubmitCountsRpt(month,year,tms,tme,dim,dom);
		LargeMemStartCountsRpt (month,year,tms,tme,dim,dom);
		LargeMemEndCountsRpt   (month,year,tms,tme,dim,dom);
	}

	/*
	 * Clear the table of jobs previously read
	 * and then get the next batch of jobs.
	 */
	if (Flags & PR_DATA) ClearJobTable();

	/*
	 *  University of Zurich specific reports (-Z)
	 */
	if (Flags & PR_UZH) {
		if (Flags & PR_DATA) GetJobs(month,year,tms,tme,"UZH");
		UZHQueueWaitRpt  (month,year,tms,tme,dim,dom);
		UZHUtilizationRpt(month,year,tms,tme,dim,dom);
		JobProfileRpt (month,year,tms,tme,dim,dom,"UZH");
	}

	if (Flags & PR_TIMERPT) {
		GetJobs(month,year,tms,tme,"UZH");
		UZHTimeRpt(month,year,tms,tme,dim,dom);
	}

	exit(0);
}


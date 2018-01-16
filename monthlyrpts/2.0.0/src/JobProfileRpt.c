/**
@file  JobProfileRpt.c
@brief Job Profile Report
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include "perfrpts.h"

extern	char *ProgName;          /* Program Name                         */
extern	char *ReportDir;         /* Report output directory              */
extern	int   Flags;             /* Program Flags                        */

/**
 * @brief   Job Profile Reports
 * @details Produce Job Profile Report
 * @param   month    Specified month
 * @param   year     Specified year
 * @param   tms      Start timestamp
 * @param   tme      End timestamp
 * @param   dim      Days In Month
 * @param   customer Specific customer report
 */
void 
JobProfileRpt(month,year,tms,tme,dom,dim,customer)
int	month;
int	year;
time_t	tms;
time_t	tme;
int	dom;
int	dim;
char	*customer;
{
int	x;
char	rptname[512];
FILE	*fd;
struct	JobTableEntry *jte;
char	*p;
time_t	tmt;
int	qw;

	if (customer == NULL)
		customer = "";

	if (Flags & PR_DATA) {

		VERBOSE printf("\n%s: %s - generating data file\n",ProgName,__FUNCTION__);

		tmt = tms + (dom * 86400);

		bzero(rptname,sizeof(rptname));
		sprintf(rptname,"%s/%sJobProfile.dat",ReportDir,customer);

		/*
		 *  Open the monthly job profile data file for writing
		 */
		if ((fd=fopen(rptname,"w")) == NULL) {
			fprintf(stderr,"%s: failed to open %s (%d: %s)\n",ProgName,rptname,errno,strerror(errno));
			fprintf(stderr,"%s: File: %s, Function: %s, Line: %d\n",ProgName,__FILE__,__FUNCTION__,__LINE__);
			exit(-1);
		}

		/*
		 *  Write the header to the file
		 */
		fprintf(fd,"#Job Data\n");
		fprintf(fd,"#                                                                (minutes)    (minutes)    (minutes)\n");
		fprintf(fd,"#   JobID       Submit     Eligible        Start         End       Elapsed    TimeLimit    QueueWait Nodes Type Partition\n");
		fprintf(fd," -------- ------------ ------------ ------------ ------------ ------------ ------------ ------------ ----- ---- ---------\n");

		/*
		 *  Reset the index in the job table
		 */
		ResetJobTable();

		/*
		 *  Loop through all jobs
		 */
		while ((jte=GetJobTableEntry()) != NULL) {

			if (jte->jte_end > tmt)
				continue;

			if (jte->jte_nodetype == NODE_GPU)
				p = "GPU";

			else if (jte->jte_nodetype == NODE_MC)
				p = "MC";
			else
				p = "???";	/*  SHOULD NEVER HAPPEN */

			/*
			 *  Queue Wait Time in minutes
			 */
			qw = (jte->jte_start-jte->jte_eligible)/60;

			/*
			 *  Write the entry
			 */
                        fprintf(fd,"%9d %12ld %12ld %12ld %12ld %12ld %12d %12ld %5d %3s %9s\n",
                                jte->jte_jobid,jte->jte_submit,jte->jte_eligible,jte->jte_start,
                                jte->jte_end,jte->jte_elapsed/60,jte->jte_timelimit,
                                (jte->jte_start-jte->jte_eligible)/60,jte->jte_nodes,p,jte->jte_partition);

			/*
			 *  Alert if greater than 10 days
			 */
			if ((qw/60/24) > 10) {
				fprintf(stderr,"%s: extreme queue wait for job %d (%4.1f days)\n",
					ProgName,jte->jte_jobid,(float)qw/60.0/24.0);
			}
		}

		fclose(fd);
	}

	if (Flags & PR_PLOT) {

		VERBOSE printf("%s: %s - producing plots\n",ProgName,__FUNCTION__);

		/*
		*  Produce plots with ploticus, loop on configured plot formats.
		*  Build the command line then execute it.
		*/
		x = 0;
		while (PlotFormats[x] != NULL) {

			/*
			 *  Produce the plot, maxnodes=5000
			 */
			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -maxvector 6000000 -maxfields 6000000 -maxrows 5000000 -%s -o %s/%sJobPlot%d.%s %s/JobPlot.ploticus DATAFILE=%s/%sJobProfile.dat MONTH=%s YEAR=%d MAXNODES=%d NODEINC=260 CUSTOMER=%s",
				PlotFormats[x],ReportDir,customer,5000,PlotFormats[x],PLOTDIR,ReportDir,customer,MONTHS[month+1],year,5000,customer);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/%sJobPlot%d.%s",ReportDir,customer,5000,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			/*
			 *  Produce the plot, maxnodes=260 (zoomed in)
			 */
			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -maxvector 6000000 -maxfields 6000000 -maxrows 5000000 -%s -o %s/%sJobPlot%d.%s %s/JobPlot.ploticus DATAFILE=%s/%sJobProfile.dat MONTH=%s YEAR=%d MAXNODES=%d NODEINC=10 CUSTOMER=%s",
				PlotFormats[x],ReportDir,customer,260,PlotFormats[x],PLOTDIR,ReportDir,customer,MONTHS[month+1],year,260,customer);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/%sJobPlot%d.%s",ReportDir,customer,260,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			/*
			 *  Requested vs Wait Time
			 */
			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -maxvector 6000000 -maxfields 6000000 -maxrows 5000000 -%s -o %s/%sReqWait.%s %s/ReqWait.ploticus DATAFILE=%s/%sJobProfile.dat MONTH=%s YEAR=%d CUSTOMER=%s",
				PlotFormats[x],ReportDir,customer,PlotFormats[x],PLOTDIR,ReportDir,customer,MONTHS[month+1],year,customer);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/%sReqWait.%s",ReportDir,customer,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			/*
			 *  Job Requested Time
			 */
			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -maxvector 6000000 -maxfields 6000000 -maxrows 5000000 -%s -o %s/%sJobReq.%s %s/JobReq.ploticus DATAFILE=%s/%sJobProfile.dat MONTH=%s YEAR=%d CUSTOMER=%s",
				PlotFormats[x],ReportDir,customer,PlotFormats[x],PLOTDIR,ReportDir,customer,MONTHS[month+1],year,customer);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/%sJobReq.%s",ReportDir,customer,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			x++;
		}
	}

	VERBOSE printf("%s: %s - reporting complete\n",ProgName,__FUNCTION__);
}


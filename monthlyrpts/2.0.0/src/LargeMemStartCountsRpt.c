/**
@file  LargeMemStartCountsRpt.c
@brief Large Memory Start Counts per day Report
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
 * @brief   Large Memory Job Start Counts
 * @details Produce Large Memory Job Start Counts per day Report
 * @param   month  Specified month
 * @param   year   Specified year
 * @param   tms    Start timestamp
 * @param   tme    End timestamp
 * @param   dim    Days In Month
 * @param   dom    Day  Of Month
 */
void 
LargeMemStartCountsRpt(month,year,tms,tme,dim,dom)
int	month;
int	year;
time_t	tms;
time_t	tme;
int	dim;
int	dom;
{
int	x,y;
char	rptname[512];
FILE	*fd;
struct	JobTableEntry *jte;
int	counts[11][32];
int	tblqueue;
int	mdom;

	if (Flags & PR_DATA) {
		bzero(counts,sizeof(counts));

		VERBOSE printf("\n%s: %s - sifting throught the data\n",ProgName,__FUNCTION__);

		ResetJobTable();

		/*
		 *  Loop on all jobs
		 */
		while ((jte=GetJobTableEntry()) != NULL) {
			/*
			 * Make sure the job is within the period
			 */
			if (jte->jte_start < tms)
				continue;

			/*
			 *  Limit to only large memory jobs
			 */
			if (jte->jte_mem <= 65000)
				continue;

			mdom = (jte->jte_start-tms)/86400 + 1;	/* what day of the month did this job start */

			/*
			 *  0: normal gpu
			 *  1: normal mc
			 *  2: debug  gpu
			 *  3: debug  mc
			 *  4: high   gpu
			 *  5: high   mc
			 *  6: low    gpu
			 *  7: low    mc
			 *  8: prepost  (mc only)
			 *  9: xfer     (runs off daint)
			 * 10: large    (gpu only)
			 */
			if (!strcmp(jte->jte_partition,"normal")) {
				if (jte->jte_nodetype == NODE_GPU)
					tblqueue = 0;

				else if (jte->jte_nodetype == NODE_MC)
					tblqueue = 1;

			} else if (!strcmp(jte->jte_partition,"debug")) {
				if (jte->jte_nodetype == NODE_GPU)
					tblqueue = 2;

				else if (jte->jte_nodetype == NODE_MC)
					tblqueue = 3;

			} else if (!strcmp(jte->jte_partition,"high")) {
				if (jte->jte_nodetype == NODE_GPU)
					tblqueue = 4;

				else if (jte->jte_nodetype == NODE_MC)
					tblqueue = 5;

			} else if (!strcmp(jte->jte_partition,"low")) {
				if (jte->jte_nodetype == NODE_GPU)
					tblqueue = 6;

				else if (jte->jte_nodetype == NODE_MC)
					tblqueue = 7;

			} else if (!strcmp(jte->jte_partition,"prepost")) {
				tblqueue = 8;

			} else if (!strcmp(jte->jte_partition,"xfer")) {
				tblqueue = 9;

			} else if (!strcmp(jte->jte_partition,"large")) {
				tblqueue = 10;
			}

			/*
			 *  Accumulate the data into the array: counts[QUEUE][DAYOFMONTH]
			 */
			counts[tblqueue][mdom]++;
		}

		VERBOSE printf("%s: %s - generating data file\n",ProgName,__FUNCTION__);

		bzero(rptname,sizeof(rptname));
		sprintf(rptname,"%s/LargeMemStartCounts.dat",ReportDir);

		/*
		 *  Open the monthly data file for writing
		 */
		if ((fd=fopen(rptname,"w")) == NULL) {
			fprintf(stderr,"%s: failed to open %s (%d: %s)\n",ProgName,rptname,errno,strerror(errno));
			fprintf(stderr,"%s: File: %s, Function: %s, Line: %d\n",ProgName,__FILE__,__FUNCTION__,__LINE__);
			exit(-1);
		}

		/*
		 *  Column headings
		 */
		fprintf(fd,"#Number of large memory jobs started:\n");
		fprintf(fd,"#        gpu      mc     gpu      mc     gpu      mc    gpu       mc     mc              gpu\n");
		fprintf(fd,"#dom  normal  normal   debug   debug    high    high    low      low prepost    xfer   large\n");
		fprintf(fd," --- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- -------\n");
		/*
		 *  Loop on each day of the month
		 */
		for (x=1;x<=dom;x++) {
			fprintf(fd,
				" %3d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d\n",
				x,counts[0][x],counts[1][x],counts[2][x],counts[3][x],counts[4][x],counts[5][x],counts[6][x],counts[7][x],counts[8][x],counts[9][x],counts[10][x]);
		}

		fclose(fd);
	}

	if (Flags & PR_PLOT) {
		VERBOSE printf("%s: %s - producing plot\n",ProgName,__FUNCTION__);

		/*
		 *  Produce plots with ploticus, loop on configured plot formats.
		 *  Build the command line then execute it.
		 */
		x =0;
		while (PlotFormats[x] != NULL) {

		        bzero(rptname,sizeof(rptname));
		        sprintf(rptname,
				"module load ploticus; pl -o %s/LargeMemStartCounts.%s -%s %s/LargeMemStartCounts.ploticus DATAFILE=%s/LargeMemStartCounts.dat DIM=%d MONTH=%s YEAR=%d",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dom,MONTHS[month+1],year);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/LargeMemStartCounts.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			x++;
		}
	}

	VERBOSE printf("%s: %s - reporting complete\n",ProgName,__FUNCTION__);
}


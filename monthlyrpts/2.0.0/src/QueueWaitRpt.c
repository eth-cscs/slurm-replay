/**
@file  QueueWait.c
@brief Average Queue Wait Time Report
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
 * @brief   Average Queue Wait Time Report
 * @details Produce Average Queue Wait Time Report
 * @param   month  Specified month
 * @param   year   Specified year
 * @param   tms    Start timestamp
 * @param   tme    End timestamp
 * @param   dim    Days In Month
 * @param   dom    Day  Of Month
 */
void 
QueueWaitRpt(month,year,tms,tme,dim,dom)
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
char	*queues [] = {"normal","debug","high",NULL};
long	counts[6][33];
long	waittm[6][33];
int	mdom;
int	tblqueue;
struct	JobTableEntry *jte;

	if (Flags & PR_DATA) {
		bzero(rptname,sizeof(rptname));
		bzero(counts,sizeof(counts));
		bzero(waittm,sizeof(waittm));

		VERBOSE printf("\n%s: %s - sifting throught the data\n",ProgName,__FUNCTION__);

		ResetJobTable();

		/*
		 * Loop on all jobs
		 */
		while ((jte=GetJobTableEntry()) != NULL) {
			/*
			 *  Make sure the job is within the period
			 */
			if (jte->jte_start < tms)
				continue;

			mdom = (jte->jte_start-tms)/86400 + 1;	/* what day of the month did the job start */

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
			}

			/*
			 *  Accumulate the data into the array: counts/waittm[QUEUE][DAYOFMONTH]
			 */
			counts[tblqueue][mdom] ++;
			waittm[tblqueue][mdom] += (jte->jte_start - jte->jte_eligible)/60;
		}

		VERBOSE printf("%s: %s - generating data file\n",ProgName,__FUNCTION__);

		sprintf(rptname,"%s/QueueWait.dat",ReportDir);
		/*
		*  Open the monthly data file for writing
		*/
		if ((fd=fopen(rptname,"w")) == NULL) {
			fprintf(stderr,"%s: failed to open %s (%d: %s)\n",ProgName,rptname,errno,strerror(errno));
			fprintf(stderr,"%s: File: %s, Function: %s, Line: %d\n",ProgName,__FILE__,__FUNCTION__,__LINE__);
			exit(-1);
		}

		/*
		 *  Column headers
		 */
		fprintf(fd,"#average queue wait time:\n");
		fprintf(fd,"#        gpu      mc     gpu      mc     gpu      mc\n");
		fprintf(fd,"#dom  normal  normal   debug   debug    high    high\n");
		fprintf(fd,"#--- ------- ------- ------- ------- ------- -------\n");

		/*
		 *  Loop on day in month
		 */
		for (x=1;x<=dom;x++) {
			fprintf(fd," %3d",x);
			/*
			 *  Loop on the 6 differnt columns
			 */
			for(y=0;y<6;y++) {
				if (counts[y][x])
					fprintf(fd," %7ld",waittm[y][x]/counts[y][x]/60);
				else
					fprintf(fd," %7d",0);
			}
			fprintf(fd,"\n");
		}

		fclose(fd);
	}

	if (Flags & PR_PLOT) {
		VERBOSE printf("%s: %s - producing plot\n",ProgName,__FUNCTION__);

		/*
		 *  Produce plots with ploticus, loop on configured plot formats.
		 *  Build the command line then execute it.
		 */
		x = 0;
		while (PlotFormats[x] != NULL) {

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -o %s/QueueWait.%s -%s %s/QueueWait.ploticus DATAFILE=%s/QueueWait.dat DIM=%d MONTH=%s YEAR=%d",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dom,MONTHS[month+1],year);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/QueueWait.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			x++;
		}
	}

	VERBOSE printf("%s: %s - reporting complete\n",ProgName,__FUNCTION__);
}


/**
 * @file  UtilizationRpt.c
 * @brief System Utilization Reports
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h> 
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include "perfrpts.h"

extern	char *ProgName;          /* Program Name                         */
extern	char *ReportDir;         /* Report output directory              */
extern	int   Flags;             /* Program Flags                        */
extern	int   DailyNodes[32][4];

/**
 * @brief   Utilization Report
 * @details Produce System Utilization Reports
 * @param   month  Specified month
 * @param   year   Specified year
 * @param   tms    Start timestamp
 * @param   tme    End timestamp
 * @param   dom    Day Of Month
 * @param   dim    Days In Month
 */
void
UtilizationRpt(month,year,tms,tme,dim,dom)
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
int	counts[12][32];
int	tblqueue;
int	mdom;
int	gput,mct;
time_t	tmt,now;
struct	tm *tmp,tml;
float	pcnt;
int	gpumax,mtcmax;
float	gpupcnt,mtcpcnt;
struct	stat stb;

	HoursInMonth(month,year);

	if (Flags & PR_DATA) {
		bzero(counts,sizeof(counts));

		ResetJobTable();

		VERBOSE printf("\n%s: %s - sifting through the data\n",ProgName,__FUNCTION__);

		/*
		 *  Loop on all jobs
		 */
		while ((jte=GetJobTableEntry()) != NULL) {
			/*
			 *  Make sure the job is within the period
			 */
			if (jte->jte_end < tms)
				continue;

			mdom = (jte->jte_end-tms)/86400 + 1;	/* what day of the moth did the job end */

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
			 * 11: all others
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
			} else {
				tblqueue = 11;
			}

			/*
			 *  Skip running jobs
			 */
			if (jte->jte_elapsed < 0 )
				continue;

			/*
			 *  Attempt to distribute the job over the right days
			 *  Only works for 2 days.
			 */
			tmp = localtime(&jte->jte_end);
			memcpy(&tml,tmp,sizeof(struct tm));
			tml.tm_hour = 0;
			tml.tm_min  = 0;
			tml.tm_sec  = 0;
			tmt = mktime(&tml);
	
			if (jte->jte_start < tmt ) {
				pcnt = (float)(jte->jte_end - tmt) / (float)jte->jte_elapsed;

				counts[tblqueue][mdom] += (jte->jte_elapsed * jte->jte_nodes * pcnt);

				if (mdom >1 )
					counts[tblqueue][mdom-1] += (jte->jte_elapsed * jte->jte_nodes * (1-pcnt));
			} else {
				counts[tblqueue][mdom] += jte->jte_elapsed * jte->jte_nodes;
			}
		}

		VERBOSE printf("%s: %s - generating data file\n",ProgName,__FUNCTION__);

		bzero(rptname,sizeof(rptname));
		sprintf(rptname,"%s/Utilization.dat",ReportDir);

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
		fprintf(fd,"#Raw Node Hours Consumed\n");
		fprintf(fd,"#        gpu      mc     gpu      mc     gpu      mc    gpu       mc     mc              gpu     gpu      mc     gpu      mc   gpu    mc\n");
		fprintf(fd,"#dom  normal  normal   debug   debug    high    high    low      low prepost    xfer   large   total   total max hrs max hrs  pcnt  pcnt\n");
		fprintf(fd,"#--- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ----- -----\n");

		time(&now);
		tmp = localtime(&now);

		memcpy(&tml,tmp,sizeof(struct tm));

		tml.tm_mon  = month;
		tml.tm_year = year - 1900;

		/*
		 *  Loop on day in month
		 */
		for (x=1;x<=dom;x++) {

			tml.tm_mday = x;
			tmt = mktime(&tml);
			tmp = localtime(&tmt);
	
			gpumax = DailyNodes[x][0] * 24;
			mtcmax = DailyNodes[x][1] * 24;

			/*
			 *  Accumulate totals
			 */
			gput    = (counts[0][x]+counts[2][x]+counts[4][x]+counts[6][x]+counts[10][x])/3600;
			mct     = (counts[1][x]+counts[3][x]+counts[5][x]+counts[7][x]+counts[8][x])/3600;
			gpupcnt = (float)gput / (float)gpumax * 100.0;
			mtcpcnt = (float)mct  / (float)mtcmax * 100.0;

			fprintf(fd,
				" %3d %7u %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %7d %5.1f %5.1f\n",
				x,
				counts[0][x]/3600,counts[1][x]/3600,counts[2][x]/3600,counts[3][x]/3600,
			        counts[4][x]/3600,counts[5][x]/3600,counts[6][x]/3600,counts[7][x]/3600,
			        counts[8][x]/3600,counts[9][x]/3600,counts[10][x]/3600,
				gput,mct,gpumax,mtcmax,gpupcnt,mtcpcnt);
		}

		fclose(fd);
	}

	if (Flags & PR_PLOT) {
		VERBOSE printf("%s: %s - producing plots\n",ProgName,__FUNCTION__);

		/*
		 *  If there are no recorded events for the month, then
		 *  create an empty file.
		 */
		bzero(rptname,sizeof(rptname));
		sprintf(rptname,"%s/%sEvents.txt",ReportDir,MONTHS[month+1]);

		if (stat(rptname,&stb)) {
			stb.st_size = 0;
			mknod(rptname,S_IFREG|S_IREAD|S_IWRITE,0);
		}

		if (!stb.st_size) {
			fprintf(stderr,"%s: no events have been recorded for %s\n",
				ProgName,MONTHS[month+1]);
		}

		/*
		 *  Produce plots with ploticus, loop on configured plot formats.
		 *  Build the command line then execute it.
		 */
		x=0;
		while (PlotFormats[x] != NULL) {

			/*
			 *  Overall utilization as hours
			 */
			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -o %s/Utilization.%s -%s %s/Utilization.ploticus DATAFILE=%s/Utilization.dat DIM=%d MONTH=%s YEAR=%d EVENTS=%s/%sEvents.txt",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dom,MONTHS[month+1],year,ReportDir,MONTHS[month+1]);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/Utilization.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			/*
			 * Overall utilization as a percentage
			 */
			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -o %s/UtilizationPcnt.%s -%s %s/UtilizationPcnt.ploticus DATAFILE=%s/Utilization.dat DIM=%d MONTH=%s YEAR=%d EVENTS=%s/%sEvents.txt",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dom,MONTHS[month+1],year,ReportDir,MONTHS[month+1]);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/UtilizationPcnt.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			/*
			 * GPU utilization
			 */
			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -o %s/GPUUtilization.%s -%s %s/GPUUtilization.ploticus DATAFILE=%s/Utilization.dat DIM=%d MONTH=%s YEAR=%d",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dom,MONTHS[month+1],year);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/GPUUtilization.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			/*
			 * Multicore utilization
			 */
			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -o %s/MCUtilization.%s -%s %s/MCUtilization.ploticus DATAFILE=%s/Utilization.dat DIM=%d MONTH=%s YEAR=%d",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dom,MONTHS[month+1],year);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/MCUtilization.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			/*
			 * Special utilization report for monthly performance report
			 */
			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -o %s/CSCS_Utilization.%s -%s %s/CSCS_Utilization.ploticus DATAFILE=%s/Utilization.dat DIM=%d MONTH=%s YEAR=%d EVENTS=%s/%sEvents.txt",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dom,MONTHS[month+1],year,ReportDir,MONTHS[month+1]);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/CSCS_Utilization.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			x++;
		}
	}

	VERBOSE printf("%s: %s - reporting complete\n",ProgName,__FUNCTION__);
}


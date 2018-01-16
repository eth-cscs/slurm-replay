/**
@file  UZHUtilizationRpt.c
@brief UZH Utilization Report
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
 * @brief   UZH Utilization Report
 * @details Produce System Utilization Reports
 * @param   month  Specified month
 * @param   year   Specified year
 * @param   tms    Start timestamp
 * @param   tme    End timestamp
 * @param   dim    Days In Month
 */
void
UZHUtilizationRpt(month,year,tms,tme,dim,dom)
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
int	counts[UZHPROJECTCOUNT][32]; /* uzh,dom */
int	mdom;
int	gput,mct;
time_t	tmt;
struct	tm *tmp,tml;
float	pcnt;
char	*p;
int	uzh,uzhp;
int	factor;
int	uzhalloc,uzhalloc_d;
/*int	uzhalloc = 561735;*/	/* allocation for uzhp */
/*int	uzhalloc = 709509;*/	/* allocation for uzhp */
/* int	uzhalloc = 649509;*/	/* allocation for uzhp */

	uzhalloc = GetUZHAlloc();
	
	if (Flags & PR_DATA) {
		bzero(counts,sizeof(counts));

		ResetJobTable();

		VERBOSE printf("\n%s: %s - sifting through the data\n",ProgName,__FUNCTION__);

		/*
		 *  Loop on UZH Jobs
		 */
		while ((jte=GetJobTableEntry()) != NULL) {
			/*
			 *  Make sure the job is within the period
			 */
			if (jte->jte_end < tms)
				continue;

			mdom = (jte->jte_end-tms)/86400 + 1;	/* what day of the month did it end */

			/*
			 *  strip off just the project number.
			 *  format is uzh#
			 */
			p = jte->jte_account;
			p += 3;
			uzh = atol(p);

			/*
			 *  Skip running jobs
			 */
			if (jte->jte_elapsed < 0 )
				continue;

			tmp = localtime(&jte->jte_end);
			memcpy(&tml,tmp,sizeof(struct tm));
			tml.tm_hour = 0;
			tml.tm_min  = 0;
			tml.tm_sec  = 0;
			tmt = mktime(&tml);

			factor = 1;

			if (!strcmp(jte->jte_partition,"high"))
				factor = 2;	/* high jobs charge double */

			if (jte->jte_start < tmt ) {
				pcnt = (float)(jte->jte_end - tmt) / (float)jte->jte_elapsed;

				counts[uzh][mdom] += (jte->jte_elapsed * jte->jte_nodes * pcnt * factor);

				if (mdom >1 )
					counts[uzh][mdom-1] += (jte->jte_elapsed * jte->jte_nodes * (1-pcnt) * factor);
			} else {
				counts[uzh][mdom] += jte->jte_elapsed * jte->jte_nodes * factor;
			}
		}

		VERBOSE printf("%s: %s - generating data file\n",ProgName,__FUNCTION__);

		bzero(rptname,sizeof(rptname));
		sprintf(rptname,"%s/UZHUtilization.dat",ReportDir);

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
		fprintf(fd,"#UZH Hours Consumed\n");
/*
		fprintf(fd,"#dom    uzh0    uzh1    uzh2    uzh3    uzh4    uzh5    uzh6    uzh7    uzh8    uzh9   uzh10   uzh11   uzh12   uzh13   uzh14   uzh15   uzh16   uzh17   uzh18   uzh19    uzhp\n");
		fprintf(fd," --- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- -------\n");
*/
		fprintf(fd,"#dom");

		memset(rptname,0,sizeof(rptname));

		for(x=0;x<UZHPROJECTCOUNT;x++){
			sprintf(rptname,"uzh%d",x);
			fprintf(fd,"   %5.5s",rptname);
		}
		fprintf(fd,"    uzhp\n");

		fprintf(fd," ---");
		for(x=0;x<UZHPROJECTCOUNT+1;x++)
			fprintf(fd," -------");
		fprintf(fd,"\n");

		/*
		 *  Loop on day in month
		 */
		for (x=1;x<=dom;x++) {
			fprintf(fd," %3d",x);
			uzhp = 0;
			/*
			 *  Loop on the UZHPROJECTCOUNT UZH projects
			 */
			for (y=0;y<UZHPROJECTCOUNT;y++) {
				fprintf(fd," %7d",counts[y][x]/3600);
				uzhp += counts[y][x]/3600;
			}

			fprintf(fd," %7d\n",uzhp);
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
			 *  For partial months, estimate the number of days in the month.
			 *  This is needed for the summary report in order to show the
			 *  monthly burn rate.
			 */
			if (dom < 28) {
				switch (month+1) {
					case  1: y = 31; break;
					case  2: y = 28; break;
					case  3: y = 31; break;
					case  4: y = 30; break;
					case  5: y = 31; break;
					case  6: y = 30; break;
					case  7: y = 31; break;
					case  8: y = 31; break;
					case  9: y = 30; break;
					case 10: y = 31; break;
					case 11: y = 30; break;
					case 12: y = 31; break;
				}

			} else {
				y = dom;
			}

			uzhalloc_d = uzhalloc/dom;

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -o %s/UZHSummary.%s -%s %s/UZHSummary.ploticus DATAFILE=%s/UZHUtilization.dat DIM=%d MONTH=%s YEAR=%d MONTHLY=%d DAILY=%d",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dim,MONTHS[month+1],year,uzhalloc,uzhalloc/dim);
/*
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,y,MONTHS[month+1],year,uzhalloc);
*/
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/UZHSummary.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,
				"module load ploticus; pl -o %s/UZHUtilization.%s -%s %s/UZH.ploticus DATAFILE=%s/UZHUtilization.dat DIM=%d MONTH=%s YEAR=%d MONTHLY=%d DAILYGOAL=%d",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dim,MONTHS[month+1],year,uzhalloc,uzhalloc/dim);
/*
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dom,MONTHS[month+1],year,uzhalloc,uzhalloc/30);
*/
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/UZHUtilization.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			x++;
		}
	}

	VERBOSE printf("%s: %s - reporting complete\n",ProgName,__FUNCTION__);
}


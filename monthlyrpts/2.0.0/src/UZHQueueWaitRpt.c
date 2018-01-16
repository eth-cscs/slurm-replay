/**
@file  UZHQueueWaitRpt.c
@brief UZH Queue Wait Report
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
 * @brief   UZH Average Queue Wait Time Report
 * @details Produce UZH Average Queue Wait Time by project Report
 * @param   month  Specified month
 * @param   year   Specified year
 * @param   tms    Start timestamp
 * @param   tme    End timestamp
 * @param   dom    Days Of Month
 */
void 
UZHQueueWaitRpt(month,year,tms,tme,dom,dim)
int	month;
int	year;
time_t	tms;
time_t	tme;
int	dom;
int	dim;
{
int	x,y;
char	rptname[512];
FILE	*fd;
int	counts[UZHPROJECTCOUNT][33];
int	waittm[UZHPROJECTCOUNT][33];
int	mdom;
struct	JobTableEntry *jte;
char	*p;
int	uzh;
int	uzhpw;
int	uzhpc;

	if (Flags & PR_DATA) {
		bzero(rptname,sizeof(rptname));
		bzero(counts, sizeof(counts));
		bzero(waittm, sizeof(waittm));

		VERBOSE printf("\n%s: %s - sifting throught the data\n",ProgName,__FUNCTION__);

		ResetJobTable();

		/*
		 * Loop on all UZH jobs
		 */
		while ((jte=GetJobTableEntry()) != NULL) {
			/*
			 *  make sure the job is within the period
			 */
			if (jte->jte_start < tms)
				continue;

			mdom = (jte->jte_start-tms)/86400 + 1;	/* what day of the month did the job start */

			/*
			 *  Strip off just the project number.  
			 *  All are in the format of uzh#.
			 */
	                p = jte->jte_account;
	                p += 3;
	                uzh = atol(p);

			/*
			 *  Accumulate the data into the array: counts/waittm[QUEUE][DAYOFMONTH]
			 */
	                counts[uzh][mdom] ++;
	                waittm[uzh][mdom] += (jte->jte_start - jte->jte_eligible);
		}

		sprintf(rptname,"%s/UZHQueueWait.dat",ReportDir);

		VERBOSE printf("%s: %s - generating data file\n",ProgName,__FUNCTION__);

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
		fprintf(fd,"#UZH average queue wait time:\n");
/*
	        fprintf(fd,"#dom    uzh0    uzh1    uzh2    uzh3    uzh4    uzh5    uzh6    uzh7    uzh8    uzh9   uzh10   uzh11   uzh12   uzh13   uzh14   uzh15   uzh16   uzh17   uzh18   uzh19    uzhp\n");
	        fprintf(fd,"#--- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- ------- -------\n");
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
	                uzhpc = uzhpw = 0;
			/*
			 *  Loop on the UZHPROJECTCOUNT UZH projects	
			 */
	                for (y=0;y<UZHPROJECTCOUNT;y++) {
				if (counts[y][x]) {
					fprintf(fd," %7d",waittm[y][x]/counts[y][x]/3600);
					uzhpw += waittm[y][x];
					uzhpc += counts[y][x]++;
				} else {
					fprintf(fd," %7d",0);
				}
	                }

			if ( uzhpc )
	                	fprintf(fd," %7d\n",uzhpw/uzhpc/3600);
			else
	                	fprintf(fd," %7d\n",0);
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
				"module load ploticus; pl -o %s/UZHQueueWait.%s -%s %s/UZHQueueWait.ploticus DATAFILE=%s/UZHQueueWait.dat DIM=%d MONTH=%s YEAR=%d",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ReportDir,dom,MONTHS[month+1],year);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/UZHQueueWait.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			x++;
		}
	}

	VERBOSE printf("%s: %s - reporting complete\n",ProgName,__FUNCTION__);
}


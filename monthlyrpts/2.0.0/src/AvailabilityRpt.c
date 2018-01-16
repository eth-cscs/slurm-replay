/**
@file  AvailabilityRpt.c
@brief Produce system availability reports
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h> 
#include <errno.h>
#include <ctype.h>
#include "perfrpts.h"

extern	char *ProgName;          /* Program Name                         */
extern	char *ReportDir;         /* Report output directory              */
extern	int   Flags;             /* Program Flags                        */

/**
 * @brief   Availability Reports
 * @details Produce System Availability Reports
 * @param   month  Specified month
 * @param   year   Specified year
 * @param   tms    Start timestamp
 * @param   tme    End timestamp
 * @param   dim    Days In Month
 * @param   dom    Day  Of Month
 */
void 
AvailabilityRpt(month,year,tms,tme,dim,dom)
int	month;
int	year;
time_t	tms;
time_t	tme;
int	dim;
int	dom;
{
int	x,y;
char	rptname[512];
char	buf[1024];
FILE	*fd,*fd1;
int	sm,sy;
float	f1,f2,f3,f4,f5;
int	i1,i2,i3;

	/*
	 * Go back 18 months
	 */
	sm = (month+20)%12;
	if (!sm) sm = 12;

	if (sm > 6) sy = year - 2;
	else        sy = year - 1;

	if (Flags & PR_DATA) {

		VERBOSE printf("%s: %s - generating data file\n",ProgName,__FUNCTION__);

		/*
		 *  Extract the data from WebRT using outagerpt
		 */
		bzero(buf,sizeof(buf));
		sprintf(buf,"%s/bin/outagerpt %d %d %s",ROOTDIR,month+1,year,MONTHS[month+1]);
		system(buf);

		/*
		 *  Open the historical availability data
		 */
		sprintf(rptname,"%s/reports/Availability.dat",ROOTDIR);
		if ((fd=fopen(rptname,"r")) == NULL) {
			fprintf(stderr,"%s: failed to open %s (%d: %s)\n",ProgName,rptname,errno,strerror(errno));
			fprintf(stderr,"%s: File: %s, Function: %s, Line: %d\n",ProgName,__FILE__,__FUNCTION__,__LINE__);
			exit(-1);
		}

		/*
		 *  Open the monthly outages data file.  This is used
		 *  to annotate the utilization report
		 */
		sprintf(rptname,"%s/Outages.dat",ReportDir);
		if ((fd1 = fopen(rptname,"w")) == NULL) {
			fprintf(stderr,"%s: failed to open %s (%d: %s)\n",ProgName,rptname,errno,strerror(errno));
			fprintf(stderr,"%s: File: %s, Function: %s, Line: %d\n",ProgName,__FILE__,__FUNCTION__,__LINE__);
			exit(-1);
		}

		/*
		 *  Loop through the availability data
		 */
		while (!feof(fd)) {
			bzero(buf,sizeof(buf));
			fgets(buf,sizeof(buf),fd);

			/*
			 *  skip non-data
			 */
			if ( !isdigit(buf[0]))
				continue;

			sscanf(buf,"%d/%d %f %f %f %d %f %f",&i1,&i2,&f1,&f2,&f3,&i3,&f4,&f5);

			/*
			 *  Looking only for the last 18 months
			 */
			if (i2 < sy )
				continue;

			else if ((i2 == sy) & (i1 < sm))
				continue;

			/*
			 *  Write out the ploticus annotation for event counts
			 */
			fprintf(fd1,"#proc   annotate\n");
			fprintf(fd1,"        location: %2.2d/%2.2d(s) -5.0(s)\n",i1,i2);
			fprintf(fd1,"        clip:     yes\n");
			fprintf(fd1,"        text:     %d\n",i3);
			fprintf(fd1,"\n");
		}
		fclose(fd);
		fclose(fd1);
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
			sprintf(rptname,"module load ploticus; pl -o %s/Availability.%s -%s %s/DaintMonthly.ploticus DATAFILE=%s/reports/Availability.dat OUTAGES=%s/Outages.dat STARTDATE=%2.2d/%4.4d ENDDATE=%2.2d/%4.4d",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ROOTDIR,ReportDir,sm,sy,month+1,year);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/Availability.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"module load ploticus; pl -o %s/CSCS_Availability.%s -%s %s/CSCSDaintMonthly.ploticus DATAFILE=%s/reports/Availability.dat OUTAGES=%s/Outages.dat STARTDATE=%2.2d/%4.4d ENDDATE=%2.2d/%4.4d",
				ReportDir,PlotFormats[x],PlotFormats[x],PLOTDIR,ROOTDIR,ReportDir,sm,sy,month+1,year);
			system(rptname);

			bzero(rptname,sizeof(rptname));
			sprintf(rptname,"%s/CSCS_Availability.%s",ReportDir,PlotFormats[x]);
			chmod(rptname,GlobalPlotMode);

			x++;
		}
	}

	VERBOSE printf("%s: %s - reporting complete\n",ProgName,__FUNCTION__);
}


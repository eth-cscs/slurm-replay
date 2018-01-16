/**
 * @file  AllocationRpt.c
 * @brief Produce the Quarterly Allocation Report showing
 *        how many hours are allocated versus the physical
 *        raw hours possible.  A second report shows how
 *        many hours were actually delivered and consumed
 *        per month.
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

static	int	dop;             /* Days in Allocation Period            */
static	int	firstmonth;      /* first month in the period            */
static	float	GrossAvail[3];   /* three month gross availability       */
static	int	delivered[3][4]; /* three month delivered/consumed hours */

/**
 * @brief    Allocation Period Settings
 * @details  Determine data and limits for the allocation period
 * @param    month The month in the period
 * @param    year  The year of the period
 */
static void
AllocationPeriod(month,year)
{
time_t	bp,np;
struct	tm tmp;
FILE	*fd;
char	buf[256];
char	dt[8];
char	tst[8];
float	mt,ge,fa,gs,sc;
int	ev;
int	x,m;
int	i1,i2,i3,i4,i5,i6,i7,i8,i9,i10,i11,i12,i13,i14,i15,i16;
float	f1,f2;
int	him;

	bzero(&GrossAvail,sizeof(GrossAvail));

	/*
	 *  Identify the first month in the period,
	 *  0 is January.
	 */
	     if (month >= 9) firstmonth = 9;
	else if (month >= 6) firstmonth = 6;
	else if (month >= 3) firstmonth = 3;
	else                 firstmonth = 0;

	/*
	 *  Determine the number of days in the period
	 *  Simple: ((timestamp of first day in next period) - (first day in this period)) / 86400
	 */
	bzero(&tmp,sizeof(tmp));
	tmp.tm_year = year - 1900;
	tmp.tm_mon  = firstmonth;
	tmp.tm_mday = 1;

	bp = mktime(&tmp);

	tmp.tm_mon += 3;

	/*
	 * next year, year end wrap
	 */
	if (tmp.tm_mon == 12) {
		tmp.tm_mon = 1;
		tmp.tm_year += 1;
	}
	np = mktime(&tmp);

	dop = (np - bp)/86400;

	x = 0;
	m = firstmonth+1;

	bzero(&tst,sizeof(tst));
	sprintf(tst,"%2.2d/%4.4d",m,year);

	/*
	 *  Open the historical Availability data file
	 */
	sprintf(buf,"%s/reports/Availability.dat",ROOTDIR);
	if ((fd=fopen(buf,"r")) == NULL) {
		fprintf(stderr,"%s: failed to open %s (%d: %s)\n",ProgName,buf,errno,strerror(errno));
		fprintf(stderr,"%s: File: %s, Function: %s, Line: %d\n",ProgName,__FILE__,__FUNCTION__,__LINE__);
		exit(-1);
	}

	/*
	 *  Loop through the historical data to locate 
	 *  only entries for months in the period.
	 *  NOTE: the file is chronological.
	 */
	while (!feof(fd)) {
		gs = 0.0;
		bzero(buf,sizeof(buf));
		fgets(buf,sizeof(buf),fd);

		/*
		 *  Skip everything but data lines
		 */
		if (!isdigit(buf[0]))
			continue;

		/*
		 *  See the file Availability.dat for field details
		 */
		sscanf(buf,"%s %f %f %f %d %f %f",dt,&mt,&ge,&fa,&ev,&gs,&sc);

		/*
		 * If it is in this period then count it
		 */
		if (!strcmp(tst,dt)) {
			GrossAvail[x++] = gs;
			sprintf(tst,"%2.2d/%4.4d",++m,year);
		}
	}
	fclose(fd);

	bzero(delivered,sizeof(delivered));

	/*
	 *  Loop through the 3 months in the allocation period
	 */
	for (x=0;x<3;x++) {
		if ((firstmonth+x) > month)
			break;

		sprintf(buf,"%s/../%2.2d-%s/Utilization.dat",ReportDir,firstmonth+1+x,MONTHS[firstmonth+1+x]);
		if ((fd=fopen(buf,"r")) != NULL) {
			bzero(buf,sizeof(buf));
			fgets(buf,sizeof(buf),fd);

			while (!feof(fd)) {
				if (buf[0] != '#') {

					sscanf(buf,"%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %f %f",
						&i1,&i2,&i3,&i4,&i5,&i6,&i7,&i8,&i9,&i10,&i11,&i12,&i13,&i14,&i15,&i16,&f1,&f2);
	
					delivered[x][0] += i13;	/* gpu total consumed */
					delivered[x][1] += i14;	/*  mc total consumed */
					delivered[x][2] += i15; /* gpu max   possible */
					delivered[x][3] += i16; /*  mc max   possible */
				}
	
				bzero(buf,sizeof(buf));
				fgets(buf,sizeof(buf),fd);
			}
			fclose(fd);

		}
	}
}

/**
 * @brief   Produce the Quarterly Allocation Report
 * @details Produce the Allocation and Delivered/Consumed reports
 * @param   month  Specified month
 * @param   year   Specified year
 * @param   tms    Start timestamp
 * @param   tme    End timestamp
 * @param   dim    Days In Month
 */
void 
AllocationRpt(month,year,tms,tme,dim,dom)
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
struct	ProjectTableEntry *pte;
int	alloc_gpu=0,alloc_any=0,alloc_mtc=0;
int	max_gpu,max_mtc;
time_t	tmn;
struct	tm *tmp,tml;

	/*
	 * Get all the data for the allocation period
	 */
	AllocationPeriod(month,year);

	if (Flags & PR_DATA) {
		ResetProjectTable();

		VERBOSE printf("\n%s: %s - sifting through the data\n",ProgName,__FUNCTION__);

		/*
		 *  Loop through all valid projects and accumulate
		 *  their allocation based on nodetype.
		 */
		while ((pte=GetProjectTableEntry()) != NULL) {
			if (!strcmp(pte->pte_nodetype,"Any"))
				alloc_any += pte->pte_quota;

			else if (!strcmp(pte->pte_nodetype,"Hybrid"))
				alloc_gpu += pte->pte_quota;

			else if (!strcmp(pte->pte_nodetype,"Multicore"))
				alloc_mtc += pte->pte_quota;
		}

		VERBOSE printf("%s: %s - generating report\n",ProgName,__FUNCTION__);

		bzero(rptname,sizeof(rptname));
		sprintf(rptname,"%s/AllocateRpt.txt",ReportDir);

		/*
		 *  Open the report file for writing
		 */
		if ((fd=fopen(rptname,"w")) == NULL) {
			fprintf(stderr,"%s: failed to open %s (%d: %s)\n",ProgName,rptname,errno,strerror(errno));
			fprintf(stderr,"%s: File: %s, Function: %s, Line: %d\n",ProgName,__FILE__,__FUNCTION__,__LINE__);
			exit(-1);
		}
		
		/*
		 *  Hours delivered/consumed report
		 */
		fprintf(fd,"\n");
		fprintf(fd,"             -----------------------------------------------------------------------------------\n");
		fprintf(fd,"                                   Monthly Hours Delivered/Consumed Report\n");
		fprintf(fd,"            +-----------------------------------------+-----------------------------------------+\n");
		fprintf(fd,"            |                  Hybrid                 |                Multicore                |\n");
		fprintf(fd,"+-----------+-----------------------------------------+-----------------------------------------+\n");
		fprintf(fd,"|    Month  | Delivered      (%%) |  Consumed      (%%) | Delivered      (%%) |  Consumed      (%%) |\n");
		fprintf(fd,"+-----------+--------------------+--------------------+--------------------+--------------------+\n");
		for(x=0;x<3;x++) {
			fprintf(fd,"| %9s | %9.0f (%5.1f%%) | %9.0f (%5.1f%%) | %9.0f (%5.1f%%) | %9.0f (%5.1f%%) |\n",
				MONTHS[firstmonth+1+x],
				(float)delivered[x][2]*(GrossAvail[x]/100.0),GrossAvail[x],
				(float)delivered[x][0],(float)delivered[x][0]/(float)delivered[x][2]*(GrossAvail[x]/100.0)*100.0,
				(float)delivered[x][3]*(GrossAvail[x]/100.0),GrossAvail[x],
				(float)delivered[x][1],(float)delivered[x][1]/(float)delivered[x][3]*(GrossAvail[x]/100.0)*100.0);

			fprintf(fd,"+-----------+--------------------+--------------------+--------------------+--------------------+\n");
		}
		fprintf(fd,"\n");

		fclose(fd);
	}

	VERBOSE printf("%s: %s - reporting complete\n",ProgName,__FUNCTION__);
}


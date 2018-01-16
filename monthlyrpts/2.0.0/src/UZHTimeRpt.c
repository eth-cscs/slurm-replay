/**
 * @file  UZHTimeRpt.c
 * @brief UZH Requested vs Elapsed Time Report
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h> 
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include "perfrpts.h"

extern	char *ProgName;          /* Program Name                         */
extern	char *ReportDir;         /* Report output directory              */
extern	int   Flags;             /* Program Flags                        */

/**
 * @brief   UZH Time Limit Report
 * @details Produce UZH Job Time Limit Report
 * @param   month  Specified month
 * @param   year   Specified year
 * @param   tms    Start timestamp
 * @param   tme    End timestamp
 * @param   dim    Days In Month
 */
void
UZHTimeRpt(month,year,tms,tme,dim,dom)
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
FILE	*fd;
struct	JobTableEntry *jte;
struct	tm *tmp,tmt;
struct	passwd *pswd;
/*int	uzhalloc = 561735;*/	/* allocation for uzhp */
/*int	uzhalloc = 709509;*/	/* allocation for uzhp */
int	uzhalloc = 649509;	/* allocation for uzhp */
time_t	strt,endt,now;
	
	ResetJobTable();

	VERBOSE printf("%s: %s - generating report file\n",ProgName,__FUNCTION__);

	time(&now);
	tmp = localtime(&now);
	memcpy(&tmt,tmp,sizeof(struct tm));
	tmt.tm_hour=tmt.tm_min=tmt.tm_sec=0;
	strt = mktime(&tmt);
	strt -= 8 * 86400;
	tmt.tm_hour=23;
	tmt.tm_min=59;
	tmt.tm_sec=59;
	endt = mktime(&tmt);
	endt -= 86400;

	bzero(rptname,sizeof(rptname));
	sprintf(rptname,"/tmp/UZHTimeLimit_%4.4d%2.2d%2.2d.txt",
		tmp->tm_year+1900,tmp->tm_mon+1,tmp->tm_mday);

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
	fprintf(fd,"#UZH Time Limit Report\n");
	tmp = localtime(&strt); 

	fprintf(fd,"#        From: %4.4d%2.2d%2.2d %2.2d:%2.2d:%2.2d\n",
		tmp->tm_year+1900,tmp->tm_mon+1,tmp->tm_mday,
		tmp->tm_hour,tmp->tm_min,tmp->tm_sec);

	tmp = localtime(&endt);

	fprintf(fd,"#     Through: %4.4d%2.2d%2.2d %2.2d:%2.2d:%2.2d\n",
		tmp->tm_year+1900,tmp->tm_mon+1,tmp->tm_mday,
		tmp->tm_hour,tmp->tm_min,tmp->tm_sec);

	fprintf(fd,"#------------------------------------------------\n");
	fprintf(fd,"#    JobID Username Account Nodes ReqTime Elapsed\n");
	fprintf(fd,"#--------- -------- ------- ----- ------- -------\n");

	/*
	 *  Loop on UZH Jobs
	 */
	while ((jte=GetJobTableEntry()) != NULL) {
		/*
		 *  Skip running jobs
		 */
		if (jte->jte_elapsed < 0 )
			continue;

		/*
		 *  Only jobs that completed in the last 7 days
		 */
		if ((jte->jte_end < strt ) || (jte->jte_end > endt))
			continue;

		/*
		 *  Limit to only jobs requesting more than
		 *  double what they actually used.
		 */
		if (jte->jte_timelimit > (jte->jte_elapsed/60*2)) {
			pswd = getpwuid(jte->jte_uid);

			fprintf(fd," %9d %8.8s %-7.7s %5d %7d %7ld\n",
				jte->jte_jobid,pswd->pw_name,jte->jte_account,jte->jte_nodes,
				jte->jte_timelimit,jte->jte_elapsed/60);
		}
	}

	fclose(fd);

	if (Flags&PR_MAIL) {
		sprintf(buf,"echo Weekly UZH Time Limit Report | mail -a %s -s \"UZH Time Limit Report\" cardo@cscs.ch",
			rptname);
		system(buf);
		unlink(rptname);
	}

	VERBOSE printf("%s: %s - reporting complete\n",ProgName,__FUNCTION__);
}


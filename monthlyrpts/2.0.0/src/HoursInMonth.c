#include <stdio.h>
#include <sys/types.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "perfrpts.h"

extern	int	INITGPUCNT,INITMCRCNT;
extern	struct	nodesperday *NodesPerDay;

int	DailyNodes[32][4];
int	TotalGPUHours=0,TotalMCRHours=0;

/**
 *
 */
void
HoursInMonth(m,y)
int	m;
int	y;
{
struct	nodesperday *npd;
struct	tm *ltm;
struct	tm  tdy;
time_t	tms,tmn,now;
int	x;
int	dim;

	/*
	 *  Current time stamp
	 */
	time(&now);

	/*
	 *  Initialize the counts for the month
	 */
	for (x=0;x<32;x++) {
		DailyNodes[x][0] = -1;
		DailyNodes[x][1] = -1;
	}

	/*
	 *  Timestamp for first day of month
	 */
	bzero(&tdy,sizeof(struct tm));
	tdy.tm_year = y - 1900;
	tdy.tm_mday = 1;
	tdy.tm_mon  = m;
	tms = mktime(&tdy);

	/*
	 *  Adjust for daylight vs standard time
	 */
	ltm = localtime(&tms);
	if ( ltm->tm_hour == 1 )
		tms -= 3600;

	/*
	 *  Timestamp for first day of next month
	 */
	bzero(&tdy,sizeof(struct tm));
	tdy.tm_year = y - 1900;
	tdy.tm_mday = 1;
	tdy.tm_mon  = m+1;
	tmn = mktime(&tdy);

	/*
	 *  If we are mid-month, then adjust
	 */
	if (tmn > now) {
		tmn = now;
		ltm = localtime(&tmn);
		tdy.tm_year = ltm->tm_year;
		tdy.tm_mday = ltm->tm_mday;
		tdy.tm_mon = ltm->tm_mon;
		tmn = mktime(&tdy);
	}

	/*
	 *  Adjust for daylight vs standard time
	 */
	ltm = localtime(&tmn);
	if ( ltm->tm_hour == 1 )
		tmn -= 3600;

	/*
	 *  How many days in the month
	 */
	dim = (tmn-tms)/86400 + 1;

	/*
	 * Determine the starting node counts for the month
	 */
	npd = NodesPerDay;
	while (npd != NULL) {
		if (npd->npd_timestamp < tms) {
			INITGPUCNT = npd->npd_gpunodes;
			INITMCRCNT = npd->npd_mcrnodes;
		}

		npd = npd->npd_next;
	}

	/*
	 *  Initialize the monthly array
	 */
	for (x=1;x<=dim;x++) {
		DailyNodes[x][0] = INITGPUCNT;
		DailyNodes[x][1] = INITMCRCNT;
	}

	/*
	 *  Loop on the changes
	 */
	npd = NodesPerDay;
	while (npd != NULL) {
		if ((npd->npd_timestamp >= tms) && (npd->npd_timestamp < tmn)) {
			/*
			 *  counts changed during the month so adjust
			 */
			ltm = localtime(&npd->npd_timestamp);

			for (x=ltm->tm_mday;x<=dim;x++) {
				DailyNodes[x][0] = npd->npd_gpunodes;
				DailyNodes[x][1] = npd->npd_mcrnodes;
			}
		}

		npd = npd->npd_next;
	}

	/*
	 *  total up the hours in the month for the max
	 */
	for (x=1;x<=dim;x++) {
		TotalGPUHours += DailyNodes[x][0];
		TotalMCRHours += DailyNodes[x][1];
	}
}


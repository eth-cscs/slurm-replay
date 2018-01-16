/**
@file  GetJobs.c
@brief Setup routine for getting all jobs from the DB
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include "perfrpts.h"

extern	char *ProgName;          /* Program Name                         */
extern  int   Flags;             /* Program Flags                        */

/**
 * @brief   Get All Jobs
 * @details Get All Jobs from the Slurm DB
 * @param   month    Specified month
 * @param   year     Specified year
 * @param   tms      Start timestamp
 * @param   tme      End timestamp
 * @param   dim      Days In Month
 * @param   customer Specific customer
 */
int 
GetJobs(month,year,tms,tme,customer)
int	month;
int	year;
time_t	tms;
time_t	tme;
char	*customer;
{

        VERBOSE printf("\n%s: collecting job data for %s users\n",ProgName,customer);

	/*
	 *  Open the MySQL database
	 */
	if (OpenDB() != 0 )
		return(1);

	/*
	 *  Go get the jobs
	 */
	GetAllJobs(tms,tme,customer);

	/*
	 *  Close the DB
	 */
	CloseDB();

	return(0);
}


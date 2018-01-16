/**
@file  GetProjects.c
@brief Setup Routine for getting all projects from the CSCS DB
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
 * @return 0 for success, 1 for failure
 */
int
GetProjects(month,year,tms,tme)
int	month;
int	year;
time_t	tms;
time_t	tme;
{

        VERBOSE printf("\n%s: collecting project data\n",ProgName);

	/*
	 *  Open the MySQL database
	 */
	if (OpenCSCSDB() != 0 )
		return(1);

	/*
	 *  Go get the projects...
	 */
	GetAllProjects(tms,tme);

	/*
	 *  Close the DB
	 */
	CloseCSCSDB();

	return(0);
}


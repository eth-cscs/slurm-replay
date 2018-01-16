#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "perfrpts.h"

extern	char *ProgName;          /* Program Name                         */

/**
 * @brief   Create Report Directory
 * @details Create the full path of the report directory
 * @param   year   Specified year
 * @param   month  Specified month
 * @return  pathname to report directory
 */
char *
reportdir(year,month)
int	year;
int	month;
{
static	char rd[256];
struct	stat stb;

	umask(GlobalUmask);

	/*
	 *  Make sure the year director exists
	 */
	bzero(rd,sizeof(rd));
	sprintf(rd,"%s/%4.4d",REPORTROOT,year);

	if (stat(rd,&stb)) {
		if (mkdir(rd,GlobalExecMode|S_ISGID))
			fprintf(stderr,"%s: unable to create: %s (%d: %s)\n",ProgName,rd,errno,strerror(errno));
	}

	/*
	 *  Make sure the month directory exists
	 */
	bzero(rd,sizeof(rd));
	sprintf(rd,"%s/%4.4d/%2.2d-%s",REPORTROOT,year,month+1,MONTHS[month+1]);

	if (stat(rd,&stb)) {
		if (mkdir(rd,GlobalExecMode|S_ISGID))
			fprintf(stderr,"%s: unable to create: %s (%d: %s)\n",ProgName,rd,errno,strerror(errno));
	}

	/*
	 *  Okay so double check the creation
	 */
	if (stat(rd,&stb)) {
		fprintf(stderr,"%s: unable to verify output directory (%d: %s)\n",ProgName,errno,strerror(errno));
		return(NULL);
	}

	return(rd);
}


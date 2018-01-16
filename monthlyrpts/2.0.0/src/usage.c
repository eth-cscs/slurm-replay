
/**
 * @file  usage.c
 * @brief Usage information.
 */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include "perfrpts.h"
#include "version.h"

/**
 * @brief   Display version number.
 * @details Display the version string.
 * @param   progname  The program name.
 */
void version(progname)
char    *progname;
{
        VERSION_STRING(progname);
}

/**
 * @brief   Display usage information.
 * @details Display command syntax with usage options.
 * @param   progname The program name.
 */
void usage(progname)
char    *progname;
{
int	x;

        printf("\n");
        version(progname);
        printf("\n");
        printf("Usage: %s [-y year] [-m month] [-DPGLZ] [-v] [-h|-V]\n",progname);
        printf("Where:\n");

	printf("    -y year ......... group name                   (default: current year)\n");
	printf("    -m month ........ number of entries to display (default: current month)\n");

	printf("    -D .............. produce only data files\n");
	printf("    -G .............. limit to general job/project reports\n");
	printf("    -L .............. limit to large job reports only\n");
	printf("    -P .............. produce only plots\n");
	printf("    -Z .............. limit to UZH only reports\n");

	printf("    -v .............. enable verbose output\n");

	/*
	 * Help/Version
	 */
        printf("    -h .............. display help text\n");
        printf("    -V .............. display the version number\n");
        printf("\n");
}


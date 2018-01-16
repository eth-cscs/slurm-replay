
/**
 * @file  rdconfig.c
 * @brief Read the configuration file
 */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include "perfrpts.h"

extern char *ProgName;		/* Program Name */

/*
 *  Global variables for configuration settings
 */
int	INITGPUCNT=0,INITMCRCNT=0;
struct	nodesperday *NodesPerDay = (struct nodesperday *)NULL;

/**
 * @brief   Read configuration file.
 * @details Read configuration options from the configuration file.
 * @retval  0 for success, 1 for failure
 */
int
rdconfig()
{
char	buf[256];
char	*line;
int	lineno = 0;
FILE	*fd;
int	x,cnt,y,m,d,gpus,mcrs;
char	*p;
time_t	tms;
struct	tm ttm,*ptm;
char	keyword[16];
char	value1[16],value2[16],value3[16],value4[16];
struct	nodesperday *npd,*tnpd;
static	int RDCONFIG = 0;

	/*
	 *  Prevent the configuration file from being
	 *  processed multiple times.
	 */
	if (RDCONFIG++) {
		fprintf(stderr,"%s: configuration file has already been read\n",ProgName);
		return(1);
	}

	/*
	 *  Open the configuration file
	 */
	if ((fd=fopen(CONFIGFILE,"r")) == NULL) {
		fprintf(stderr,"%s: failed to open configuration file - %s\n",ProgName,CONFIGFILE);
		printf("%s\n",CONFIGFILE);
		return(1);
	}

	/*
	 *  Loop through the configuration file
	 */
	bzero(buf,sizeof(buf));
	while(fgets(buf,sizeof(buf),fd)) {
		/*
		 *  Strip off newline if there
		 */
		if (buf[strlen(buf)-1] == '\n')
			buf[strlen(buf)-1] = '\0';

		lineno++;

		/*
		 *  Strip off comments
		 */
		p = strchr(buf,'#');
		if (p != NULL)
			*p = '\0';

		/*
		 *  strip off trailing white space
		 */
		p = buf + strlen(buf) - 1;
		while(isspace(*p)) {
			*p = '\0';
			p--;
		}

		/*
		 *  strip off leading white space
		 */
		p = buf;
		while(isspace(*p))
			p++;

		/*
		 *  If this line still contains information, then parse it for options
		 */
		if (strlen(p)) {
			cnt = sscanf(buf,"%s %s %s %s %s",keyword,value1,value2,value3,value4);

			/*
			 * convert to uppercase
			 */
			for(x=0;x<strlen(keyword);x++) {
				if (islower(keyword[x]))  keyword[x] = toupper(keyword[x]);
			}

			if (!strcmp(keyword,"NODECOUNTS")) {
				/*
				 *  On the identified date, the node counts were altered 
				 *  to these new values.
				 *  Format:  NODECOUNTS yyyymmdd #gpunodes #multicorenodes
				 */
				if (cnt != 4) {
					fprintf(stderr,"%s: line %d: incorrect number of arguments for NODECOUNTS\n",ProgName,lineno);

			 	} else {
					bzero(&ttm,sizeof(struct tm));
	
					/*
					 * Parse the date field
					 */
					d = atoi(value1+6);	/* Day   */
					value1[6] = '\0';
					m = atoi(value1+4);	/* Month */
					value1[4] = '\0';
					y = atoi(value1);	/* Year  */

					ttm.tm_year = y - 1900;
					ttm.tm_mon  = m - 1;
					ttm.tm_mday = d;

					tms = mktime(&ttm);
					ptm = localtime(&tms);
					if (ptm->tm_hour == 1)
						tms -= 3600;

					if ( NodesPerDay == (struct nodesperday *)NULL) {
						/*
						 *  List is empty, first to be placed
						 */
						NodesPerDay = (struct nodesperday *)malloc(sizeof(struct nodesperday));
						npd = NodesPerDay;

					} else {
						npd = NodesPerDay;

						/*
						 *  go to the end of the list
						 */
						while (npd->npd_next != NULL) npd = npd->npd_next; 

						npd->npd_next = (struct nodesperday *)malloc(sizeof(struct nodesperday));
						npd = npd->npd_next;
					}

					bzero(npd,sizeof(struct nodesperday));

					npd->npd_timestamp = tms;
					npd->npd_gpunodes = atoi(value2);
					npd->npd_mcrnodes = atoi(value3);
				}

			} else if (!strcmp(keyword,"NODEINIT")) {
				/*
				 *  Initial node counts for the system
				 *  Format:  NODEINIT yyyymmdd #gpunodes #multicorenodes
				 */
				if (cnt != 4) {
					fprintf(stderr,"%s: line %d: incorrect number of arguments for NODEINIT\n",ProgName,lineno);
	
			 	} else {
					INITGPUCNT = atoi(value2);
					INITMCRCNT = atoi(value3);
				}

			} else {
				fprintf(stderr, "%s: line %d: invalid option %s\n",ProgName,lineno,buf);
			}
		}
	}

	fclose(fd);

	return(0);
}


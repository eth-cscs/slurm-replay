/**
 * @file   dbops.c
 * @brief  MySQL database functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <my_global.h>
#include <mysql.h>
#include <ctype.h>
#include "perfrpts.h"

extern	char	*ProgName;
static	MYSQL	*con = NULL;
static	MYSQL	*cscscon = NULL;
static	int	db_open=0;
static	int	cscsdb_open=0;

static	struct	JobTableEntry *JobTable = NULL;
static	int	JobTableIndex = 0;
static	int	JobTableCount = 0;

static	struct	ProjectTableEntry *ProjectTable = NULL;
static	int	ProjectTableIndex = 0;
static	int	ProjectTableCount = 0;

/**
 *  @brief   Open the mysql database
 *  @details Initialize and open the connection to the MySQL database.
 *  @retval  rc 0 for success, 1 for failure 
 */
int 
OpenDB()
{
int	rc = 0;

	/*
	 *  Safety check to make sure the db is only opened once
	 */
	if (db_open == 1) {
		fprintf(stderr,"%s: db already open\n",ProgName);
		rc = 1;
	} else { 

		/*
		 *  Allocate a new db object
		 */
		con = mysql_init(NULL);

		if (con == NULL) {
			fprintf(stderr,"%s: mysql_init() failed\n",ProgName);
			rc = 1;

		} else {
			/*
			 *  Make the connection to the database
			 */
			if (mysql_real_connect(con, "localhost", "maximem", "", "slurm_acct_db", 0, NULL, 0) == NULL) {
				fprintf(stderr,"%s: mysql_real_connection() failed\n",ProgName);
				rc = 1;

			} else {
				/*
				 *  Important to mark the database as open, on success
				 */
				db_open = 1;
			}
		}
	}
	
	return(rc);
}

/**
 *  @brief   Open the mysql database
 *  @details Initialize and open the connection to the MySQL database.
 *  @retval  rc 0 for success, 1 for failure 
 */
int 
OpenCSCSDB()
{
int	rc = 0;

	/*
	 *  Safety check to make sure the db is only opened once
	 */
	if (cscsdb_open == 1) {
		fprintf(stderr,"%s: CSCS db already open\n",ProgName);
		rc = 1;

	} else { 
		/*
		 *  Allocate a new db object
		 */
		cscscon = mysql_init(NULL);

		if (cscscon == NULL) {
			fprintf(stderr,"%s: mysql_init() failed\n",ProgName);
			rc = 1;

		} else {
			/*
			 *  Make the connection to the database
			 */
			if (mysql_real_connect(cscscon, "db.cscs.ch", "useradm", "readonly", "stat_csmon", 0, NULL, 0) == NULL) {
				fprintf(stderr,"%s: mysql_real_connection() failed\n",ProgName);
				rc = 1;

			} else {
				/*
				 *  Important to mark the database as open, on success
				 */
				cscsdb_open = 1;
			}
		}
	}
	
	return(rc);
}

int
GetUZHAlloc()
{
int	uzhalloc = -1;
char	query[256];
MYSQL_RES *result = NULL;
MYSQL_ROW row;
int	num_rows;

	memset(query,0,sizeof(query));
	sprintf(query,"SELECT q.quota FROM quotas as q WHERE q.project_id = 'uzhp' AND q.FK_fcl_id = 'DAINT';");

	/*
	 *  Open the MySQL database
	 */
	if (OpenCSCSDB() != 0 )
		return(1);

	/*
	 *  
	 */
	if (mysql_query(cscscon, query)) {
		fprintf(stderr,"%s: mysql_query failed! %s\n",ProgName, query);
		uzhalloc = -1;

	} else {
		/*
		*  store the results
		*/
		result = mysql_store_result(cscscon);

		if (result == NULL) {
			fprintf(stderr,"%s: mysql_store_result() returned NULL\n",ProgName);
			uzhalloc = -1;

		} else {
			num_rows = mysql_num_rows(result);
			row = mysql_fetch_row(result);
			uzhalloc = atol(row[0]);
		}
	}

	/*
	 *  Close the DB
	 */
	CloseCSCSDB();

	return(uzhalloc);
}

/**
 *  @brief   Open the mysql database
 *  @details Initialize and open the connection to the MySQL database.
 *  @param   strt      Start timestamp
 *  @param   endt      End Timestamp
 *  @param   customer  Specific Customer name
 *  @retval  rc 0 for success, -1 for failure 
 */
int 
GetAllJobs(strt,endt,customer)
time_t	strt;
time_t	endt;
char	*customer;
{
MYSQL_RES	*result = NULL;
MYSQL_ROW	row;
int		rc;
char		query[356];
int		num_rows;
struct JobTableEntry *jt;
int		idx;
char		*p,*pp;
char		*ndlst = NULL;

	/*
	 *  Construct the query
	 */
	memset(query,0,sizeof(query));

	if (!strcmp(customer,"UZH")) {
		/*
		 *  Specialized query for UZH
		 */
		sprintf(query,
			"SELECT d.time_submit,d.time_eligible,d.time_start,d.time_end,d.nodes_alloc,d.partition,d.id_job,d.account,d.mem_req,d.timelimit,d.nodelist,d.id_user FROM daint_job_table as d WHERE d.time_start >= '%ld' AND d.time_start < '%ld' AND d.account LIKE '%%uzh%%';",
			strt,endt);

	} else {
		/*
		 *  General query for ALL customers
		 */
		sprintf(query,
			"SELECT d.time_submit,d.time_eligible,d.time_start,d.time_end,d.nodes_alloc,d.partition,d.id_job,d.account,d.mem_req,d.timelimit,d.nodelist,d.id_user FROM daint_job_table as d WHERE time_start >= '%ld' AND time_start < '%ld' AND partition <> 'wlcg';",
			strt,endt);
	}

	/*
	 *  Perform the mysql query 
	 */
	if (mysql_query(con, query)) {
		fprintf(stderr,"%s: mysql_query failed, %s\n",ProgName, query);
		rc = -1;

	} else {
		/*
		 *  store the results
		 */
		result = mysql_store_result(con);
  
		if (result == NULL) {
			fprintf(stderr,"%s: mysql_store_result() returned NULL\n",ProgName);
			rc = -1;

		} else {
			num_rows = mysql_num_rows(result);
			JobTableCount = num_rows;

			/*
			 *  Initialize the data storage
			 */
			JobTable = (struct JobTableEntry *)malloc(num_rows * (sizeof(struct JobTableEntry)));
			bzero(JobTable,(num_rows * (sizeof(struct JobTableEntry))));
			jt = JobTable;

			/*
			 *  Extract the data
			 */
			idx = 0;
			while((row = mysql_fetch_row(result))) {
				/*
				 * Store the data
				 *
				 * [0] ... time_submit
				 * [1] ... time_eligible
				 * [2] ... time_start
				 * [3] ... time_end
				 * [4] ... nodes_alloc
				 * [5] ... partition
				 * [6] ... jobid
				 * [7] ... account
				 * [8] ... mem_req
				 * [9] .. timelimit
				 * [10] .. nodelist
				 * [11] .. user ID
				 */

				jt[idx].jte_submit    = atol(row[0]);
				jt[idx].jte_eligible  = atol(row[1]);
				jt[idx].jte_start     = atol(row[2]);
				jt[idx].jte_end       = atol(row[3]);
				jt[idx].jte_nodes     = atoi(row[4]);

				if (row[5] != NULL)
					sprintf(jt[idx].jte_partition,"%s",row[5]);
				else 
					sprintf(jt[idx].jte_partition,"BAD");

				jt[idx].jte_jobid   = atoi(row[6]);
				jt[idx].jte_elapsed = jt[idx].jte_end - jt[idx].jte_start;

				/*
				 * job was running
				 */
				if (jt[idx].jte_elapsed < 0 )
					jt[idx].jte_elapsed = 0;

				if (row[7] != NULL)
					sprintf(jt[idx].jte_account,"%s",row[7]);
				else
					sprintf(jt[idx].jte_account,"BAD");

				jt[idx].jte_mem       = atoi(row[8]);
				jt[idx].jte_timelimit = atol(row[9]);
				jt[idx].jte_uid       = atol(row[11]);

				ndlst = strdup(row[10]);

				/*
				 *  isolate the first nid, these are numerically ordered
				 */
				p = strchr(ndlst,'[');
				if (p == NULL) {
					p = ndlst + 3;
					
				} else {
					*p = '\0';
					p++;
					p = strtok(p,"-");
					pp = strchr(p,',');
					if (pp != NULL)
						*pp = '\0';
				}

				/*
				 *  Is this a MC or GPU job
				 */
				if (atoi(p) < 1920)
					jt[idx].jte_nodetype = NODE_MC;
				else
					jt[idx].jte_nodetype = NODE_GPU;


				if (ndlst != NULL)
					free(ndlst);

				/*
				 * sanity check
				 */
				if (jt[idx].jte_start < jt[idx].jte_eligible)
					jt[idx].jte_start = jt[idx].jte_eligible;

				if (jt[idx].jte_eligible == 0 )
					jt[idx].jte_eligible = jt[idx].jte_start;

				idx++;
			}

			mysql_free_result(result);
		}
	}

	return(rc);
}

/**
 *  @brief   Job Table Entry
 *  @details Returns the next entry in the job table
 *  @retval  rc pointer for success, NULL for failure 
 */
struct	JobTableEntry *
GetJobTableEntry()
{
static	struct JobTableEntry *jte;

	if (JobTableIndex == JobTableCount)
		jte = NULL;
	else
		jte = (struct JobTableEntry *)&JobTable[JobTableIndex++];

	return(jte);
}

/**
 *  @brief   Reset Job Table Index
 *  @details Sets the index into the job table to 0
 */
void
ResetJobTable()
{
	JobTableIndex = 0;
}

/**
 *  @brief   Clear Job Table 
 *  @details Sets the index into the job table to 0
 *           and releases the memory for the Job Table
 */
void 
ClearJobTable()
{
	free(JobTable);
	JobTableIndex = 0;
	JobTable = (struct JobTableEntry *)NULL;
}

/**
 *  @brief   Open the mysql database
 *  @details Initialize and open the connection to the MySQL database.
 *  @param   strt      Start timestamp
 *  @param   endt      End Timestamp
 *  @retval  rc 0 for success, -1 for failure 
 */
int 
GetAllProjects(strt,endt)
time_t	strt;
time_t	endt;
{
MYSQL_RES	*result = NULL;
MYSQL_ROW	row;
int		rc;
char		query[256];
int		num_rows;
struct ProjectTableEntry *pt;
int		idx;
struct		tm tmp,*tms;
	
	tms = localtime(&strt);
	memcpy(&tmp,tms,sizeof(struct tm));
	if (tmp.tm_mon <= 2)
		tmp.tm_mon = 0;
	else if (tmp.tm_mon <= 5)
		tmp.tm_mon = 3;
	else if (tmp.tm_mon <= 8)
		tmp.tm_mon = 6;
	else
		tmp.tm_mon = 9;

	/*
	 *  Construct the query
	 */
	memset(query,0,sizeof(query));

/*
	printf("%4.4d-%2.2d-01 %4.4d-%2.2d-01\n",
		tmp.tm_year+1900,tmp.tm_mon+1,tmp.tm_year+1900,tmp.tm_mon+4);
*/
	sprintf(query,
		"SELECT quotas.project_id,quotas.quota,project.proj_nodetype,project.proj_end FROM quotas JOIN project ON quotas.project_id=project.proj_id WHERE quotas.FK_fcl_id = 'daint' AND project.proj_end >= '%4.4d-%2.2d-01' AND project.proj_start < '%4.4d-%2.2d-01'",
		tmp.tm_year+1900,tmp.tm_mon+1,tmp.tm_year+1900,tmp.tm_mon+4);
/*
		"SELECT quotas.project_id,quotas.quota,project.proj_nodetype,project.proj_end FROM quotas JOIN project ON quotas.project_id=project.proj_id WHERE quotas.FK_fcl_id = 'daint' AND project.proj_end >= '2017-04-01'");
*/

	if (mysql_query(cscscon, query)) {
		fprintf(stderr,"%s: mysql_query failed! %s\n",ProgName, query);
		rc = -1;

	} else {
		/*
		 *  store the results
		 */
		result = mysql_store_result(cscscon);
  
		if (result == NULL) {
			fprintf(stderr,"%s: mysql_store_result() returned NULL\n",ProgName);
			rc = -1;

		} else {
			num_rows = mysql_num_rows(result);
			ProjectTableCount = num_rows;

			/*
			 *  Initialize the data storage
			 */
			ProjectTable = (struct ProjectTableEntry *)malloc(num_rows * (sizeof(struct ProjectTableEntry)));
			bzero(ProjectTable,(num_rows * (sizeof(struct ProjectTableEntry))));
			pt = ProjectTable;

			/*
			 *  Extract the data
			 */
			idx = 0;
			while((row = mysql_fetch_row(result))) {
				/*
				 * Store the data
				 *
				 * [0] .. project_id
				 * [1] .. quota
				 * [2] .. nodetype
				 * [3] .. proj_end
				 */

				if ( row[2] == NULL) {
					printf("Project %s is missing nodetype\n",row[0]);
					continue;
				}

				sprintf(pt[idx].pte_projectid,"%s",row[0]);
				pt[idx].pte_quota           = atol(row[1]) * 3;
				sprintf(pt[idx].pte_nodetype,"%s", row[2]);
					
				idx++;
			}

			mysql_free_result(result);
		}
	}

	return(rc);
}

/**
 *  @brief   Project Table Entry
 *  @details Return the next entry in the project table
 *  @retval  rc pointer for success, NULL for failure 
 */
struct	ProjectTableEntry *
GetProjectTableEntry()
{
static	struct ProjectTableEntry *pte;

	if (ProjectTableIndex == ProjectTableCount)
		pte = NULL;
	else
		pte = (struct ProjectTableEntry *)&ProjectTable[ProjectTableIndex++];

	return(pte);
}

/**
 *  @brief   Reset Project Table Index
 *  @details Sets the index into the project table to 0
 */
void
ResetProjectTable()
{
	ProjectTableIndex = 0;
}

/**
 *  @brief   Clear Project Table 
 *  @details Sets the index into the project table to 0
 *           and releases the memory for the Project Table
 */
void 
ClearProjectTable()
{
	free(ProjectTable);
	ProjectTableIndex = 0;
	ProjectTable = (struct ProjectTableEntry *)NULL;
}

/**
 *  @brief   Close the slurm database
 *  @details Close the slurm database
 */
void
CloseDB()
{
	/*
	 *  Close the DB Connection
	 */
	mysql_close(con);

	/*
	 *  Set the indicator to close
	 */
	db_open = 0;
}

/**
 *  @brief   Close the CSCS database
 *  @details Close the CSCS database
 */
void
CloseCSCSDB()
{
	/*
	 *  Close the DB Connection
	 */
	mysql_close(cscscon);

	/*
	 *  Set the indicator to close
	 */
	cscsdb_open = 0;
}


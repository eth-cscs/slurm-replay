
/**
 *  @file  perfrpt.h
 *  @brief Prevent multiple inclusions of the same header file
 */

#ifndef _SYS_STAT_H
#include <sys/stat.h>
#endif

#ifndef PERFRPTS_H
#define PERFRPTS_H

/**
 * @brief Easy trick to get month names
 *        Months[((struct localtime)0)->tm_mon+1]
 *        MONTHS[((struct localtime)0)->tm_mon+1]
 */
static char	*Months[] = {NULL,"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
static char	*MONTHS[] = {NULL,"January","February","March","April","May","June","July","August","September","October","November","December"};

/**
 * @brief List of plot formats to produce
 */
static	char	*PlotFormats[] = {"eps","png",NULL};

/**
 * @brief Internal Job Table Entry Record
 */
struct	JobTableEntry {
	int	jte_jobid;             /**< batch job ID           */
	uid_t	jte_uid;               /**< user ID                */
	time_t	jte_submit;            /**< job submit timestamp   */
	time_t	jte_eligible;          /**< job eligible timestamp */
	time_t	jte_start;             /**< job start timestamp    */
	time_t	jte_end;               /**< job end timestamp      */
	time_t	jte_elapsed;           /**< end - start            */
	int	jte_timelimit;         /**< requested time limit   */
	int	jte_nodes;             /**< node count             */
	int	jte_nodetype;          /**< NODE_MC or NODE_GPU    */
	char	jte_partition[32];     /**< slurm partition name   */
	char	jte_account[32];       /**< job account name       */
	int	jte_mem;               /**< requested memory       */
};

/**
 * @brief Internal Project Table Entry Record
 */
struct	ProjectTableEntry {
	char	pte_projectid[32];     /**< Project ID             */
	int	pte_quota;             /**< Project current quota  */ 
	char	pte_nodetype[32];      /**< Authorized node type   */
};

/**
 * @brief Node Counts per Day
 */
struct	nodesperday {
	time_t	npd_timestamp;		/**< timestamp of change      */
	int	npd_gpunodes;		/**< count of gpu nodes       */
	int	npd_mcrnodes;		/**< count of multicore nodes */
	struct	nodesperday *npd_next;	/**< next element in the lest */
};

/**
 * @brief Custom defined variables
 */
#define NODE_MC    1	/**< MultiCore node */
#define NODE_GPU   2	/**< GPU node       */

/**
 * @brief Flag definitions
 */
#define PR_PLOT    0001	/**< generate plots only      */
#define PR_DATA    0002	/**< generate data files only */
#define PR_VERBOSE 0004  /**< verbose messages        */
#define PR_LARGE   0010  /**< Large Job reports       */
#define PR_UZH     0020  /**< UZH reports             */
#define PR_GENERAL 0040  /**< General Job reports     */
#define PR_TIMERPT 0100  /**< UZH Job Time report     */
#define PR_MAIL    0200  /**< email reports           */

/**
 * @ brief Macro definitions
 */
#define	VERBOSE	 if (Flags & PR_VERBOSE)  /**< display verbose messages */

/**
 *  @ File Permission Modes
 */
static mode_t	GlobalExecMode = S_IRWXU|S_IRWXG;
static mode_t	GlobalPlotMode = S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP;
static mode_t	GlobalUmask    = S_IRWXO;

/**
 * @brief Database Function Prototypes
 */
extern	int  OpenDB(void);
extern	void CloseDB(void);
extern	int  OpenCSCSDB(void);
extern	void CloseCSCSDB(void);

/**
 * @brief Supporting Function Prototypes
 */
extern	void usage(char *);
extern	void version(char *);
extern	char *reportdir(int,int);
extern	int  rdconfig(void);
extern	void HoursInMonth(int,int);
extern	int  GetUZHAlloc(void);

/**
 * @brief Job Extraction Function Prototypes
 */
extern	int    GetJobs(int,int,time_t,time_t,char *);
extern	int    GetProjects(int,int,time_t,time_t);
extern	int    GetAllJobs(time_t,time_t,char *);
extern	int    GetAllProjects(time_t,time_t);
extern	void   ClearJobTable(void);
extern	void   ResetJobTable(void);
extern	void   ClearProjectTable(void);
extern	void   ResetProjectTable(void);
extern	struct JobTableEntry *GetJobTableEntry(void);
extern	struct ProjectTableEntry *GetProjectTableEntry(void);

/**
 * @brief Specialized Report Prototypes
 */
extern	void AllocationRpt(int,int,time_t,time_t,int,int);
extern	void AvailabilityRpt(int,int,time_t,time_t,int,int);

extern	void JobProfileRpt(int,int,time_t,time_t,int,int,char*);
extern	void QueueWaitRpt(int,int,time_t,time_t,int,int);
extern	void EndCountsRpt(int,int,time_t,time_t,int,int);
extern	void StartCountsRpt(int,int,time_t,time_t,int,int);
extern	void SubmitCountsRpt(int,int,time_t,time_t,int,int);
extern	void UtilizationRpt(int,int,time_t,time_t,int,int);
extern	void UZHUtilizationRpt(int,int,time_t,time_t,int,int);
extern	void UZHQueueWaitRpt(int,int,time_t,time_t,int,int);
extern	void UZHTimeRpt(int,int,time_t,time_t,int,int);
extern	void LargeMemSubmitCountsRpt(int,int,time_t,time_t,int,int);
extern	void LargeMemStartCountsRpt(int,int,time_t,time_t,int,int);
extern	void LargeMemEndCountsRpt(int,int,time_t,time_t,int,int);

#endif	/* PERFRPTS_H */


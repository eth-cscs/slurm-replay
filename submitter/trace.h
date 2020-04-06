#ifndef _TRACE_H_
#define _TRACE_H_

#define MAX_CHAR 1024
#define TINYTEXT_SIZE 128
#define TEXT_SIZE 8*1024
#define LARGETEXT_SIZE 16*1024
typedef struct job_trace {
    char account[TINYTEXT_SIZE];
    int exit_code;
    char job_name[TINYTEXT_SIZE];
    int id_job;
    char qos_name[TINYTEXT_SIZE];
    int id_user;
    int id_group;
    char resv_name[TINYTEXT_SIZE];
    char nodelist[LARGETEXT_SIZE];
    int nodes_alloc;
    char partition[TINYTEXT_SIZE];
    char dependencies[TEXT_SIZE];
    int switches;
    int state;
    int timelimit;
    long time_submit;
    long time_eligible;
    long time_start;
    long time_end;
    long time_suspended;
    char gres_alloc[TEXT_SIZE];
    int preset;
    int priority;
} job_trace_t;

typedef struct node_trace {
    long time_start;
    long time_end;
    char node_name[TINYTEXT_SIZE];
    char reason[TINYTEXT_SIZE];
    int state;
    int preset;
} node_trace_t;

typedef struct resv_trace {
    int id_resv;
    long time_start;
    long time_end;
    char nodelist[TEXT_SIZE];
    char resv_name[TEXT_SIZE];
    char accts[TEXT_SIZE];
    char tres[TEXT_SIZE];
    int flags;
    int preset;
} resv_trace_t;

#endif

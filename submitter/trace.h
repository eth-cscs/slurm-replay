#ifndef _TRACE_H_
#define _TRACE_H_

#define MAX_CHAR 1024
#define TINYTEXT_SIZE 10
#define SMALLTEXT_SIZE 128
#define MEDTEXT_SIZE 2*1024
#define LARGETEXT_SIZE 16*1024
typedef struct __attribute__((__packed__)) job_trace {
    char account[TINYTEXT_SIZE];
    int exit_code;
    char job_name[SMALLTEXT_SIZE];
    int id_job;
    char qos_name[TINYTEXT_SIZE];
    int id_user;
    int id_group;
    char resv_name[MEDTEXT_SIZE];
    char nodelist[MEDTEXT_SIZE];
    int nodes_alloc;
    char partition[TINYTEXT_SIZE];
    char dependencies[LARGETEXT_SIZE];
    int switches;
    int state;
    int timelimit;
    long time_submit;
    long time_eligible;
    long time_start;
    long time_end;
    long time_suspended;
    char gres_alloc[TINYTEXT_SIZE];
    int preset;
    int priority;
    char user[TINYTEXT_SIZE];
} job_trace_t;

typedef struct node_trace {
    long time_start;
    long time_end;
    char node_name[TINYTEXT_SIZE];
    char reason[SMALLTEXT_SIZE];
    int state;
    int preset;
} node_trace_t;

typedef struct resv_trace {
    int id_resv;
    long time_start;
    long time_end;
    char nodelist[MEDTEXT_SIZE];
    char resv_name[MEDTEXT_SIZE];
    char accts[SMALLTEXT_SIZE];
    char tres[SMALLTEXT_SIZE];
    int flags;
    int preset;
} resv_trace_t;

#endif

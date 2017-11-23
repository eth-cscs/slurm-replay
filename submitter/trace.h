#ifndef _TRACE_H_
#define _TRACE_H_


#define TINYTEXT_SIZE 32
#define TEXT_SIZE 1512

typedef struct job_trace {
    char account[TINYTEXT_SIZE];
    int cpus_req;
    int exit_code;
    char job_name[TINYTEXT_SIZE];
    int id_job;
    int id_qos;
    int id_user;
    int id_group;
    char resv_name[TINYTEXT_SIZE];
    unsigned long mem_req;
    char nodelist[TEXT_SIZE];
    int nodes_alloc;
    char partition[TINYTEXT_SIZE];
    int priority;
    int state;
    int timelimit;
    unsigned long time_submit;
    unsigned long time_eligible;
    unsigned long time_start;
    unsigned long time_end;
    unsigned long time_suspended;
    char gres_req[TEXT_SIZE];
    char gres_alloc[TEXT_SIZE];
    char tres_req[TEXT_SIZE];
} job_trace_t;

typedef struct node_trace {
    unsigned long time_start;
    unsigned long time_end;
    char node_name[TINYTEXT_SIZE];
    char reason[TINYTEXT_SIZE];
    int state;
} node_trace_t;

typedef struct resv_trace {
    int id_resv;
    unsigned long time_start;
    unsigned long time_end;
    char nodelist[TEXT_SIZE];
    char resv_name[TEXT_SIZE];
    char accts[TEXT_SIZE];
    int flags;
} resv_trace_t;

#endif

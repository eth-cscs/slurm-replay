#ifndef _TRACE_H_
#define _TRACE_H_


#define TINYTEXT_SIZE 64
#define TEXT_SIZE 4096

typedef struct job_trace {
    char account[TINYTEXT_SIZE];
    int cpus_req;
    int exit_code;
    char job_name[TINYTEXT_SIZE];
    int id_job;
    int id_qos;
    int id_user;
    int id_group;
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
    struct job_trace *next;
} job_trace_t;

typedef struct rsv_trace {
    long int           creation_time;
    char *             rsv_command;
    struct rsv_trace * next;
} rsv_trace_t;



#endif

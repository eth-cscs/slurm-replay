#ifndef __SIM_TRACE_H_
#define __SIM_TRACE_H_

#define MAX_USERNAME_LEN 30
#define MAX_RSVNAME_LEN  30
#define MAX_QOSNAME      30
#define TIMESPEC_LEN     30
#define MAX_RSVNAME      30

typedef struct job_trace {
    int  job_id;
    char username[MAX_USERNAME_LEN];
    unsigned long submit; /* relative or absolute? */
    unsigned long duration;
    int  wclimit;
    int  tasks;
    char qosname[MAX_QOSNAME];
    char partition[MAX_QOSNAME];
    char account[MAX_QOSNAME];
    int  cpus_per_task;
    int  tasks_per_node;
    char reservation[MAX_RSVNAME];
    char dependency[MAX_RSVNAME];
    struct job_trace *next;
} job_trace_t;

typedef struct rsv_trace {
    long int           creation_time;
    char *             rsv_command;
    struct rsv_trace * next;
} rsv_trace_t;

#endif

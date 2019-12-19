#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <getopt.h>

#include "trace.h"

static int time_format = 0;
static int reservation = 0;
static int event = 0;
static int noheader = 0;
static int display_query = 0;
char* workload_trace_file = NULL;
char* special_action = NULL;
char help_msg[] = "\
list_trace [OPTIONS]\n\
    -q, --query                   Print the SQL query used to generate the trace\n\
    -n, --noheader                Do not print headers\n\
    -w, --wrkldfile filename      The name of the trace file to view\n\
    -u, --humantime               Display submit time in a human-readable\n\
    -r, --reservation             Display reservation information\n\
    -e, --event                   Display node event information\n\
    -s, --special                 Do a special action define in the code\n\
    -h, --help                    This help message.\n";


enum job_states {
    JOB_PENDING,        /* queued waiting for initiation */
    JOB_RUNNING,        /* allocated resources and executing */
    JOB_SUSPENDED,      /* allocated resources, execution suspended */
    JOB_COMPLETE,       /* completed execution successfully */
    JOB_CANCELLED,      /* cancelled by user */
    JOB_FAILED,     /* completed execution unsuccessfully */
    JOB_TIMEOUT,        /* terminated on reaching time limit */
    JOB_NODE_FAIL,      /* terminated on node failure */
    JOB_PREEMPTED,      /* terminated due to preemption */
    JOB_BOOT_FAIL,      /* terminated due to node boot failure */
    JOB_DEADLINE,       /* terminated on deadline */
    JOB_OOM,        /* experienced out of memory error */
    JOB_END         /* not a real state, last entry in table */
};

static char *job_state_string(int inx)
{
    switch (inx) {
    case JOB_PENDING:
        return "PENDING";
    case JOB_RUNNING:
        return "RUNNING";
    case JOB_SUSPENDED:
        return "SUSPENDED";
    case JOB_COMPLETE:
        return "COMPLETED";
    case JOB_CANCELLED:
        return "CANCELLED";
    case JOB_FAILED:
        return "FAILED";
    case JOB_TIMEOUT:
        return "TIMEOUT";
    case JOB_NODE_FAIL:
        return "NODE_FAIL";
    case JOB_PREEMPTED:
        return "PREEMPTED";
    case JOB_BOOT_FAIL:
        return "BOOT_FAIL";
    case JOB_DEADLINE:
        return "DEADLINE";
    case JOB_OOM:
        return "OUT_OF_MEMORY";
    default:
        return "?";
    }
}

void getArgs(int argc, char** argv)
{
    static struct option long_options[] = {
        {"wrkldfile",      1, 0, 'w'},
        {"query",          0, 0, 'q'},
        {"noheader",       0, 0, 'n'},
        {"reservation",    0, 0, 'r'},
        {"event",    0, 0, 'e'},
        {"special",    1, 0, 's'},
        {"humantime",    0, 0, 'u'},
        {"help",           0, 0, 'h'},
    };
    int opt_char, option_index;

    while (1) {
        if ( (opt_char = getopt_long(argc, argv, "w:nurhqes:", long_options,
                                     &option_index)) == -1 )
            break;
        switch (opt_char) {
        case ('w'):
            workload_trace_file = strdup(optarg);
            break;
        case ('n'):
            noheader = 1;
            break;
        case ('q'):
            display_query = 1;
            break;
        case ('r'):
            reservation = 1;
            break;
        case ('e'):
            event = 1;
            break;
        case ('u'):
            time_format = 1;
            break;
        case ('s'):
            special_action = strdup(optarg);
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        };
    }
}

void do_special_action(job_trace_t* job_arr, unsigned long long num_rows)
{
    unsigned long long k, idx = 0;
    unsigned long* job_id;
    int list_file;
    long duration;
    double fact;

    job_id = (unsigned long*)malloc(sizeof(unsigned long)*num_rows);

    for(k = 0; k < num_rows; k++) {
        duration = job_arr[k].time_end - job_arr[k].time_start;
        fact = (double)duration / (double)(job_arr[k].timelimit*60);
        if ( fact < 0.5 && job_arr[k].state == 3) {
            job_id[idx] = job_arr[k].id_job;
            idx++;
            printf("%d %ld %d %.2f\n",job_arr[k].id_job, duration, job_arr[k].timelimit*60, fact);
        }
    }
    printf("number of special jobs: %lld\n", idx);
    list_file = open(special_action,  O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if ( list_file < 0) {
        printf("special: data not written, error %d: %s\n",list_file, strerror(list_file));
    } else {
        write(list_file, &idx, sizeof(unsigned long long));
        write(list_file, job_id, idx*sizeof(unsigned long long));
    }
}

int main(int argc, char *argv[])
{
    int trace_file = -1;
    job_trace_t *job_arr;
    resv_trace_t *resv_arr;
    node_trace_t *node_arr;
    char submit[20];
    char start[20];
    char eligible[20];
    char end[20];
    const char *qosname = "normal";
    size_t query_length = 0;
    char query[1024];
    unsigned long long num_rows;
    unsigned long long k;
    memset(query,'\0',1024);

    getArgs(argc, argv);

    if ( workload_trace_file == NULL) {
        printf("%s\n", help_msg);
        exit(-1);
    }

    if ( (trace_file = open(workload_trace_file, O_RDONLY)) < 0 ) {
        printf("Error opening workload file: %s\nAbort!\n",
               workload_trace_file);
        exit(-1);
    }

    read(trace_file, &query_length, sizeof(size_t));
    read(trace_file, query, query_length*sizeof(char));

    read(trace_file, &num_rows, sizeof(unsigned long long));

    if (display_query) {
        printf("%s\n",query);
    }

    if (!noheader) {
        printf("\t%10s \t%10s \t%8s \t%10s \t%19s \t%19s \t%19s \t%19s \t%10s \t%8s \t%8s \t%10s \t%10s \t%10s \t%10s \t%10s \t%10s \t%10s \t%8s\n",
               "JOBID", "ACCOUNT", "PRIORITY", "DURATION", "SUBMIT", "ELIGIBLE", "START", "END", "TIMELIMIT", "NODES", "EXITCODE", "STATE", "RESERVATION", "USERID", "NODELIST", "PARTITION","QOS","DEPENDENCIES","SWITCHES");
        printf("\t%10s \t%10s \t%8s \t%10s \t%19s \t%19s \t%19s \t%19s \t%10s \t%8s \t%8s \t%10s \t%10s \t%10s \t%10s \t%10s \t%10s \t%10s \t%8s\n",
               "=====", "=======", "========", "========", "======", "========", "=====", "===", "=========", "=====", "========", "=====", "===========", "======", "========", "=========","===","============","=========");
    }

    job_arr = (job_trace_t*)malloc(sizeof(job_trace_t)*num_rows);
    read(trace_file, job_arr, sizeof(job_trace_t)*num_rows);


    for(k = 0; k < num_rows; k++) {
        if (time_format) {
            strftime(submit, sizeof(submit), "%Y-%m-%d %H:%M:%S", localtime(&job_arr[k].time_submit));
            strftime(start, sizeof(start), "%Y-%m-%d %H:%M:%S", localtime(&job_arr[k].time_start));
            strftime(eligible, sizeof(eligible), "%Y-%m-%d %H:%M:%S", localtime(&job_arr[k].time_eligible));
            strftime(end, sizeof(end), "%Y-%m-%d %H:%M:%S", localtime(&job_arr[k].time_end));
        } else {
            sprintf(submit, "%ld", job_arr[k].time_submit);
            sprintf(start, "%ld", job_arr[k].time_start);
            sprintf(eligible, "%ld", job_arr[k].time_eligible);
            sprintf(end, "%ld", job_arr[k].time_end);
        }

        printf("\t%10d \t%10s \t%8d \t%10ld \t%19s \t%19s \t%19s \t%19s \t%10d \t%8d \t%8d \t%10s \t%10s \t%10d \t%10s \t%10s \t%10s \t%10s \t%8d \t%s\n",
               job_arr[k].id_job,
               job_arr[k].account,
               job_arr[k].priority,
               job_arr[k].time_end - job_arr[k].time_start,
               submit,
               eligible,
               start,
               end,
               job_arr[k].timelimit*60,
               job_arr[k].nodes_alloc,
               job_arr[k].exit_code,
               job_state_string(job_arr[k].state),
               job_arr[k].resv_name,
               job_arr[k].id_user,
               job_arr[k].nodelist,
               job_arr[k].partition,
               job_arr[k].qos_name,
               job_arr[k].dependencies,
               job_arr[k].switches,
               job_arr[k].gres_alloc);
    }

    if (special_action) {
        do_special_action(job_arr, num_rows);
    }

    free(job_arr);

    read(trace_file, &num_rows, sizeof(unsigned long long));
    if (reservation && num_rows > 0) {

        if (!noheader) {
            printf("\t%8s \t%10s \%10s \t%19s \t%19s \t%8s \t%8s \t%10s\n",
                   "RESID", "ACCOUNT", "NAME", "START", "END", "NODELIST", "FLAGS", "TRES");
            printf("\t%8s \t%10s \%10s \t%19s \t%19s \t%8s \t%8s \t%10s\n",
                   "=====", "=======", "====", "=====", "===", "========", "=====", "====");
        }

        resv_arr = (resv_trace_t*)malloc(sizeof(resv_trace_t)*num_rows);
        read(trace_file, resv_arr, sizeof(resv_trace_t)*num_rows);

        for(k = 0; k < num_rows; k++) {
            if (time_format) {
                strftime(start, sizeof(start), "%Y-%m-%d %H:%M:%S", localtime(&resv_arr[k].time_start));
                strftime(end, sizeof(end), "%Y-%m-%d %H:%M:%S", localtime(&resv_arr[k].time_end));
            } else {
                sprintf(start, "%ld", resv_arr[k].time_start);
                sprintf(end, "%ld", resv_arr[k].time_end);
            }

            printf("\t%8d \t%10s \t%10s \t%19s \t%19s \t%8s \t%8d \t%10s\n",
                   resv_arr[k].id_resv,
                   resv_arr[k].accts,
                   resv_arr[k].resv_name,
                   start,
                   end,
                   resv_arr[k].nodelist,
                   resv_arr[k].flags,
                   resv_arr[k].tres);
        }
        free(resv_arr);
    } else {
        if (num_rows > 0) {
            lseek(trace_file, num_rows*sizeof(resv_trace_t), SEEK_CUR);
        }
    }

    read(trace_file, &num_rows, sizeof(unsigned long long));
    if (event && num_rows > 0) {

        if (!noheader) {
            printf("\t%19s \t%19s \t%10s \t%10s \t%8s\n",
                   "START", "END", "NODENAME", "REASON", "STATE");
            printf("\t%19s \t%19s \t%10s \t%10s \t%8s\n",
                   "=====", "===", "========", "======", "=====");
        }


        node_arr = (node_trace_t*)malloc(sizeof(node_trace_t)*num_rows);
        read(trace_file, node_arr, sizeof(node_trace_t)*num_rows);

        for(k = 0; k < num_rows; k++) {
            if (time_format) {
                strftime(start, sizeof(start), "%Y-%m-%d %H:%M:%S", localtime(&node_arr[k].time_start));
                strftime(end, sizeof(end), "%Y-%m-%d %H:%M:%S", localtime(&node_arr[k].time_end));
            } else {
                sprintf(start, "%ld", node_arr[k].time_start);
                sprintf(end, "%ld", node_arr[k].time_end);
            }

            printf("\t%19s \t%19s \t%10s \t%10s \t%8d\n",
                   start,
                   end,
                   node_arr[k].node_name,
                   node_arr[k].reason,
                   node_arr[k].state);
        }
        free(node_arr);
    }

    close(trace_file);
    return 0;
}

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
static int noheader = 0;
static int display_query = 0;
char* workload_trace_file = NULL;
char help_msg[] = "\
list_trace [OPTIONS]\n\
    -q, --query                   Print the SQL query used to generate the trace\n\
    -n, --noheader                Do not print headers\n\
    -w, --wrkldfile filename      The name of the trace file to view\n\
    -u, --humantime               Display submit time in a human-readable\n\
    -r, --reservation             Display reservation information\n\
    -h, --help                    This help message.\n";

void getArgs(int argc, char** argv)
{
    static struct option long_options[] = {
        {"wrkldfile",      1, 0, 'w'},
        {"query",          0, 0, 'q'},
        {"noheader",       0, 0, 'n'},
        {"reservation",    0, 0, 'r'},
        {"humantime",    0, 0, 'u'},
        {"help",           0, 0, 'h'},
    };
    int opt_char, option_index;

    while (1) {
        if ( (opt_char = getopt_long(argc, argv, "w:nurhq", long_options,
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
        case ('u'):
            time_format = 1;
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        };
    }
}

int main(int argc, char *argv[])
{
    int trace_file = -1;
    job_trace_t *job_arr;
    resv_trace_t *resv_arr;
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
        printf("\t%10s \t%10s \t%10s \t%10s \t%19s \t%19s \t%19s \t%19s \t%10s \t%8s \t%8s \t%8s \t%8s \t%10s\n",
               "JOBID", "USERID", "ACCOUNT", "DURATION", "SUBMIT", "ELIGIBLE", "START", "END", "TIMELIMIT", "NODES", "EXITCODE", "STATE", "RESERVATION");
        printf("\t%10s \t%10s \t%10s \t%10s \t%19s \t%19s \t%19s \t%19s \t%10s \t%8s \t%8s \t%8s \t%10s\n",
               "=====", "======", "=======", "========", "======", "========", "=====", "===", "=========", "=====", "========", "=====", "===========");
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

        printf("\t%10d \t%10d \t%10s \t%10d \t%19s \t%19s \t%19s \t%19s \t%10d \t%8d \t%8d \t%8d \t%10s\n",
               job_arr[k].id_job,
               job_arr[k].id_user,
               job_arr[k].account,
               job_arr[k].time_end - job_arr[k].time_start,
               submit,
               eligible,
               start,
               end,
               job_arr[k].timelimit*60,
               job_arr[k].nodes_alloc,
               job_arr[k].exit_code,
               job_arr[k].state,
               job_arr[k].resv_name);
    }

    if (reservation) {

        if (!noheader) {
            printf("\t%8s \t%10s \%10s \t%19s \t%19s \t%8s \t%8s\n",
                   "RESID", "ACCOUNT", "NAME", "START", "END", "NODELIST", "FLAGS");
            printf("\t%8s \t%10s \%10s \t%19s \t%19s \t%8s \t%8s\n",
                   "=====", "=======", "====", "=====", "===", "========", "=====");
        }

        read(trace_file, &num_rows, sizeof(unsigned long long));

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

            printf("\t%8d \t%10s \t%10s \t%19s \t%19s \t%8s \t%8d\n",
                   resv_arr[k].id_resv,
                   resv_arr[k].accts,
                   resv_arr[k].resv_name,
                   start,
                   end,
                   resv_arr[k].nodelist,
                   resv_arr[k].flags);
        }
        free(resv_arr);
    }

    free(job_arr);
    return 0;
}



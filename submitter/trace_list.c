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

static int time_format = 0;           /* Default to Unix Epoch Format */
static int noheader = 0;
char* workload_trace_file = NULL;
char default_trace_file[] = "test.trace";
char help_msg[] = "\
list_trace [OPTIONS]\n\
    -n, --noheader                Do not print headers\n\
    -w, --wrkldfile filename      The name of the trace file to view\n\
    -u, --unixtime                Display submit time in Unix epoch format\n\
                                  Default\n\
    -r, --humantime               Display submit time in a human-readable\n\
                                  format (YYYY-MM-DD hh:mm:ss)\n\
    -h, --help                    This help message.\n";

int getArgs(int argc, char** argv);

static unsigned long slurmdb_find_tres_count_in_string(char *tres_str_in, int id)
{
    const int INFINITE64 = 0;
    char *tmp_str = tres_str_in;

    if (!tmp_str || !tmp_str[0])
        return INFINITE64;

    while (tmp_str) {
        if (id == atoi(tmp_str)) {
            if (!(tmp_str = strchr(tmp_str, '='))) {
                printf("slurmdb_find_tres_count_in_string: no value found\n");
                break;
            }
            return strtoul(++tmp_str,NULL,10);
        }


        if (!(tmp_str = strchr(tmp_str, ',')))
            break;
        tmp_str++;
    }

    return INFINITE64;
}


int main(int argc, char *argv[])
{
    int tasks;
    int trace_file, record_idx = 0;
    job_trace_t job_trace;
    char submit[20];
    char start[20];
    char eligible[20];
    char end[20];
    const char *qosname = "normal";

    if ( !getArgs(argc, argv) ) {
        printf("Usage: %s\n", help_msg);
        exit(-1);
    }

    if ( (trace_file = open(workload_trace_file, O_RDONLY)) < 0 ) {
        printf("Error opening workload file: %s\nAbort!\n",
               workload_trace_file);
        exit(-1);
    }
    if (!noheader) {
        printf("\t%10s \t%10s \t%10s \t%10s \t%19s \t%19s \t%19s \t%19s \t%10s \t%8s \t%8s \t%8s \t%8s\n",
               "JOBID", "USERID", "ACCOUNT", "DURATION", "SUBMIT", "ELIGIBLE", "START", "END", "TIMELIMIT", "TASKS", "NODES", "EXITCODE", "STATE");

        printf("\t%10s \t%10s \t%10s \t%10s \t%19s \t%19s \t%19s \t%19s \t%10s \t%8s \t%8s \t%8s \t%8s\n",
               "=====", "======", "=======", "========", "======", "========", "=====", "===", "=========", "=====", "=====", "========", "=====");
    }
    while (read(trace_file, &job_trace, sizeof(job_trace))) {
        ++record_idx;

        if (time_format) {
            strftime(submit, sizeof(submit), "%Y-%m-%d %H:%M:%S", localtime(&job_trace.time_submit));
            strftime(start, sizeof(start), "%Y-%m-%d %H:%M:%S", localtime(&job_trace.time_start));
            strftime(eligible, sizeof(eligible), "%Y-%m-%d %H:%M:%S", localtime(&job_trace.time_eligible));
            strftime(end, sizeof(end), "%Y-%m-%d %H:%M:%S", localtime(&job_trace.time_end));
        } else {
            sprintf(submit, "%ld", job_trace.time_submit);
            sprintf(start, "%ld", job_trace.time_start);
            sprintf(eligible, "%ld", job_trace.time_eligible);
            sprintf(end, "%ld", job_trace.time_end);
        }

        tasks = slurmdb_find_tres_count_in_string(job_trace.tres_req,1);
        printf("\t%10d \t%10d \t%10s \t%10d \t%19s \t%19s \t%19s \t%19s \t%10d \t%8d \t%8d \t%8d \t%8d\n",
               job_trace.id_job,
               job_trace.id_user,
               job_trace.account,
               job_trace.time_end - job_trace.time_start,
               submit,
               eligible,
               start,
               end,
               job_trace.timelimit*60,
               tasks,
               job_trace.nodes_alloc,
               job_trace.exit_code,
               job_trace.state);
    }

    return 0;
}

int
getArgs(int argc, char** argv)
{
    static struct option long_options[] = {
        {"wrkldfile",      1, 0, 'w'},
        {"noheader",       0, 0, 'n'},
        {"unixtime",       0, 0, 'u'},
        {"humantime",      0, 0, 'r'},
        {"help",           0, 0, 'h'},
    };
    int opt_char, option_index;
    int valid = 1;

    while (1) {
        if ( (opt_char = getopt_long(argc, argv, "w:nurh", long_options,
                                     &option_index)) == -1 )
            break;
        switch (opt_char) {
        case ('w'):
            workload_trace_file = strdup(optarg);
            break;
        case ('n'):
            noheader = 1;
            break;
        case ('u'):
            time_format = 0;
            break;
        case ('r'):
            time_format = 1;
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        };
    }

    if (!workload_trace_file) workload_trace_file = default_trace_file;

    return valid;
}

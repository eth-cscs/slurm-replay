#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>

#include "trace.h"

char* workload_trace_file = NULL;
char* replay_trace_file = NULL;
double threshold = 0.0;
int check_sanity = 0;
char help_msg[] = "\
list_trace [OPTIONS]\n\
    -w, --wrkldfile filename      The name of the trace file with the original data\n\
    -r, --replay    filename      The name of the trace file with the replayed data\n\
    -f, --field     field         Field to compare trace: duration, submit, eligible, start, end, nodes, state\n\
    -d, --differ     value        Threshold from which to report a difference\n\
    -s, --sanity                  Check nodes and state\n\
    -h, --help                    This help message.\n";

void getArgs(int argc, char** argv)
{
    static struct option long_options[] = {
        {"wrkldfile",      1, 0, 'w'},
        {"replay",         1, 0, 'r'},
        {"differ",          1, 0, 'd'},
        {"sannity",         0, 0, 's'},
        {"help",           0, 0, 'h'},
    };
    int opt_char, option_index;

    while (1) {
        if ( (opt_char = getopt_long(argc, argv, "w:r:d:hs", long_options,
                                     &option_index)) == -1 )
            break;
        switch ( opt_char) {
        case ('w'):
            workload_trace_file = strdup(optarg);
            break;
        case ('r'):
            replay_trace_file = strdup(optarg);
            break;
        case ('d'):
            threshold = strtod(optarg,NULL);
            break;
        case ('s'):
            check_sanity = 1;
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        };
    }
}


static inline double euclidean_dist(time_t x, time_t y)
{
    return sqrt( (x-y)*(x-y) );
}

int main(int argc, char *argv[])
{
    struct stat stat_buf;
    int work_file, replay_file;
    unsigned long long j, min_jobs, nwork_jobs, nreplay_jobs;
    double dist;
    time_t t1, t2;
    job_trace_t jwt; // job_work_trace
    job_trace_t jrt; // job_replay_trace
    time_t jwt_min_start = LONG_MAX;//for makespan
    time_t jwt_max_end = 0; // for makespan
    time_t jrt_min_start = LONG_MAX;//for makespan
    time_t jrt_max_end = 0; // for makespan
    size_t query_length = 0;
    char query[1024];

    getArgs(argc, argv);
    if (workload_trace_file == NULL || replay_trace_file == NULL) {
        printf("%s\n", help_msg);
        exit(-1);
    }

    if ( (work_file = open(workload_trace_file, O_RDONLY)) < 0 ) {
        printf("Error opening workload file: %s\nAbort!\n",
               workload_trace_file);
        exit(-1);
    }

    if ( (replay_file = open(replay_trace_file, O_RDONLY)) < 0 ) {
        printf("Error opening workload file: %s\nAbort!\n",
               replay_trace_file);
        exit(-1);
    }

    read(work_file, &query_length, sizeof(size_t));
    read(work_file, query, query_length*sizeof(char));
    read(work_file, &nwork_jobs, sizeof(unsigned long long));

    read(replay_file, &query_length, sizeof(size_t));
    read(replay_file, query, query_length*sizeof(char));
    read(replay_file, &nreplay_jobs, sizeof(unsigned long long));


    min_jobs = nreplay_jobs;
    if (nwork_jobs < min_jobs) {
        min_jobs = nwork_jobs;
    }

    for(j = 0; j < min_jobs; j++) {
        read(work_file, &jwt, sizeof(job_trace_t));
        read(replay_file, &jrt, sizeof(job_trace_t));

        if (jwt.id_job != jrt.id_job) {
            printf("Missmatch of job id (%d != %d), files are out of synch. Exiting.\n",jwt.id_job, jrt.id_job);
            exit(-1);
        }

        if (check_sanity) {
            if ( jwt.nodes_alloc != jrt.nodes_alloc ) {
                printf("[%d] Number of nodes differ: %d <> %d\n",jwt.id_job, jwt.nodes_alloc, jrt.nodes_alloc);
            }
            if ( strcmp(jwt.nodelist, jrt.nodelist) != 0 ) {
                printf("[%d] List of nodes differ.\n",jwt.id_job);
            }
            /*if ( jwt.state != jrt.state ) {
                printf("[%d] Job state differ: %d <> %d\n",jwt.id_job, jwt.state, jrt.state);
            }*/
        }

        // makepsan
        if (jwt.time_start < jwt_min_start) {
            jwt_min_start = jwt.time_start;
        }
        if (jwt.time_end > jwt_max_end) {
            jwt_max_end = jwt.time_end;
        }
        if (jrt.time_start < jrt_min_start) {
            jrt_min_start = jrt.time_start;
        }
        if (jrt.time_end > jrt_max_end) {
            jrt_max_end = jrt.time_end;
        }

        t1 = jwt.time_end - jwt.time_start;
        t2 = jrt.time_end - jrt.time_start;
        dist = euclidean_dist(t1,t2);
        if (dist > threshold) {
            printf("[%d] distance duration over threshold (%.2f)\n",jwt.id_job, dist);
        }
        t1 = jwt.time_submit;
        t2 = jrt.time_submit;
        dist = euclidean_dist(t1,t2);
        if (dist > threshold) {
            printf("[%d] distance time_submit over threshold (%.2f)\n",jwt.id_job, dist);
        }
        t1 = jwt.time_eligible;
        t2 = jrt.time_eligible;
        dist = euclidean_dist(t1,t2);
        if (dist > threshold) {
            printf("[%d] distance time_eligible over threshold (%.2f)\n",jwt.id_job, dist);
        }
        t1 = jwt.time_start;
        t2 = jrt.time_start;
        dist = euclidean_dist(t1,t2);
        if (dist > threshold) {
            printf("[%d] distance time_start over threshold (%.2f)\n",jwt.id_job, dist);
        }
        t1 = jwt.time_end;
        t2 = jrt.time_end;
        dist = euclidean_dist(t1,t2);
        if (dist > threshold) {
            printf("[%d] distance time_end over threshold (%.2f)\n",jwt.id_job, dist);
        }
    }

    printf("Makespan of workload: %ld\n", jwt_max_end - jwt_min_start);
    printf("Makespan of replay: %ld\n", jrt_max_end - jrt_min_start);
    printf("Makespan differences: %ld\n", (jwt_max_end - jwt_min_start) -  (jrt_max_end - jrt_min_start));
    return 0;
}

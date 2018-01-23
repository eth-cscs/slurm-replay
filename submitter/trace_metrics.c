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

#define STRIDE 1

char* workload_trace_file = NULL;
char help_msg[] = "\
list_trace [OPTIONS]\n\
    -w, --wrkldfile filename      The name of the trace file with the original data\n\
    -h, --help                    This help message.\n";

void getArgs(int argc, char** argv)
{
    static struct option long_options[] = {
        {"wrkldfile",      1, 0, 'w'},
        {"help",           0, 0, 'h'}
    };
    int opt_char, option_index;

    while (1) {
        if ( (opt_char = getopt_long(argc, argv, "w:h", long_options,
                                     &option_index)) == -1 )
            break;
        switch ( opt_char) {
        case ('w'):
            workload_trace_file = strdup(optarg);
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        };
    }
}

void compute_metrics(job_trace_t *job_arr, unsigned long long njobs, unsigned long long Nnodes, long *makespan, double *util, double *avg_wait, double *std_wait, long *min_wait, long *max_wait, long *nwait, double *dispersion, double *slowdown) {
    unsigned long long j;
    long *time_submit_arr;
    long *time_exec_arr; // end-start
    long *time_wait_arr; // start-eligible
    long *time_end_arr;
    int *nodes_alloc_arr;
    long time_end_max = 0;
    long time_submit_min = LONG_MAX;
    long cum_exec;
    long cum_wait;
    long cum_stddev;
    double bsd;

    *max_wait = 0;
    *min_wait = LONG_MAX;

    time_submit_arr = (long*)malloc(sizeof(long)*njobs);
    time_exec_arr = (long*)malloc(sizeof(long)*njobs);
    time_wait_arr = (long*)malloc(sizeof(long)*njobs);
    time_end_arr = (long*)malloc(sizeof(long)*njobs);
    nodes_alloc_arr = (int*)malloc(sizeof(int)*njobs);

    for(j = 0; j < njobs; j++) {
        time_submit_arr[j] = job_arr[j].time_submit;
        time_exec_arr[j] = job_arr[j].time_end - job_arr[j].time_start;
        time_wait_arr[j] = job_arr[j].time_start - job_arr[j].time_eligible;
        if ( time_wait_arr[j] < 0 ) {
            printf("%d %ld %ld %ld\n",job_arr[j].id_job, job_arr[j].time_submit, job_arr[j].time_eligible, job_arr[j].time_start);
        }
        time_end_arr[j] = job_arr[j].time_end;
        nodes_alloc_arr[j] = job_arr[j].nodes_alloc;
        if(time_end_arr[j] > time_end_max) {
            time_end_max = time_end_arr[j];
        }
        if(time_submit_arr[j] < time_submit_min) {
            time_submit_min = time_submit_arr[j];
        }
    }

    // Utilization
    // makespan
    *makespan = time_end_max - time_submit_min;
    // avg utilization
    cum_exec = 0;
    for(j = 0; j < njobs; j++) {
        cum_exec += time_exec_arr[j]*nodes_alloc_arr[j];
    }
    *util = (double)(cum_exec) / (double)((*makespan)*Nnodes);
    //cum_stddev = 0;
    //for(j = 0; j < njobs; j++) {
    //    cum_stddev += (time_exec_arr[j]*nodes_alloc_arr[j] - *avg_util)*(time_exec_arr[j]*nodes_alloc_arr[j] - *avg_util);
    //}
    //*std_util = sqrt((double)(cum_stddev)/(double)((*makespan)*Nnodes));
    // Responsiveness
    // cumulative completion
    //*cum_completion = 0;
    //for(j = 0; j < njobs; j++) {
    //    *cum_completion += (1 + (*makespan) - time_end_arr[j])*time_exec_arr[j]*nodes_alloc_arr[j];
    //}
    cum_wait = 0;
    for(j = 0; j < njobs; j++) {
        if(time_wait_arr[j] > 1) {
            cum_wait += time_wait_arr[j];
            if(time_wait_arr[j] > *max_wait) {
                *max_wait = time_wait_arr[j];
            }
            if(time_wait_arr[j] < *min_wait) {
                *min_wait = time_wait_arr[j];
            }
            (*nwait)++;
        }
    }
    *avg_wait = (double)(cum_wait)/(double)(*nwait);
    cum_stddev = 0;
    for(j = 0; j < njobs; j++) {
        cum_stddev += (time_wait_arr[j] - *avg_wait)*(time_wait_arr[j] - *avg_wait);
    }
    *std_wait = sqrt((double)(cum_stddev)/(double)(*nwait));

    // Fairness
    // dispersion
    *dispersion = 1.0 / (1.0 + (double)(*std_wait / *avg_wait));
    bsd = 0.0;
    for(j = 0; j < njobs; j++) {
        bsd = (double)(time_wait_arr[j]+time_exec_arr[j])/((double)time_exec_arr[j]);
    }
    *slowdown = ((double)bsd)/((double)njobs);

    free(time_submit_arr);
    free(time_exec_arr);
    free(time_wait_arr);
    free(time_end_arr);
    free(nodes_alloc_arr);
}

int main(int argc, char *argv[])
{
    int trace_file;
    job_trace_t *job_arr;
    job_trace_t *job_arr_all;
    job_trace_t *job_arr_mc;
    job_trace_t *job_arr_gpu;
    size_t query_length = 0;
    char query[1024];
    unsigned long long njobs, njobs_all, njobs_mc, njobs_gpu, j;
    long makespan;
    //long cum_completion;
    double util;
    double avg_wait;
    double std_util;
    double std_wait;
    long min_wait;
    long max_wait;
    long nwait = 0;
    double dispersion;
    double slowdown;
    int Nnodes_mc  = 1813;
    int Nnodes_gpu  = 5320;
    int Nnodes  = Nnodes_mc+Nnodes_gpu+2; // all daint + data movers


    getArgs(argc, argv);

    if (workload_trace_file == NULL) {
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
    read(trace_file, &njobs, sizeof(unsigned long long));

    job_arr = (job_trace_t*)malloc(sizeof(job_trace_t)*njobs);
    job_arr_all = (job_trace_t*)malloc(sizeof(job_trace_t)*njobs);
    job_arr_mc = (job_trace_t*)malloc(sizeof(job_trace_t)*njobs);
    job_arr_gpu = (job_trace_t*)malloc(sizeof(job_trace_t)*njobs);
    if (!job_arr) {
        printf("unable to allocate memory for all job records.\n");
        return -1;
    }
    read(trace_file, job_arr, sizeof(job_trace_t)*njobs);

    njobs_mc = 0;
    njobs_gpu = 0;
    njobs_all = 0;
    for(j = 0; j < njobs; j++) {
        if (strcmp("normal",job_arr[j].partition) != 0 && strcmp("xfer",job_arr[j].partition) != 0) {
            continue;
        }
        memcpy(&job_arr_all[njobs_all], &job_arr[j], sizeof(job_trace_t));
        njobs_all++;
        if (strcmp(job_arr[j].partition, "xfer") != 0) {
            if (strncmp("gpu:0", job_arr[j].gres_alloc, 5) == 0) {
                memcpy(&job_arr_mc[njobs_mc], &job_arr[j], sizeof(job_trace_t));
                njobs_mc++;
            } else {
                memcpy(&job_arr_gpu[njobs_gpu], &job_arr[j], sizeof(job_trace_t));
                njobs_gpu++;
            }
        }
    }

    compute_metrics(job_arr_all, njobs_all, Nnodes, &makespan, &util, &avg_wait, &std_wait, &min_wait, &max_wait, &nwait, &dispersion, &slowdown);
    printf("[ALL]\t Makespan=%ld\t Util=%.8f\t Avg_Wait=(%.8f,%.8f,%ld,%ld,%ld)\t Dispersion=%.8f Slowdown=%.8f\n", makespan, util, avg_wait, std_wait, nwait, min_wait, max_wait, dispersion, slowdown);

    compute_metrics(job_arr_mc, njobs_mc, Nnodes_mc, &makespan, &util, &avg_wait, &std_wait, &min_wait, &max_wait, &nwait, &dispersion, &slowdown);
    printf("[MC]\t Makespan=%ld\t Util=%.8f\t Avg_Wait=(%.8f,%.8f,%ld,%ld,%ld)\t Dispersion=%.8f Slowdown=%.8f\n", makespan, util, avg_wait, std_wait, nwait, min_wait, max_wait, dispersion, slowdown);

    compute_metrics(job_arr_gpu, njobs_gpu, Nnodes_gpu, &makespan, &util, &avg_wait, &std_wait, &min_wait, &max_wait,  &nwait, &dispersion, &slowdown);
    printf("[GPU]\t Makespan=%ld\t Util=%.8f\t Avg_Wait=(%.8f,%.8f,%ld,%ld,%ld)\t Dispersion=%.8f Slowdown=%.8f\n", makespan, util, avg_wait, std_wait, nwait, min_wait, max_wait, dispersion, slowdown);

    free(job_arr);
    free(job_arr_all);
    free(job_arr_mc);
    free(job_arr_gpu);

    return 0;
}

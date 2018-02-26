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

typedef enum field {
	submit = 0,
	eligible,
	start,
	end,
	duration
} fields;

fields f = submit;
char* field_name = NULL;
char* workload_trace_file = NULL;
char* replay_trace_file = NULL;
char help_msg[] = "\
list_trace [OPTIONS]\n\
    -w, --wrkldfile filename      The name of the trace file with the original data\n\
    -r, --replay    filename      The name of the trace file with the replayed data\n\
    -f, --field     field         Field to compare trace: duration, submit, eligible, start, end, nodes, state\n\
    -h, --help                    This help message.\n";

void getArgs(int argc, char** argv)
{
    static struct option long_options[] = {
        {"wrkldfile",      1, 0, 'w'},
        {"replay",         1, 0, 'r'},
        {"field",          1, 0, 'f'},
        {"help",           0, 0, 'h'},
    };
    int opt_char, option_index;

    while (1) {
        if ( (opt_char = getopt_long(argc, argv, "w:r:f:h", long_options,
                                     &option_index)) == -1 )
            break;
        switch ( opt_char) {
        case ('w'):
            workload_trace_file = strdup(optarg);
            break;
        case ('r'):
            replay_trace_file = strdup(optarg);
            break;
        case ('f'):
	    field_name = strdup(optarg);
	    if (strcmp("submit",optarg) == 0) {
		    f = submit;
	    }
	    if (strcmp("eligible",optarg) == 0) {
		    f = eligible;
	    }
	    if (strcmp("start",optarg) == 0) {
		    f = start;
	    }
	    if (strcmp("end",optarg) == 0) {
		    f = end;
	    }
	    if (strcmp("duration",optarg) == 0) {
		    f = duration;
	    }
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

int job_compare(const void *s1, const void *s2)
{
  job_trace_t *e1 = (job_trace_t*)s1;
  job_trace_t *e2 = (job_trace_t*)s2;
  return e1->id_job - e2->id_job;
}


int main(int argc, char *argv[])
{
    struct stat stat_buf;
    int work_file, replay_file;
    unsigned long long j, min_jobs, nwork_jobs, nreplay_jobs;
    double *dist;
    time_t t1, t2;
    job_trace_t *jwt; // job_work_trace
    job_trace_t *jrt; // job_replay_trace
    time_t jwt_min_start = LONG_MAX;//for makespan
    time_t jwt_max_end = 0; // for makespan
    time_t jrt_min_start = LONG_MAX;//for makespan
    time_t jrt_max_end = 0; // for makespan
    size_t query_length = 0;
    char query[1024];
    double avg_field,std_field;
    double cum_field, cum_stddev_field;

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
if (nwork_jobs != nreplay_jobs) {
    printf("Missmatch of number of jobs: (%llu != %llu). Exiting.\n",nwork_jobs,nreplay_jobs);
    exit(-1);
}
    dist = (double*)malloc(min_jobs*sizeof(double));
    jwt = (job_trace_t*)malloc(min_jobs*sizeof(job_trace_t));
    jrt = (job_trace_t*)malloc(min_jobs*sizeof(job_trace_t));
    read(work_file, jwt, min_jobs*sizeof(job_trace_t));
    read(replay_file, jrt, min_jobs*sizeof(job_trace_t));

    qsort(jwt, min_jobs, sizeof(job_trace_t), job_compare);
    qsort(jrt, min_jobs, sizeof(job_trace_t), job_compare);

    for(j = 0; j < min_jobs; j++) {

        if (jwt[j].id_job != jrt[j].id_job) {
            printf("Missmatch of job id (%d != %d), files are out of synch. Exiting.\n",jwt[j].id_job, jrt[j].id_job);
            exit(-1);
        }

        // makepsan
        if (jwt[j].time_start < jwt_min_start) {
            jwt_min_start = jwt[j].time_start;
        }
        if (jwt[j].time_end > jwt_max_end) {
            jwt_max_end = jwt[j].time_end;
        }
        if (jrt[j].time_start < jrt_min_start) {
            jrt_min_start = jrt[j].time_start;
        }
        if (jrt[j].time_end > jrt_max_end) {
            jrt_max_end = jrt[j].time_end;
        }

	if (f == duration) {
		t1 = jwt[j].time_end - jwt[j].time_start;
		t2 = jrt[j].time_end - jrt[j].time_start;
		dist[j] = euclidean_dist(t1,t2);
	}
	if (f == submit) {
        t1 = jwt[j].time_submit;
        t2 = jrt[j].time_submit;
        dist[j] = euclidean_dist(t1,t2);
        }
	if (f == eligible) {
        t1 = jwt[j].time_eligible;
        t2 = jrt[j].time_eligible;
        dist[j] = euclidean_dist(t1,t2);
        }
	if (f == start) {
        t1 = jwt[j].time_start;
        t2 = jrt[j].time_start;
        dist[j] = euclidean_dist(t1,t2);
        }
	if (f == end) {
        t1 = jwt[j].time_end;
        t2 = jrt[j].time_end;
        dist[j] = euclidean_dist(t1,t2);
        }
    }

    printf("Makespan of workload: %ld\n", jwt_max_end - jwt_min_start);
    printf("Makespan of replay: %ld\n", jrt_max_end - jrt_min_start);
    printf("Makespan differences: %ld\n", (jrt_max_end - jrt_min_start) -  (jwt_max_end - jwt_min_start));
    cum_field = 0;
    for(j = 0; j < min_jobs; j++) {
	    cum_field+=dist[j];
    }
    avg_field = cum_field/min_jobs;
    cum_stddev_field = 0;
    for(j = 0; j < min_jobs; j++) {
        cum_stddev_field += (dist[j] - avg_field)*(dist[j] - avg_field);
    }
    std_field = sqrt(cum_stddev_field/min_jobs);
    if (field_name == NULL) {
	    field_name = strdup("submit");
    }
    printf("%s : %.6f %.8f\n",field_name, avg_field, std_field);
    for(j = 0; j < min_jobs; j++) {
	    printf(" (%d,%.6f)", jwt[j].id_job, dist[j]);
    }
    printf("\n");

    free(dist);
    free(jwt);
    free(jrt);
    return 0;
}

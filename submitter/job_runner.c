#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <math.h>

#include "shmemclock.h"
#define ONE_OVER_BILLION 1E-9

char   help_msg[] = "\
job_runner\n\
      -d, --duration   Duration of the runner in seconds\n\
      -h, --help       This help message.\n\
      -j, --jobid      Job Id of the runner\n\
      -n, --nnodes     Number of nodes used by the runner\n\
      -r, --clock_rate Clock rate of the Replay-clock\n\
      -i, --init_time  Time at which the job has been submitted (from the submitter)\n\
      -p, --preset     If the job is a preset jobs or not\n\
      -e, --end_time   Time at which the jobs ends in the original trace\n\
      -x, --exitcode   Exit code returned by the runner\n";

int preset = 0;
int exitcode = 0;
long int duration = 0;
long int time_end = 0;
long int jobid = -1;
int nnodes = 0;
double init_time = 0.0;
double clock_rate = 0.0;

static void
get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"duration",  1, 0, 'd'},
        {"help",    0, 0, 'h'},
        {"jobid",  1, 0, 'j'},
        {"nnodes",    1, 0, 'n'},
        {"exitcode",    1, 0, 'x'},
        {"preset",    1, 0, 'p'},
        {"clock_rate",    1, 0, 'r'},
        {"init_time",    1, 0, 'i'},
        {"end_time",    1, 0, 'e'}
    };
    int opt_char, option_index;

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "d:hj:n:x:r:i:p:e:", long_options, &option_index)) == -1 )
            break;
        switch(opt_char) {
        case ('d'):
            duration = strtol(optarg,NULL,10);
            break;
        case ('e'):
            time_end = strtol(optarg,NULL,10);
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        case ('j'):
            jobid = strtol(optarg,NULL,10);
            break;
        case ('n'):
            nnodes = atoi(optarg);
            break;
        case ('x'):
            exitcode = atoi(optarg);
            break;
        case ('i'):
            init_time = strtod(optarg,NULL);
            break;
        case ('r'):
            clock_rate = strtod(optarg,NULL);
            break;
        case ('p'):
            preset = atoi(optarg);
            break;
        };
    }
}

int main(int argc, char *argv[])
{
    const int one_second = 1000000;
    int freq;
    time_t end_time, start_time, cur_time;
    char strstart_time[20];
    char strcur_time[20];
    char strend_time[20];
    double time_nsec;
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    time_nsec = tv.tv_sec + tv.tv_nsec * ONE_OVER_BILLION;

    open_rdonly_shmemclock();

    get_args(argc, argv);

    start_time = get_shmemclock();
    end_time = start_time+duration;

    strftime(strstart_time, sizeof(strstart_time), "%Y-%m-%d %H:%M:%S", localtime(&start_time));
    strftime(strend_time, sizeof(strend_time), "%Y-%m-%d %H:%M:%S", localtime(&end_time));
    printf("Job %ld: %s -- %s -- %ld [s] -- %d nodes, exit=%d, delta_time=%.6f, clock_rate=%f, preset=%d\n",jobid, strstart_time, strend_time, duration, nnodes, exitcode, time_nsec-init_time, clock_rate, preset);

    freq = one_second*clock_rate;
    while(get_shmemclock() < end_time) {
        usleep(freq);
    }

    cur_time = get_shmemclock();
    strftime(strcur_time, sizeof(strcur_time), "%Y-%m-%d %H:%M:%S", localtime(&cur_time));
    printf("Spinning over: %ld|%s\n", cur_time, strcur_time);

    // exit code are restricted in POSIX C
    if ( exitcode != 0 ) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

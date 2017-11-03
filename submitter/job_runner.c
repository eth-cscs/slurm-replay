#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>


#include "shmemclock.h"

char   help_msg[] = "\
job_runner\n\
      -d, --duration   Duration of the runner in seconds\n\
      -h, --help     This help message.\n\
      -j, --jobid      Job Id of the runner\n\
      -n, --ntasks     Number of tasks used by the runner\n\
      -x, --exitcode   Exit code returned by the runner\n";

int exitcode = 0;
long int duration = 0;
long int jobid = -1;
int ntasks = 0;

static void
get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"duration",  1, 0, 'd'},
        {"help",    0, 0, 'h'},
        {"jobid",  1, 0, 'j'},
        {"ntasks",    1, 0, 'n'},
        {"exitcode",    1, 0, 'x'}
    };
    int opt_char, option_index;

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "d:hj:n:x:", long_options, &option_index)) == -1 )
            break;
        switch(opt_char) {
        case ('d'):
            duration = strtol(optarg,NULL,10);
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        case ('j'):
            jobid = strtol(optarg,NULL,10);
            break;
        case ('n'):
            ntasks = atoi(optarg);
            break;
        case ('x'):
            exitcode = atoi(optarg);
            break;
        };
    }
}

int main(int argc, char *argv[])
{
    time_t cur_time, reg_time,start_time;

    open_rdonly_shmemclock();

    get_args(argc, argv);

    start_time = cur_time = get_shmemclock();
    reg_time = cur_time+duration;
    while(cur_time < reg_time) {
        usleep(500);
        cur_time = get_shmemclock();
    }
    printf("Job %ld: time=[%ld,%ld,%ld], %d tasks, exit=%d\n",jobid, start_time, duration, cur_time, ntasks, exitcode );


    exit(exitcode);
}

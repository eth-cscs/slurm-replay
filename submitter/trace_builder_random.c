#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include "trace.h"
#include <getopt.h>


#define DEFAULT_START_TIMESTAMP 1420066800

static const char DEFAULT_OFILE[]    = "simple.trace";
static const char DEFAULT_UFILE[]    = "users.sim";
static const int  MAX_CPUS           = 10;
static const int  MAX_TASKS          = 10;
static const int  MAX_TASKS_PER_NODE = 8;
enum { MAXLINES = 10000 };

static void print_usage()
{
    printf("\
Usage: trace_builder [OPTIONS]\n\
  -c <cpus_per_task>,             The number of CPU's per task\n\
  -d <duration>,                  The number of seconds that each job will run\n\
  -i <id_start>,                  The first jobid.  All subsequent jobid's will\n\
                  be one greater than the previous.\n\
  -l <number_of_jobs>,            The number of jobs to create in the trace file\n\
  -n <tasks_per_node>,            The maximum number of tasks that a job is to\n\
                  have per node.\n\
  -o <output_file>,               Specifies a different output file name.  \n\
                  Default is %s\n\
  -r,                             The random argument.  This specifies to use \n\
                  random values for duration, tasks_per_node, \n\
                  cpus_per_task and tasks.  This option takes\n\
                  no argument.\n\
  -s <submission_step>,           The amount of time, in seconds, between \n\
                  submission times of jobs.\n\
  -S <initial timestamp>,         The initial time stamp to use for the first\n\
                  job of the workload in lieu of the default.\n\
                  This must be in Unix epoch time format.\n\
  -t <tasks>,                     The number of tasks that a job is to have.\n\
  -u <user.sim_file>,             The name of the text file containing a list \n\
                  of users and their system id's.\n\n\
  NOTES: If not specified in conjunction with the -r (random) option, the\n\
  following options will have the following default values used in the\n\
  computation of the upper bound of the random number range:\n\t\
    duration = 1\n\t\
    tasks    = %d\n\t\
    tasks_per_node = %d\n\t\
    cpus_per_task = %d \n\
  If not specified but still using the random option, the upper bounds will\n\
  be based upon arbitrary default values.\n\
  ... The wall clock value will be set to always be greater than the duration.\n\
  ... If using the random option, there is no guarantee that the combination\n\
  of cpus_per_task and tasks_per_node will be valid for the system being\n\
  emulated.  Therefore, it is up to the user to use the -c and -n optons with\n\
  such values that the resultant product could never exceed the number of\n\
  of processors on the emulated nodes or the user will have to edit the\n\
  trace file after creating it.\
\n",
           DEFAULT_OFILE, MAX_TASKS, MAX_TASKS_PER_NODE, MAX_CPUS);
}


int
main(int argc, char **argv)
{

    char lines[MAXLINES][100];
    unsigned long timestamp = DEFAULT_START_TIMESTAMP;
    char *endptr;

    int  cpus_per_task  = 1;
    int  tasks_per_node = 1;
    int  tasks          = 1;
    int  random         = 0;
    int  length         = 100;
    int  id_start       = 1001;
    int  duration       = 10;
    char *ofile         = NULL;
    char *ufile         = NULL;
    int  step           = 0;
    int  i              = 0;
    int  flag           = 0;

    int  index, options;

    opterr   = 0;


    while ((options = getopt (argc, argv, "c:d:i:l:n:o:rs:S:t:u:")) != -1)
        switch (options) {
        case 'c':
            cpus_per_task = atoi(optarg);
            break;
        case 'd':
            duration = atoi(optarg);
            break;
        case 'i':
            id_start = atoi(optarg);
            break;
        case 'l':
            length = atoi(optarg);
            break;
        case 'n':
            tasks_per_node = atoi(optarg);
            break;
        case 'o':
            ofile = optarg;
            break;
        case 'r':
            random = 1;
            break;
        case 's':
            step = atoi(optarg);
            break;
        case 'S':
            timestamp = strtoul(optarg, &endptr, 10);
            if (!(optarg[0] != '\0' && *endptr == '\0')) {
                timestamp = DEFAULT_START_TIMESTAMP;
                printf ("Invalid timestamp!  Using "
                        "default: %lu\n", timestamp);
            }
            break;
        case 't':
            tasks = atoi(optarg);
            break;
        case 'u':
            ufile = optarg;
            break;
        default:
            print_usage();
            return 0;
        }

    if (!ofile) ofile = (char*)DEFAULT_OFILE;
    if (!ufile) ufile = (char*)DEFAULT_UFILE;


    for (index = optind; index < argc; index++) {
        printf ("Non-option argument %s\n", argv[index]);
        flag++;
    }

    if (flag) {
        print_usage();
        exit(-1);
    }

    /* reding users file */
    FILE *up = fopen(ufile, "r");
    if (up == 0) {
        fprintf(stderr, "failed to open %s\n", ufile);
        exit(1);
    } else  {
        while (i < MAXLINES && fgets(lines[i], sizeof(lines[0]), up)) {
            i = i + 1;
        }
    }

    fclose(up);

    int j=0,k=0;
    job_trace_t new_trace;
    int trace_file;
    int written;
    int t1,t2;
    char username[100];
    char account[100];

    /* open trace file: */
    if ((trace_file = open(ofile, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR |
                           S_IRGRP | S_IROTH)) < 0) {
        printf("Error opening trace file %s\n", ofile);
        return -1;
    }


    for (j=0; j<(length); j++ ) {
        new_trace.job_id = id_start;
        id_start++;
        new_trace.submit = timestamp + step;
        timestamp = timestamp + step;

        if (random==1) {
            new_trace.duration = rand() % duration;
        } else {
            new_trace.duration = duration;
        }

        new_trace.wclimit = new_trace.duration+2;

        k  = rand() % i;
        t1 = strcspn (lines[k],":");
        t2 = strlen(lines[k]);
        strncpy(username, lines[k], t1);
        username[t1] = '\0';
        strncpy(account, lines[k]+t1+1, t2-1);
        strtok(account,"\n");

        /* printf("username = %s - %d\n", username, strlen(username)); */
        /* printf("account = %s - %d\n", account, strlen(account));    */


        sprintf(new_trace.username, "%s", username);
        sprintf(new_trace.account, "%s", account);
        sprintf(new_trace.partition, "%s", "normal");

        if (random == 1) {
            /* If the default value exists, then random values will
             * be in the range of 1 through the maximum value for
             * that field + 1.  Otherwise use the user-supplied
             * value as the upper bound of the randome value.
             */
            if (tasks == 1)
                new_trace.tasks          =
                    (rand() % MAX_TASKS) + 1;
            else
                new_trace.tasks          =
                    (rand() % tasks) + 1;
            if (cpus_per_task == 1)
                new_trace.cpus_per_task  =
                    (rand() % MAX_CPUS) + 1;
            else
                new_trace.cpus_per_task  =
                    (rand() % cpus_per_task) + 1;
            if (tasks_per_node == 1)
                new_trace.tasks_per_node =
                    (rand() % MAX_TASKS_PER_NODE) + 1;
            else
                new_trace.tasks_per_node =
                    (rand() % tasks_per_node) + 1;
        } else {
            new_trace.cpus_per_task = cpus_per_task;
            new_trace.tasks_per_node = tasks_per_node;
            new_trace.tasks = tasks;
        }

        sprintf(new_trace.reservation, "%s", "");
        sprintf(new_trace.qosname, "%s", "");

        written = write(trace_file, &new_trace, sizeof(new_trace));
        if (written != sizeof(new_trace)) {
            printf("Error writing to file: %d of %ld\n",
                   written, sizeof(new_trace));
            exit(-1);
        }
    }

    /* printf("WRITTEN %d TRACES \n", j); */

    close(trace_file);


    printf ("\t"
            "%24s = %s\n\t"
            "%24s = %s\n\t"
            "%24s = %lu\n\t"
            "%24s = %d\n\t"
            "%24s = %d\n\t"
            "%24s = %d\n\t"
            "%24s = %d\n\t"
            "%24s = %d\n\t"
            "%24s = %d\n\t"
            "%24s = %d\n\t"
            "%24s = %d\n",
            "Users file",             ufile,
            "Output file",            ofile,
            "Initial timestamp",      timestamp,
            "Number of jobs",         length,
            "First jobid",            id_start,
            "Time step",              step,
            "Random",                 random,
            "Duration (Job runtime)", duration,
            "Tasks per job",          tasks,
            "Tasks per node",         tasks_per_node,
            "Cpus per task",          cpus_per_task);

    return 0;
}

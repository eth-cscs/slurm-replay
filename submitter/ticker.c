#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <slurm/slurm.h>

// from src/common/slurm_protocol_defs.h
int REQUEST_SUBMIT_BATCH_JOB=4003;

#include "shmemclock.h"

char   help_msg[] = "\
starter -t <time> -a <action>\n\
      -c, --clock    Start clock, parameters are endtime, rate and tick:\n\
                        endtime is the time at which the clock stop\n\
                        rate is the number of seconds between each clock ticks\n\
                        tick is the size in simulated seconds of a clock tick\n\
                        Example: -c 144,0.1,2 will increase the clock by 2 simulated seconds every 0.1 seconds until time 144 is reached\n\
      -g, --gettime  Get time of the clock\n\
      -s, --settime  Set time of the clock\n\
      -n, --njobs    Number of jobs that had to be completed\n\
      -o, --over     Indicate if the schedule is over\n\
      -h, --help     This help message.\n";


char *accel_filename = NULL;
int enable_set = 0;
int enable_get = 0;
int enable_clock = 0;
time_t time_evt = 0;
time_t endtime_evt = 0;
double rate = 0.1;
int tick = 1;
int enable_over = 0;
unsigned long njobs = 0;

static void
get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"clock",  1, 0, 'c'},
        {"gettime", 0, 0, 'g'},
        {"settime", 1, 0, 's'},
        {"njobs", 1, 0, 'n'},
        {"accel", 1, 0, 'a'},
        {"over", 0, 0, 'o'},
        {"help", 0, 0, 'h'}
    };
    int opt_char, option_index, i, k;
    char *clock_v;
    char endtime_v[128];
    char rate_v[32];
    char tick_v[32];

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "c:hgs:n:oa:", long_options, &option_index)) == -1 )
            break;
        switch(opt_char) {
        case ('c'):
            enable_clock = 1;
            clock_v = strdup(optarg);
            i = 0;
            while(clock_v[i] != ',' && clock_v[i] != '\0') {
                if (clock_v[i] != ' ') {
                    endtime_v[i] = clock_v[i];
                }
                i++;
            }
            endtime_v[i] = '\0';
            endtime_evt = strtoul(endtime_v,NULL,10);
            if (clock_v[i] == ',') {
                i++;
                k = 0;
                while(clock_v[i] != ',' && clock_v[i] != '\0') {
                    rate_v[k] = clock_v[i];
                    k++;
                    i++;
                }
                rate_v[k] = '\0';
                rate = strtod(rate_v,NULL);
            }
            if (clock_v[i] == ',') {
                i++;
                k = 0;
                while(clock_v[i] != '\0') {
                    tick_v[k] = clock_v[i];
                    k++;
                    i++;
                }
                tick_v[k] = '\0';
                tick = atoi(tick_v);
            }
            free(clock_v);
            break;
        case ('g'):
            enable_get = 1;
            break;
        case ('s'):
            enable_set = 1;
            time_evt = strtoul(optarg,NULL,10);
            break;
        case ('n'):
            njobs = strtoul(optarg,NULL,10);
            break;
        case ('o'):
            enable_over = 1;
            break;
        case ('a'):
            accel_filename = strdup(optarg);
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        };
    }
}

static inline int is_schedule()
{
    int i;
    unsigned long jobs_done = 0;
    stats_info_response_msg_t *stat_info;
    stats_info_request_msg_t stat_req;

    stat_req.command_id = STAT_COMMAND_GET;
    slurm_get_statistics(&stat_info, &stat_req);
    for(i = 0; i < stat_info->rpc_type_size; i++) {
        if (stat_info->rpc_type_id[i] == REQUEST_SUBMIT_BATCH_JOB)
            break;
    }
    // check if all jobs were submitted
    if (stat_info->rpc_type_cnt[i] < njobs) {
        return 1;
    } else {
        // all jobs are submitted, check if at least a job is running
        return stat_info->schedule_queue_len > 0;
    }
}

int main(int argc, char *argv[])
{
    const int one_second = 1000000;
    int freq, freq_slow, freq_fast;
    int accel_file;
    unsigned long naccel_time = 0;
    long *accel_times = NULL;
    char strstart_time[20];
    char strend_time[20];
    time_t tmp_time, amount_slow, amount_fast;
    long maxtick = 0;
    long k, j;

    get_args(argc, argv);

    open_rdwr_shmemclock();

    if (accel_filename != NULL) {
        accel_file = open(accel_filename, O_RDONLY);
        if (accel_file < 0) {
            printf("Error: opening file %s\n", accel_filename);
            return -1;
        }
        read(accel_file, &naccel_time, sizeof(unsigned long long));
        accel_times = (long*)malloc(naccel_time*sizeof(long));
        read(accel_file, accel_times, naccel_time*sizeof(long));
        close(accel_file);
    }

    if (enable_set) {
        strftime(strstart_time, sizeof(strstart_time), "%Y-%m-%d %H:%M:%S", localtime(&time_evt));
        set_shmemclock(time_evt);
        printf("Set time to %s\n", strstart_time);
    }

    if (enable_get) {
        tmp_time = get_shmemclock();
        strftime(strstart_time, sizeof(strstart_time), "%Y-%m-%d %H:%M:%S", localtime(&tmp_time));
        printf("%ld -- %s\n", tmp_time, strstart_time);
    }

    if (enable_clock) {
        tmp_time = get_shmemclock();
        strftime(strstart_time, sizeof(strstart_time), "%Y-%m-%d %H:%M:%S", localtime(&tmp_time));
        strftime(strend_time, sizeof(strend_time), "%Y-%m-%d %H:%M:%S", localtime(&endtime_evt));
        printf("Clock: njobs=%lu start='%s|%ld', end='%s|%ld', duration=%ld[s], rate=(%.5f|%.5f)[s] for 1 replayed second\n", njobs, strstart_time, tmp_time, strend_time, endtime_evt, endtime_evt-tmp_time, rate/tick, (rate/2.0)/tick);
        fflush(stdin);
        freq_slow = one_second*rate;
        freq_fast = one_second*(rate/2.0);
        freq = freq_fast;
        if (accel_filename != NULL) {
            amount_slow = amount_fast = 0;
            for(j = 0; j < naccel_time; j++) {
                maxtick = accel_times[j] - tmp_time;
                k = 0;
                while(k < maxtick) {
                    usleep(freq);
                    incr_shmemclock(tick);
                    k++;
                }
                tmp_time = get_shmemclock();
                if (freq == freq_slow) {
                   amount_slow += k;
                   freq = freq_fast;
                } else {
                   amount_fast += k;
                   freq = freq_slow;
                }
            }
            freq = freq_slow;
        }
        maxtick=endtime_evt-tmp_time;
        k = 0;
        while(k < maxtick) {
            usleep(freq);
            incr_shmemclock(tick);
            k++;
        }

        /*tmp_time = get_shmemclock();
        if (tmp_time < endtime_evt) {
            strftime(strstart_time, sizeof(strstart_time), "%Y-%m-%d %H:%M:%S", localtime(&tmp_time));
            printf("Exausted ticks - time not reached %ld -- %s\n", tmp_time, strstart_time);
            while(get_shmemclock() < endtime_evt) {
                usleep(freq);
                incr_shmemclock(tick);
            }
        }*/

        if (is_schedule()) {
            tmp_time = get_shmemclock();
            strftime(strstart_time, sizeof(strstart_time), "%Y-%m-%d %H:%M:%S", localtime(&tmp_time));
            printf("Schedule not finished - time %ld -- %s\n", tmp_time, strstart_time);
            while(is_schedule()) {
                usleep(freq);
                incr_shmemclock(tick);
                k++;
            }
        }
        if (accel_filename != NULL) {
            amount_slow+=k;
            printf("Ticks breakdown - fast: %ld - slow: %ld\n", amount_fast, amount_slow);
        }
    }

    if (enable_over) {
        tmp_time = get_shmemclock();
        strftime(strstart_time, sizeof(strstart_time), "%Y-%m-%d %H:%M:%S", localtime(&tmp_time));
        if (is_schedule()) {
            printf("%ld -- %s  Schedule is still active\n", tmp_time, strstart_time);
        } else {
            printf("%ld -- %s  Schedule is over\n", tmp_time, strstart_time);
        }
    }

    exit(0);
}

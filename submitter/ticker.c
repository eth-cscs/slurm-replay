#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>


#include "shmemclock.h"

char   help_msg[] = "\
starter -t <time> -a <action>\n\
      -c, --clock    Start clock, parameters are endtime, rate and tick:\n\
                        endtime is the time at which the clock stop\n\
                        rate is the number of seconds between each clock ticks\n\
                        tick is the size in simulated seconds of a clock tick\n\
                        Example: -c 144,0.1,2 will increase the clock by 2 simulated seconds every 0.1 seconds until time 144 is reached\n\
      -g, --gettime     Get time of the clock\n\
      -s, --settime     Set time of the clock\n\
      -h, --help     This help message.\n";


int enable_set = 0;
int enable_get = 0;
int enable_clock = 0;
time_t time_evt = 0;
time_t endtime_evt = 0;
double rate = 0.1;
int tick = 1;

static void
get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"clock",  1, 0, 'c'},
        {"gettime", 0, 0, 'g'},
        {"settime", 1, 0, 's'},
        {"help", 0, 0, 'h'}
    };
    int opt_char, option_index, i, k;
    char *clock_v;
    char endtime_v[128];
    char rate_v[32];
    char tick_v[32];

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "c:hgs:", long_options, &option_index)) == -1 )
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
            printf("%ld, %f, %d\n",endtime_evt, rate, tick);
            break;
        case ('g'):
            enable_get = 1;
            break;
        case ('s'):
            enable_set = 1;
            time_evt = strtoul(optarg,NULL,10);
            break;
        case ('h'):
            printf("%s\n", help_msg);
            exit(0);
        };
    }
}

int main(int argc, char *argv[])
{
    const int one_second = 1000000;

    get_args(argc, argv);

    open_rdwr_shmemclock();

    if (enable_set) {
        set_shmemclock(time_evt);
        printf("Set time to %ld\n", time_evt);
        printf("Get time to %ld\n", get_shmemclock());
    }

    if (enable_get) {
        printf("get: %ld\n",get_shmemclock());
    }

    if (enable_clock) {
        while(get_shmemclock() < endtime_evt) {
            usleep(one_second*rate);
            incr_shmemclock(tick);
        }

    }

    exit(0);
}

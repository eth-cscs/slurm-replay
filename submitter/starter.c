#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>


#include "shmem_pq.h"

char   help_msg[] = "\
starter -t <time> -a <action>\n\
      -a, --action   Action could be either:\n\
                        'A' for adding the time in the clock\n\
                        'C' use it at a clock mode\n\
                        'R' for removing a specific time when it is the top of the queue\n\
                        'Z' for adding and waiting to remove a specific time when it is the top of the queue\n\
                        'D' for adding a constante to current time and waiting to remove a that time when it is the top of the queue\n\
                        'r' for removing all times up to the value of -t\n\
                        'T' for reading top 10 values\n\
                        'H' history of event on the clock, positive number means push negative numbers mean pop\n\
      -t, --time     Time at which the clock is set\n\
      -s, --header   String to display\n\
      -h, --help     This help message.\n";

char *action = NULL;
char *header = NULL;
shmem_pq_elt_type time;

static void
get_args(int argc, char** argv)
{
    static struct option long_options[]  = {
        {"action",  1, 0, 'a'},
        {"time",    1, 0, 't'},
        {"header",  1, 0, 's'},
        {"help",    0, 0, 'h'}
    };
    int opt_char, option_index;

    while (1) {
        if ((opt_char = getopt_long(argc, argv, "a:ht:s:", long_options, &option_index)) == -1 )
            break;
        switch(opt_char) {
        case ('a'):
            action = strdup(optarg);
            break;
        case ('t'):
            time = strtoul(optarg,NULL,10);
            break;
        case ('s'):
            header = strdup(optarg);
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
    int k = 0;
    int n = 0;
    int s = 0;
    shmem_pq_elt_type cur_time, reg_time;
    shmem_pq_elt_type rng_time[10];
    shmem_pq_elt_type hist[1024];

    open_pq();

    get_args(argc, argv);

    if (action == NULL) {
        printf("Error: action is required\n");
    }

    if (*action == 'A' || *action == 'Z') {
        lock_pq();
        push_pq(time);
        unlock_pq();
    }

    if (*action == 'C') {
            //lock_pq();
            //push_pq(time);
            //unlock_pq();
            while(!empty_pq() && cur_time < time) {
                usleep(one_second/2);
                cur_time = top_pq();
                lock_pq();
                push_pq(cur_time+1);
                pop_pq();
                unlock_pq();
            }

    }
    if (*action == 'D') {
        if(!empty_pq()) {
            cur_time = top_pq();
            reg_time = cur_time+time;
            //lock_pq();
            //push_pq(reg_time);
            //unlock_pq();
            //cur_time = top_pq();
            while(cur_time != reg_time) {
                usleep(500);
                cur_time = top_pq();
            }
            //lock_pq();
            //pop_pq();
            //unlock_pq();
        }
    }

    if (*action == 'r') {
        if(!empty_pq()) {
            lock_pq();
            cur_time = top_pq();
            while(cur_time < time) {
                pop_pq();
                cur_time = top_pq();
            }
            unlock_pq();
        }
    }

    if (*action == 'R' || *action == 'Z') {
        if(!empty_pq()) {
            cur_time = top_pq();
            while(cur_time != time) {
                usleep(500);
                cur_time = top_pq();
            }
            lock_pq();
            pop_pq();
            unlock_pq();
        }
    }

    if (*action == 'T') {
        n = 10;
        printf("top_pq %d:\n",n);
        s = range_pq(n, rng_time);
        for(k = 0; k < s; k++) {
            printf("%ld\n",rng_time[k]);
        }
    }

    if (*action == 'H') {
        printf("history:\n");
        s = getHistory(100,hist);
        for(k = 0; k < s; k++) {
            if (hist[k] > 0)
                printf("+++ %ld\n",hist[k]);
            else
                printf("--- %ld\n",-hist[k]);
        }
    }


    if (header) {
        printf("%s\n",header);
        free(header);
    }

    if (empty_pq()) {
        printf("Queue is empty!\n");
    }

    exit(0);
}

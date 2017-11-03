#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "shmemclock.h"

unsigned int sleep(unsigned int seconds)
{
    time_t cur_time = get_shmemclock();
    while(get_shmemclock() < cur_time+seconds);
    return 0;
}


time_t time(time_t *tloc)
{
    time_t t = get_shmemclock();
    if (tloc)
        *tloc = t;
    return t;
}


int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    tv->tv_sec = get_shmemclock();
    tv->tv_usec = 0;
}


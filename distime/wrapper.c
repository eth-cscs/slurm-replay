#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include "shmemclock.h"
static int init = 0;

unsigned int sleep(unsigned int seconds)
{
    time_t cur_time;
    if (init == 0) {
	    open_rdonly_shmemclock();
	    init = 1;
    }
    cur_time = get_shmemclock();
    while(get_shmemclock() < cur_time+seconds);
    return 0;
}


time_t time(time_t *tloc)
{
    time_t t;
    if (init == 0) {
	    open_rdonly_shmemclock();
	    init = 1;
    }
    t = get_shmemclock();
    if (tloc)
        *tloc = t;
    return t;
}


int gettimeofday(struct timeval *tv, struct timezone *tz)
{
    tv->tv_sec = get_shmemclock();
    tv->tv_usec = 0;
}

int setuid(uid_t uid) {
    return 0;
}

int setgid(gid_t gid) {
    return 0;
}

int seteuid(uid_t uid) {
    return 0;
}

int setegid(gid_t gid) {
    return 0;
}

int chown(const char *pathname, uid_t owner, gid_t group) {
    return 0;
}

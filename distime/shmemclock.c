#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <fcntl.h>
#include <time.h>
#include <err.h>
#include <string.h>

#include "shmemclock.h"

void *clock_ptr;

static inline void _open_shmemclock(int flag1, int flag2)
{
    int fd = -1;
    const char* shmem_name = getenv("DISTIME_SHMEMCLOCK_NAME");
    if (shmem_name == NULL || strlen(shmem_name) == 0) {
       fd = shm_open("/ShmemClock", O_CREAT | flag1, S_IRUSR | S_IWUSR );
    } else {
       fd = shm_open(shmem_name, O_CREAT | flag1, S_IRUSR | S_IWUSR );
    }

    if ( fd < 0 ) {
        err(1,"Error opening shared memory");
    }

    ftruncate(fd, sizeof(time_t));

    clock_ptr = mmap(0, sizeof(time_t), flag2, MAP_SHARED, fd, 0);
    if (clock_ptr == MAP_FAILED) {
        err(1, "Error mmaping shared memory");
    }

    clock_val = (time_t*)clock_ptr;
}

void open_rdonly_shmemclock() {
_open_shmemclock(O_RDONLY, PROT_READ);
}

void open_rdwr_shmemclock() {
_open_shmemclock(O_RDWR, PROT_READ | PROT_WRITE);
}


extern time_t get_shmemclock();
extern void set_shmemclock(time_t t);
extern time_t incr_shmemclock(int v);

#ifndef SHMEM_PQ_H
#define SHMEM_PQ_H

#include <time.h>

void open_rdonly_shmemclock();
void open_rdwr_shmemclock();
time_t get_shmemclock();
void set_shmemclock(time_t t);
time_t incr_shmemclock(int v);

#endif

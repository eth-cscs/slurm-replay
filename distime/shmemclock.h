#ifndef SHMEM_PQ_H
#define SHMEM_PQ_H

#include <time.h>

time_t *clock_val;

void open_rdonly_shmemclock();
void open_rdwr_shmemclock();

static inline time_t get_shmemclock()
{
    return __atomic_load_n(clock_val, __ATOMIC_SEQ_CST);
}

static inline void set_shmemclock(time_t t)
{
    __atomic_store_n(clock_val, t, __ATOMIC_SEQ_CST);
}

static inline time_t incr_shmemclock(int v) {
    return __atomic_add_fetch(clock_val, v, __ATOMIC_SEQ_CST);
}

#endif

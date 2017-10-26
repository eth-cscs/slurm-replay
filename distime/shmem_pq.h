#ifndef SHMEM_PQ_H
#define SHMEM_PQ_H


#ifdef __cplusplus
extern "C" {
#endif

typedef long int shmem_pq_elt_type;

void open_pq();
void close_pq();
shmem_pq_elt_type top_pq();
void push_pq(shmem_pq_elt_type elt);
void pop_pq();
int empty_pq();
int range_pq(int n, shmem_pq_elt_type* v);
void lock_pq();
void unlock_pq();
int getHistory(int n, shmem_pq_elt_type* v);

#ifdef __cplusplus
}
#endif

#endif

#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <algorithm>

#include "shmem_pq.h"

using namespace boost::interprocess;

typedef allocator<shmem_pq_elt_type, managed_shared_memory::segment_manager> ShmemAllocator_t;
typedef vector<shmem_pq_elt_type, ShmemAllocator_t> ShmemVector_t;


// History
static ShmemVector_t *shmem_hist;
static managed_shared_memory segment_hist;

// Container holding the times
static ShmemVector_t *shmem_vec;
static managed_shared_memory segment;

void open_pq()
{
    //Create or open a segment with given name and size
    segment = managed_shared_memory(open_or_create, "ShmemSegmentPriorityQueueName", 65536);

    //Get the object if not found then instanciate it
    shmem_vec = segment.find<ShmemVector_t>("ShmemObjectPriorityQueueName").first;
    if (shmem_vec == 0) {
        //Initialize shared memory STL-compatible allocator
        const ShmemAllocator_t alloc_inst (segment.get_segment_manager());
        shmem_vec = segment.construct<ShmemVector_t>("ShmemObjectPriorityQueueName")(alloc_inst);
    }

    //Do the same for history

    //Create or open a segment with given name and size
    segment_hist = managed_shared_memory(open_or_create, "ShmemSegmentVectorHistoryName", 65536);

    //Get the object if not found then instanciate it
    shmem_hist = segment_hist.find<ShmemVector_t>("ShmemObjectVectorHistoryName").first;
    if (shmem_hist == 0) {
        //Initialize shared memory STL-compatible allocator
        const ShmemAllocator_t alloc_inst (segment_hist.get_segment_manager());
        shmem_hist = segment_hist.construct<ShmemVector_t>("ShmemObjectVectorHistoryName")(alloc_inst);
    }

}

void close_pq()
{
    shared_memory_object::remove("ShmemSegmentPriorityQueueName");
    shared_memory_object::remove("ShmemSegmentVectorHistoryName");
    shared_memory_object::remove("ShmemMutexPriorityQueueName");
}

shmem_pq_elt_type top_pq()
{
    return shmem_vec->front();
}

void push_pq(shmem_pq_elt_type elt)
{
    if (!shmem_vec->empty() && elt < shmem_vec->front())
    {
        printf("SHMEM_PQ error pushing a time before current time\n");

    }
    shmem_vec->push_back(elt);
    std::push_heap(shmem_vec->begin(), shmem_vec->end(), std::greater<shmem_pq_elt_type>());
    shmem_hist->push_back(elt);
}

void pop_pq()
{
    shmem_hist->push_back(-top_pq());
    std::pop_heap(shmem_vec->begin(), shmem_vec->end(), std::greater<shmem_pq_elt_type>());
    shmem_vec->pop_back();
}

int getHistory(int n, shmem_pq_elt_type* v)
{
    int s = shmem_hist->size();
    if (s > n) s = n;
    for(int k = 0; k < s; k++)
        v[k] = shmem_hist->at(k);
    return s;
}

int empty_pq()
{
    return shmem_vec->empty();
}

int range_pq(int n, shmem_pq_elt_type* v)
{
    int s = shmem_vec->size();
    if (s > n) s = n;
    for(int k = 0; k < s; k++)
        v[k] = shmem_vec->at(k);
    return s;
}

void lock_pq()
{
    named_mutex mutex_w(open_or_create, "ShmemMutexPriorityQueueName");
    mutex_w.lock();
}

void unlock_pq()
{
    named_mutex mutex_w(open_or_create, "ShmemMutexPriorityQueueName");
    mutex_w.unlock();
}

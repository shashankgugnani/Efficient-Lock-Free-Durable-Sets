#ifndef COMMON_H_
#define COMMON_H_

#include <atomic>
#include <stddef.h> //for null
#include <climits>  //for max int
#include <fstream>
#include <libpmem.h>

#define compiler_fence std::atomic_thread_fence(std::memory_order_release)
#define MFENCE __sync_synchronize
#define MAX_LEVEL (21) //one cache-line node; use 13 for two cache-line nodes

#define UNLIKELY(x) __builtin_expect((x), 0)
#define LIKELY(x) __builtin_expect((x), 1)

typedef unsigned char uchar;

static inline void SFENCE()
{
    asm volatile("sfence" ::
                     : "memory");
}

static inline void FLUSH(void *p, size_t size)
{
    pmem_persist(p, size);
}

static inline void BARRIER(void *p)
{
    SFENCE();
}

#endif

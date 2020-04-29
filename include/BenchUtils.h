#ifndef _BENCH_UTILS_
#define _BENCH_UTILS_

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <atomic>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <pthread.h>
#include <assert.h>

#include "rand_r_32.h"
#include "ssmem.h"
#include "barrier.h"
#include "common.h"
#include "timer.h"
using namespace std;

std::ofstream file;
__thread ssmem_allocator_t *alloc;

static int NUM_THREADS = 1;
static uint32_t DURATION = 5;
static uint32_t RO_RATIO = 90;
static uint32_t KEY_RANGE = 1024;
static uint32_t ITERATION = 1;
static string ALG_NAME = "BucketList";
static bool SanityMode = false;
static int TEST_NUM = 1;
barrier_t barrier_global;
barrier_t init_barrier;

static std::atomic<bool> bench_stop;

static inline void set_cpu(int cpu)
{
    assert(cpu > -1);
    int n_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu < n_cpus)
    {
        int cpu_use = cpu;
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu_use, &mask);
        pthread_t thread = pthread_self();
        if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &mask) != 0)
        {
            fprintf(stderr, "Error setting thread affinity\n");
        }
    }
}

static void printHelp()
{
    cout << "  -a     algorithm" << endl;
    cout << "  -p     thread num" << endl;
    cout << "  -d     duration" << endl;
    cout << "  -R     lookup ratio (0~100)" << endl;
    cout << "  -M     key range" << endl;
    cout << "  -I     iteration number" << endl;
    cout << "  -t     test number" << endl;
}

static bool parseArgs(int argc, char **argv)
{
    int c;
    while ((c = getopt(argc, argv, "a:p:d:R:M:I:t:hc")) != -1)
    {
        switch (c)
        {
        case 'a':
            ALG_NAME = string(optarg);
            break;
        case 'p':
            NUM_THREADS = atoi(optarg);
            break;
        case 'd':
            DURATION = atoi(optarg);
            break;
        case 'R':
            RO_RATIO = atoi(optarg);
            break;
        case 'M':
            KEY_RANGE = atoi(optarg);
            break;
        case 'I':
            ITERATION = atoi(optarg);
            break;
        case 't':
            TEST_NUM = atoi(optarg);
            break;
        case 'h':
            printHelp();
            return false;
        default:
            return false;
        }
    }
    return true;
}


template<class SET>
void specificInit(int id);

//Throughput Measurements

struct bench_ops_thread_arg_t
{
    uintptr_t tid;
    void *set;
    uint64_t ops;
    uint64_t num_insert;
    uint64_t num_remove;
    uint64_t time_search;
    uint64_t time_insert;
    uint64_t time_remove;
};

template <class SET>
void benchOpsThread(bench_ops_thread_arg_t *arg)
{
    int cRatio = 1000 * RO_RATIO / 100;
    int iRatio = cRatio + (1000 - cRatio) / 2;
    int id = arg->tid;
    set_cpu(id - 1);
    uint32_t seed1 = id;
    uint32_t seed2 = seed1 + 1;
    specificInit<SET>(id);

    TIMER_HP_REGISTER();

    alloc = (ssmem_allocator_t *)malloc(sizeof(ssmem_allocator_t));
    ssmem_alloc_init(alloc, SSMEM_DEFAULT_MEM_SIZE, true, id);

    barrier_cross(&init_barrier);

    uint32_t num_elems_thread = (uint32_t)((KEY_RANGE / 2) / NUM_THREADS);
    uint32_t missing = (uint32_t)(KEY_RANGE / 2) % NUM_THREADS;
    if (id <= missing)
    {
        num_elems_thread++;
    }

    uint64_t ops = 0;
    SET *set = (SET *)arg->set;

    for (int i = 0; i < (int)num_elems_thread; i++)
    {
        int key = rand_r_32(&seed2) % KEY_RANGE;
        if (!set->insert(key, id, id))
        {
            i--;
        }
    }

    barrier_cross(&barrier_global);

    while (!bench_stop)
    {
        int op = rand_r_32(&seed1) % 1000;
        int key = rand_r_32(&seed2) % KEY_RANGE;
        TIMER_HP_START();
        if (op < cRatio)
        {
            set->contains(key, id);
            if (CONFIG_TIMER) arg->time_search += TIMER_HP_ELAPSED();
        }
        else if (op < iRatio)
        {
            set->insert(key, id, id);
            if (CONFIG_TIMER) arg->time_insert += TIMER_HP_ELAPSED();
            arg->num_insert++;
        }
        else
        {
            set->remove(key, id);
            if (CONFIG_TIMER) arg->time_remove += TIMER_HP_ELAPSED();
            arg->num_remove++;
        }
        ops++;
    }
    arg->ops = ops;
}

template <class SET>
static void runBench()
{
    SET *set = new SET();
    if (ITERATION == 1)
    {
        cout << "Running " << ALG_NAME << ": Reads " << RO_RATIO << " Key Range " << KEY_RANGE;
        cout << " Num Threads " << NUM_THREADS << endl;
    }

    barrier_init(&barrier_global, NUM_THREADS + 1);
    barrier_init(&init_barrier, NUM_THREADS);

    struct timespec timeout;
    struct timeval start, end;
    timeout.tv_sec = DURATION;
    timeout.tv_nsec = 0;

    alloc = (ssmem_allocator_t *)malloc(sizeof(ssmem_allocator_t));
    ssmem_alloc_init(alloc, SSMEM_DEFAULT_MEM_SIZE, true, 0);

    bench_stop = false;

    thread *thrs[NUM_THREADS];
    bench_ops_thread_arg_t args[NUM_THREADS];

    for (uint32_t j = 1; j < NUM_THREADS + 1; j++)
    {
        bench_ops_thread_arg_t &arg = args[j - 1];
        arg.tid = j;
        arg.set = set;
        arg.ops = 0;
        arg.num_insert = 0;
        arg.num_remove = 0;
        arg.time_search = 0;
        arg.time_insert = 0;
        arg.time_remove = 0;
        thrs[j - 1] = new thread(benchOpsThread<SET>, &arg);
    }

    // broadcast begin signal
    barrier_cross(&barrier_global);

    gettimeofday(&start, NULL);
    nanosleep(&timeout, NULL);

    bench_stop = true;
    gettimeofday(&end, NULL);

    for (uint32_t j = 0; j < NUM_THREADS; j++)
        thrs[j]->join();

    //compute the exact duration of the experiment
    int duration = (end.tv_sec * 1000 + end.tv_usec / 1000) - (start.tv_sec * 1000 +
                   start.tv_usec / 1000);

    delete set;

    uint64_t totalOps = 0;
    uint64_t searches = 0;
    uint64_t inserts = 0;
    uint64_t removes = 0;
    uint64_t time_searches = 0;
    uint64_t time_inserts = 0;
    uint64_t time_removes = 0;

    for (uint32_t j = 0; j < NUM_THREADS; j++)
    {
        totalOps += args[j].ops;
        inserts += args[j].num_insert;
        removes += args[j].num_remove;
        time_searches += args[j].time_search;
        time_inserts += args[j].time_insert;
        time_removes += args[j].time_remove;
    }
    searches = totalOps - (inserts + removes);

    file << totalOps / (duration * 1.0) << endl;
    cout << "Search Lat (cycles): " << time_searches / (searches * 1.0) << endl;
    cout << "Insert Lat (cycles): " << time_inserts / (inserts * 1.0) << endl;
    cout << "Remove Lat (cycles): " << time_removes / (removes * 1.0) << endl;
    cout << "Throughput (Kops/s): " << totalOps / (duration * 1.0) << endl;
}

#endif

/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2018-2019, Shashank Gugnani.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LL_TIMER_H
#define LL_TIMER_H

#include "stdinc.h"

#ifndef CONFIG_TIMER
#define CONFIG_TIMER 1
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Timer stuff
 * Usage:
 * TIMER_START("xyz")
 * xyz()
 * TIMER_END("xyz")
 * Info:
 * Timer will print time spent in the xyz routine as follows:
 * xyz took a.bc us
 * Nested timers are supported as well;
 * Define MAX_TIMERS to change the maximum number
 * of concurrent timers
 */
#define MAX_TIMERS 10
static int timers = 0;
static double start_time[MAX_TIMERS];
static double end_time[MAX_TIMERS];

__attribute__((used))
static void
TLOG(FILE *stream, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stream, format, args);
    va_end(args);
}

__attribute__((used))
static void
print_current_time_as_ms(void)
{
    long ms; /* milliseconds */
    time_t s; /* seconds */
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);

    s = spec.tv_sec;
    ms = spec.tv_nsec / 1.0e6; /* convert nanoseconds to milliseconds */
    if (ms > 999) {
        ++s;
        ms = 0;
    }

    TLOG(stderr,
         "Current time: %" PRIdMAX ".%03ld seconds since the epoch\n",
         (intmax_t)s, ms);
}

/**
 * Get microsecond precision timestamp.
 */
static inline double
get_timestamp(void)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL)) {
        perror("gettimeofday");
        return 0;
    }
    return (double)tv.tv_sec * 1000000 + (double)tv.tv_usec;
}

/**
 * Read cpu timestamp counter.
 */
static inline u64
rdtsc(void)
{
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((u64)hi << 32) | lo;
}

#define TIMER_START(...)                                    \
    do {                                                    \
        if (CONFIG_TIMER) {                                 \
	    start_time[timers++] = get_timestamp();         \
	}                                                   \
    } while (0)

#define TIMER_END(...)                                      \
    do {                                                    \
        if (CONFIG_TIMER) {                                 \
	    end_time[--timers] = get_timestamp();           \
	    TLOG(stderr, __VA_ARGS__);                      \
	    TLOG(stderr, " took %f us\n",                   \
		 end_time[timers] - start_time[timers]);    \
	} \
    } while (0)

static inline double
TIMER_ELAPSED(void)
{
#if CONFIG_TIMER
    end_time[--timers] = get_timestamp();
    return end_time[timers] - start_time[timers];
#else
    return 0;
#endif
}

/**
 * Thread safe timers
 * Usage:
 * TIMER_MT_REGISTER() [once per thread]
 * TIMER_MT_START("xyz")
 * xyz()
 * TIMER_MT_END("xyz")
 */
#define TIMER_MT_REGISTER()                                 \
    double start_mt_time, end_mt_time;                      \
    (void)(end_mt_time);

#define TIMER_MT_START(...)                                 \
    do {                                                    \
        if (CONFIG_TIMER) {                                 \
	    start_mt_time = get_timestamp();                \
	}                                                   \
    } while (0)

#define TIMER_MT_END(...)                                   \
    do {                                                    \
        if (CONFIG_TIMER) {                                 \
	    end_mt_time = get_timestamp();                  \
	    TLOG(stderr, __VA_ARGS__);                      \
	    TLOG(stderr, " took %f us\n",                   \
		 end_mt_time - start_mt_time);              \
	}                                                   \
    } while (0)

#define TIMER_MT_ELAPSED(...)                               \
    (get_timestamp() - start_mt_time)

/**
 * Thread safe high precision timers
 * Usage:
 * TIMER_HP_REGISTER() [once per thread]
 * TIMER_HP_START("xyz")
 * xyz()
 * TIMER_HP_END("xyz")
 */
#define TIMER_HP_REGISTER()                                 \
    u64 start_hp_time, end_hp_time;                         \
    (void)(end_hp_time);

#define TIMER_HP_START(...)                                 \
    do {                                                    \
        if (CONFIG_TIMER) {                                 \
            start_hp_time = rdtsc();                        \
        }                                                   \
    } while (0)

#define TIMER_HP_END(...)                                   \
    do {                                                    \
        if (CONFIG_TIMER) {                                 \
            end_hp_time = rdtsc();                          \
            TLOG(stderr, "took %" PRIu64 "cycles\n",        \
                 end_hp_time - start_hp_time);              \
        }                                                   \
    } while (0)

#define TIMER_HP_ELAPSED(...)                               \
    (rdtsc() - start_hp_time)

#ifdef __cplusplus
}
#endif

#endif /* LL_TIMER_H */

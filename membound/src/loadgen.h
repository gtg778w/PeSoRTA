#ifndef LOADGEN_HEADER
#define LOADGEN_HEADER

#include <time.h>
#include <stdint.h>

/*
This file needs to be linked with -lrt
*/

static double ipms = 1000000.0;

static int32_t work_function(long count, int32_t workfunc_state)
{
	unsigned long i;
    /*These variables need to be volatile to keep them from
    being optimized out*/
	int32_t volatile a=134775813, c=1, x=workfunc_state;

	for(i = 0; i < count; i++)
	{
		x = a*x+c;
	}

	return x;
}

#define timed_work_function(ms)\
{\
    long iterations = (long)(ipms * ms);\
    work_function(iterations);\
}

static long callibrate(long count)
{
    int ret;
    struct timespec start_ts, stop_ts;
    double start, stop, interval;
    int32_t x;

    if(count == 0) count = 100000000;

    ret = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start_ts);
    if(ret == -1)
    {
        ipms = -1.0;
        return -1;
    }

    x = work_function(count, 0);

    ret = clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &stop_ts);
    if(ret == -1)
    {
        ipms = -1.0;
        return -1;
    }

    start = ((start_ts.tv_sec * 1000000000.0) + 
            (start_ts.tv_nsec));

    stop = ((stop_ts.tv_sec * 1000000000.0) + 
            (stop_ts.tv_nsec));
    interval = stop - start;
    ipms = (((double)count)/interval)*1000000.0;
    return x;
}

#endif

#include <stdlib.h>
#include <stdint.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sched.h>

void*   mapped_region;
size_t  mapped_region_size;
int32_t *cache_line_array;

static __inline__ uint64_t getns(void)
{
    struct timespec time;
    int ret;
    uint64_t now_ns;

	ret = clock_gettime(CLOCK_MONOTONIC, &time);
    if(ret == -1)
    {
        perror("clock_gettime failed");
        exit(EXIT_FAILURE);
    }
    

    now_ns = time.tv_sec * 1000000000;
    now_ns = now_ns + time.tv_nsec;

    return now_ns;
}

int32_t mainloop(int32_t doubleword_per_cacheline, int32_t cacheoffset, int64_t iterations)
{
    int32_t m;
    
    m = 1;
    do
    {
        m = cache_line_array[(doubleword_per_cacheline*m) + cacheoffset];
    }while(iterations--);
    
    return m;
}

int realtime_init(double desired_priority)
{
    int ret = 0;
    
    /*Variables for the scheduler.*/
    pid_t my_pid;
    double max_priority;
    double min_priority;
    int set_priority;
    struct sched_param sched_param;

    my_pid = getpid();
        
    max_priority = (double)sched_get_priority_max(SCHED_FIFO);
    if(max_priority == -1.0)        
    {
        perror("realtime_init: sched_get_priority_max failed");
        ret = -1;
        goto exit0;
    }

    min_priority = (double)sched_get_priority_min(SCHED_FIFO);
    if(min_priority == -1.0)        
    {
        perror("realtime_init: sched_get_priority_min failed");
        ret = -1;
        goto exit0;
    }

    set_priority = (int)(   (desired_priority * max_priority)  +
                            ((1.0 - desired_priority) * min_priority));

    sched_param.sched_priority = set_priority;
    ret = sched_setscheduler(my_pid, SCHED_FIFO, &sched_param);
    if(ret == -1)
    {
        perror("realtime_init: sched_setsceduler failed");
        goto exit0;
    }

    ret = mlockall(MCL_CURRENT);
    if(ret == -1)
    {
        perror("realtime_init: mlock failed");
        goto exit0;
    }
    
exit0:
    return ret;
}

int32_t get_doubleword_per_cacheline(void)
{
    int32_t ret = 0;
    long int cacheline_size;
    FILE* filep;
    
    filep = fopen("/sys/devices/system/cpu/cpu0/cache/index0/coherency_line_size", "r");
    if(NULL == filep)
    {
        perror( "get_doubleword_per_cacheline: failed to open sysfs file for "
                "cacheline size");
        ret = -1;
        goto error0;
    }
    
    ret = fscanf(filep, "%li", &cacheline_size);
    if(1 != ret)
    {
        perror("get_doubleword_per_cacheline: fscanf failed");
        ret = -1;
        goto error1;
    }

    ret = (int32_t)(cacheline_size / sizeof(int32_t));

error1:
    fclose(filep);
error0:
    return ret;
}

void cachelinearray_free(void)
{
    if(NULL != mapped_region)
    {
        munmap(mapped_region, mapped_region_size);
        mapped_region = NULL;
        mapped_region_size = 0;
    }
}

int cachelinearray_init(int32_t doubleword_per_cacheline, int32_t graph_index, int fd)
{
    int ret = 0;
    size_t read_ret;
    int32_t cacheline[doubleword_per_cacheline];
    
    read_ret =  read(fd, cacheline, (sizeof(int32_t) * doubleword_per_cacheline)); 
    if(read_ret != (sizeof(int32_t) * doubleword_per_cacheline))
    {
        if(-1 == read_ret)
        {
            perror("cachelinearray_init: read failed");
        }
        else
        {
            fprintf(stderr, "cachelinearray_init: read returned fewer bytes "
                            "than requested");
        }
        ret = -1;
        goto error0;
    }
    
    mapped_region_size = cacheline[graph_index];
    mapped_region = mmap(   NULL, mapped_region_size, 
                            PROT_READ, 
                            (MAP_PRIVATE | MAP_LOCKED | MAP_POPULATE),
                            fd, (off_t)0);
    if(MAP_FAILED == mapped_region)
    {
        perror("cachelinearray_init: mmap failed!");
        mapped_region = NULL;
        mapped_region_size = 0;
        ret = -1;
        goto error0;
    }
    
    cache_line_array = (int32_t*)mapped_region;
    
error0:
    return ret;
}

char *usage_string 
	= "[-f <input file name>] [-r [-p <real-time priority>]] [-g <loop index>] [ -i <loop iterations>]";
char *optstring = "f:rp:g:i:";

int main (int argc, char * const * argv)
{
	int ret;

    /*Variables for parsing options.*/
    char    *inputfile_name = "./membound_input.dat";
    int     fd;
    
    /*Variables for setting up the scheduling priority.*/
    unsigned char rflag = 0;
    double desired_priority = 0.0;
    
    /*Variables relating to the workload itself*/
    int32_t doubleword_per_cacheline;
    int32_t graphindex = 0;
    int64_t loopiterations = 10000000000;
    int32_t m;
    
    /*Variables related to timing*/
    uint64_t	ns_start, ns_end, ns_diff;
        
	/*Process input arguments.*/
	while ((ret = getopt(argc, argv, optstring)) != -1)
	{
		switch(ret)
		{
            case 'f':
                inputfile_name = optarg;
                break;

            case 'r':
                rflag = 1;
                break;
            
            case 'p':
                errno = 0;
                desired_priority = strtod(optarg, NULL);
                if( (errno == 0))
                {
                    if( (desired_priority >= 0.0) && 
                        (desired_priority <= 1.0) )
                    {
                        break;
                    }
                    else
                    {
                        fprintf(stderr, "main: Desired priority must be between "
                                        "0.0 (minimum priority) and 1.0 "
                                        "(maximum priority).\n");
                        exit(EXIT_FAILURE);
                    }
                }
                else
                {
                    perror("main: Failed to parse the p option");
                    exit(EXIT_FAILURE);
                }
                break;

			case 'i':
				errno = 0;
				loopiterations = (uint64_t)strtoull(optarg, NULL, 10);
				if(errno)
				{
					perror("main: Failed to parse the i option");
					exit(EXIT_FAILURE);
				}
				break;

            case 'g':
                errno = 0;
				graphindex = (uint32_t)strtoul(optarg, NULL, 10);
				if(errno)
				{
					perror("main: Failed to parse the g option");
					exit(EXIT_FAILURE);
				}
				break;

			default:
				fprintf(stderr, "main: Bad option %c!\nUsage %s %s!\n", 
				                (char)ret, argv[0], usage_string);
				ret = -EINVAL;
				goto exit0;
		}
	}

    /*Check that there are no more arguments.*/
	if(optind != argc)
	{
		fprintf(stderr, "main: Usage %s %s!\n", argv[0], usage_string);
		ret = -EINVAL;
		goto exit0;
	}

    /*Open the input file.*/
    fd = open(inputfile_name, O_RDONLY);
    if(-1 == fd)
    {
        fprintf(stderr, "main: Failed to open the input file: \"%s\". ", 
                        inputfile_name);
        perror("open failed");
        ret = -EINVAL;
        goto exit0;
    }    

    /*Get the number of int32_t's in a cacheline.*/
    doubleword_per_cacheline = get_doubleword_per_cacheline();
    if(-1 == doubleword_per_cacheline)
    {
        fprintf(stderr, "main: get_doubleword_per_cacheline failed!\n");
        ret = -1;
        goto exit1;
    }

    /*Setup the cache-line array.*/
    ret = cachelinearray_init(doubleword_per_cacheline, graphindex, fd);
    if(-1 == ret)
    {
        fprintf(stderr, "main: cachelinearray_init failed!\n");
        goto exit1;
    }

    /*Setup real-time scheduling parameters if necessary.*/
    if(1 == rflag)
    {
        ret = realtime_init(desired_priority);
        if(-1 == ret)
        {
            fprintf(stderr, "main: realtime_init failed!\n");
            goto exit1;
        }
    }

    /*Time and enter the main loop*/
    ns_start = getns();
    m = mainloop(   doubleword_per_cacheline, 
                    graphindex, 
                    loopiterations);
    ns_end = getns();
    
    /*Compute the elapsed time*/
    ns_diff = ns_end - ns_start;
    
    /*Print the elapsed time*/
    printf("\n%s: final m = %i, elapsed time = %lluns\n\n", 
            argv[0], m, (unsigned long long int)ns_diff);
    
    /*Cleanup*/
    cachelinearray_free();
exit1:
    close(fd);
exit0:
    if(-1 == ret)
    {
        fprintf(stderr, "%s: main failed!\n", argv[0]);
    }
    
    return ret;
}


// workload_timing
// 
// a generic program to create a log of job execution times for the PeSoRTA workloads

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdint.h>

#include <errno.h>

#include <sched.h>

#include <sys/mman.h>

#include <sys/types.h>

#include <sys/io.h>

#include <time.h>

/*****************************************************************************/
//				TSC related Code
/*************************************************************************/

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

/*****************************************************************************/


#include "PeSoRTA.h"

char *usage_string 
	= "[-j <maxjobs>] [-r] [ -R <workload root directory>] [-C <config file>] [-L <output file>]";
char *optstring = "j:rR:C:L:";

int main (int argc, char * const * argv)
{
	int ret;

    /*variables for parsing options*/
	unsigned char jflag = 0;
	long maxjobs = 0;
    unsigned char rflag = 0;
    
    char *workload_root_dir = "./";
    char *config_file = "config";
    char *logfile_name = "timing.csv";

    /*working directory*/
    void    *buffer_p = NULL;
    char    *cwd_name_buffer = NULL;
    char    *cwd_name = NULL;
    int     cwd_name_length = 0;
    
    /*the workload state*/
    void *workload_state = NULL;	
    
	/*variables for the scheduler*/
    pid_t my_pid;
    int max_priority;
    struct sched_param sched_param;

    /*timing log*/
    long possiblejobs;
	long jobi;
    uint64_t *log_mem; 

    /*logfile*/
    FILE *logfile_h;

	uint64_t	ns_start, ns_end, ns_diff;

	/* process input arguments */
	while ((ret = getopt(argc, argv, optstring)) != -1)
	{
		switch(ret)
		{
			case 'j':
				errno = 0;
				maxjobs = strtol(optarg, NULL, 10);
				if(errno)
				{
					perror("Failed to parse the j option");
					exit(EXIT_FAILURE);
				}
				jflag = 1;
				break;

            case 'r':
                rflag = 1;
                break;

            case 'R':
                workload_root_dir = optarg;
                break;

            case 'C':
                config_file = optarg;
                break;

            case 'L':
                logfile_name = optarg;
                break;
                
			default:
				fprintf(stderr, "ERROR: Bad option %c!\nUsage %s %s!\n", 
				                (char)ret, argv[0], usage_string);
				ret = -EINVAL;
				goto exit0;
		}
	}

	if(optind != argc)
	{
		fprintf(stderr, "ERROR: Usage %s %s!\n", argv[0], usage_string);
		ret = -EINVAL;
		goto exit0;
	}

    /*Save the current working directory*/
    cwd_name_length = 512;
    do
    {
        buffer_p = realloc(cwd_name_buffer, sizeof(char)*cwd_name_length);
        if(NULL == buffer_p)
        {
            fprintf(stderr, "ERROR: (%s) main)  realloc failed to allocate memory for "
                            "the current directory name buffer ", workload_name());
            perror("");
            ret = -1;
            goto exit1;
        }
        else
        {
            cwd_name_buffer = (char*)buffer_p;
        }
        
        errno = 0;
        cwd_name = getcwd(cwd_name_buffer, cwd_name_length);
        if(NULL == cwd_name)
        {
            if(ERANGE == errno)
            {
                cwd_name_length = cwd_name_length + 512;
            }
            else
            {
                fprintf(stderr, "ERROR: (%s) main)  getcwd failed ", workload_name());
                perror("");
                ret = -1;
                goto exit0;
            }
        }
    }while(NULL == cwd_name);
    
    /*Cheange to the desired working directory*/
    ret = chdir(workload_root_dir);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: (%s) main)  getcwd failed to change the current working "
                        "directory to the desired directory \"%s\" ", 
                        workload_name(), workload_root_dir);
        perror("");
        ret = -1;
        goto exit0;
    }

    /*Initialize the workload*/
    ret = workload_init(config_file, &workload_state, &possiblejobs);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: (%s) main) workload_init failed\n", workload_name());
        goto exit0;
    }
    
    /*Check if the workload returned a valid possiblejobs*/
    if(possiblejobs < 0)
    {
        /*Set it to an arbitrarily high value*/
        possiblejobs = 10000;
    }
    
    /*Check the number of jobs*/
    if(jflag == 0)
    {
        maxjobs = possiblejobs;
    }

	/*Allocate space for the timing log_mem*/
    log_mem = (uint64_t*)malloc(maxjobs * sizeof(uint64_t));
    if(NULL == log_mem)
    {
        fprintf(stderr, "ERROR: failed to allocate memory for timing log_mem\n");
        perror("ERROR: malloc failed in main");
        goto exit1;
    }

    if(rflag == 1)
    {
        ret = mlockall(MCL_CURRENT);
        if(ret == -1)
        {
            perror("ERROR: mlock failed in main");
            goto exit2;
        }

        my_pid = getpid();
        
        max_priority = sched_get_priority_max(SCHED_FIFO);
        if(max_priority == -1)        
        {
            perror("ERROR: sched_get_priority_max failed in main");
            goto exit3;
        }

        sched_param.sched_priority = max_priority;
        ret = sched_setscheduler(my_pid, SCHED_FIFO, &sched_param);
        if(ret == -1)
        {
            fprintf(stderr, "ERROR: failed to set real-time priority!\n");
            perror("ERROR: sched_setsceduler failed in main");
            goto exit3;
        }
    }

	/* the main job loop */
	for(jobi = 0; jobi < maxjobs; jobi++)
	{
	    /*run and time the next job*/
		ns_start = getns();
		ret = perform_job(workload_state);
    	ns_end = getns();

        /*make sure the job didn't incur any errors*/
		if(ret < 0)
		{
            fprintf(stderr, "ERROR: main) (%s) perform_job returned -1 for job %li\n", 
                workload_name(), jobi);
            /*correct the number of executed jobs*/
            maxjobs = jobi;
			break;
		}
		else if(ret == 1)
		{
		    /*no more jobs to perform*/
		    maxjobs = jobi;
		}

        /*log_mem the time*/
        ns_diff = ns_end - ns_start;

        log_mem[jobi] = ns_diff;
	}

    /*Cheange back to the original working directory*/
    ret = chdir(cwd_name);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: (%s) main)  getcwd failed to change the current working "
                        "directory to the desired directory \"%s\" ", 
                        workload_name(), cwd_name);
        perror("");
        ret = -1;
        goto exit3;
    }

    /*open the log file*/
    logfile_h = fopen(logfile_name, "w");
    if(NULL == logfile_h)
    {
        fprintf(stderr, "ERROR: Failed to open log file \"%s\"!\n", logfile_name);
        perror("ERROR: fopen failed in main");
        goto exit3;
    }
    
    /*write the log_mem out to file*/
    for(jobi = 0; jobi < maxjobs; jobi++)
    {
        ret = fprintf(logfile_h, "%lu,\n", log_mem[jobi]);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: Failed to write log index %li to log file!\n", jobi);
            perror("ERROR: fprintf failed in main");
            goto exit4;
        }
    }

    /*undo everything*/
exit4:
    fclose(logfile_h);
exit3:
    if(rflag == 1)
    {
        munlockall();
    }
exit2:
	free(log_mem);
exit1:    
    workload_uninit(workload_state);
exit0:
    if(NULL != cwd_name_buffer)
    {
        free(cwd_name_buffer);
    }

	return 0;
}


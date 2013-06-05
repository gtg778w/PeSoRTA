#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "PeSoRTA.h"
#include "PeSoRTA_helper.h"

#include "loadgen.h"
#include "sqrwav.h"

typedef struct PeSoRTA_sqrwav_s
{
    struct sqrwav_struct sqrwav;
    int32_t  work_func_state;
	//variables for the scheduler
	unsigned long jobs_remaining;
} PeSoRTA_sqrwav_t;

/*
    - a simple function that returns the name and any description of the workload as a static string
*/
char *workload_name(void)
{
    return "sqrwav";
}

/*
"-j: number of jobs, (positive integer)\n"\
"-P: period of sqare-wave (number of jobs)\n"\
"-D: duty cycle (fraction of the period)\n"\
"-d: initial value of index. (positive number of jobs)\n"\
"-M: Maximum nominal value (computation time in ms).\n"\
"-m: minimum nominal value (computation time in ms).\n"\
"-N: noise ratio (fraction of the nominal value).\n"\
*/
static int PeSoRTA_sqrwav_parse_config(char *configfile_name, PeSoRTA_sqrwav_t *workload_state)
{
    int ret = 0;
    FILE *configfile_p;

	//parsing variables
    char *optstring = "j:P:D:d:M:m:N:";
    int  opt;
    char *optarg;
    
    double temp_d;
    
    /*Initialize workload_state->p*/
    workload_state->jobs_remaining = 10000; 
    
    workload_state->sqrwav.period = 10000;
    workload_state->sqrwav.duty_cycle = 0.5;
    workload_state->sqrwav.minimum_nominal_value = (uint64_t)(1*ipms);
    workload_state->sqrwav.maximum_nominal_value = (uint64_t)(5*ipms);
    workload_state->sqrwav.noise_ratio = 0.2;
    workload_state->sqrwav.index = 0;
    workload_state->sqrwav.rangen_state = 0;
    
    /*Open the config file*/
    configfile_p = fopen(configfile_name, "r");
    if(NULL == configfile_p)
    {
        fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) fopen failed to open file "
                        "\"%s\" :", configfile_name);
        perror("");
        ret = -1;
        goto error0;
    }

    /*Parse the file, line by line*/
    while(!feof(configfile_p))
    {
        ret = PeSoRTA_getconfigopt(configfile_p, optstring, &opt, &optarg);
        if(ret == -1)
        {
            if(!feof(configfile_p))
            {
                fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) "
                                "PeSoRTA_getconfigopt failed\n");
                goto error1;
            }
            else
            {
                ret = 0;
                continue;
            }
        }
        
        if(opt == -1)
        {
                fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) config file "
                                "contains bad line:\n\t\"%s\"\n", optarg);                
                ret = -1;
                free(optarg);
                goto error1;
        }

        if(optarg == NULL)
        {
            fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) config file contains "
                            "valid option (%c) without argument!\n", (int)opt);
            ret = -1;
            goto error1;
        }
        
        switch(opt)
        {
            case 'j':
				errno = 0;
				workload_state->jobs_remaining = strtoul(optarg, NULL, 10);
				if(errno)
				{
                    fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) Failed to "
					                "parse the j option\n");
					ret = -1;
			        free(optarg);
					goto error1;
				}
				break;

            case 'P':
                errno = 0;
                workload_state->sqrwav.period 
                    = (uint64_t)strtoul(optarg, NULL, 10);
				if(errno)
				{
                    fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) Failed to parse " 
					                "the P option\n");
					ret = -1;
			        free(optarg);
					goto error1;
				}
                break;

            case 'D':
                errno = 0;
                workload_state->sqrwav.duty_cycle 
                    = (double)strtod(optarg, NULL);
				if(errno)
				{
                    fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) Failed to parse "
					                "the D option\n");
					ret = -1;
			        free(optarg);
					goto error1;
				}
                break;

            case 'd':
                errno = 0;
                workload_state->sqrwav.index 
                    = (uint64_t)strtoul(optarg, NULL, 10);
				if(errno)
				{
                    fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) Failed to parse "
					                "the d option\n");
					ret = -1;
			        free(optarg);
					goto error1;
				}
                break;

            case 'M':
                errno = 0;
                temp_d = (double)strtod(optarg, NULL);
				if(errno)
				{
                    fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) Failed to "
					                "parse the M option\n");
					ret = -1;
                    free(optarg);
					goto error1;
				}
                workload_state->sqrwav.maximum_nominal_value 
                    = (uint64_t)(ipms * temp_d);
                break;

            case 'm':
                errno = 0;
                temp_d = (double)strtod(optarg, NULL);
				if(errno)
				{
                    fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) Failed to "
					                "parse the m option\n");
					ret = -1;
    		        free(optarg);
					goto error1;
				}
                workload_state->sqrwav.minimum_nominal_value
                    = (uint64_t)(ipms * temp_d);
                break;

            case 'N':
                errno = 0;
                workload_state->sqrwav.noise_ratio 
                    = (double)strtod(optarg, NULL);
				if(errno)
				{
                    fprintf(stderr, "ERROR: PeSoRTA_sqrwav_parse_config) Failed to "
					                "parse the N option\n");
					ret = -1;
                    free(optarg);
					goto error1;
				}
                break;
        }
        
        free(optarg);
    };

error1:
    fclose(configfile_p);
error0:
    return ret;
}

/*
    allocate space for the relevant data structures
    the workload root directory is not necessary in this case
    read the config file to determine the sqrwav params
    if the config file contains no limits to the number of jobs, set job count to -1
*/
int workload_init(char *configfile, void **state_p, long *job_count_p)
{
    int ret = 0;
    
    PeSoRTA_sqrwav_t *workload_state;

    /*callibrate the load generator*/
    /*warmup run*/
    callibrate(3000);
    /*callibration run*/
    callibrate(30000000);
        
    /*allocate space for the workload state*/
    workload_state = malloc(sizeof(PeSoRTA_sqrwav_t));
    if(NULL == workload_state)
    {
        fprintf(stderr, "ERROR: (sqrwav) workload_init) malloc failed to allocate "
                        "space for the workload state");
        perror("");
        ret = -1;
        goto error0;
    }
    
    /*initialize the workload state*/
    workload_state->jobs_remaining = 0;
    *state_p = workload_state;
        
    /*read the config file*/
    ret = PeSoRTA_sqrwav_parse_config(configfile, workload_state);
    if(ret == -1)
    {
        fprintf(stderr, "ERROR: (sqrwav) workload_init) PeSoRTA_sqrwav_parse_config "
                        "failed\n");
        ret = -1;
        goto error0;   
    }
    
    /*set return values*/
    *state_p = workload_state;
    *job_count_p = workload_state->jobs_remaining;

error0:
    return ret;
}

/*
    Do the actual "computation"
    Use the sqrwav module to determine the number of inner-loop iterations.
    Pass the number of loop iterations to the work_function in loadgen.
*/
int perform_job(void *state)
{
    int ret = 0;
    
    PeSoRTA_sqrwav_t *workload_state = (PeSoRTA_sqrwav_t*)state;

    struct sqrwav_struct *sqrwav_p = &(workload_state->sqrwav);
    int32_t work_func_state = workload_state->work_func_state;
    
    int64_t job_length;

    /*If there are no more jobs left, just return 0*/
    if(workload_state->jobs_remaining <= 0)
    {
        return 1;
    }
//fprintf(stderr, "actually doing some work!\n");

    job_length = sqrwav_next(sqrwav_p);

    workload_state->work_func_state
        = work_function(job_length, 
            workload_state->work_func_state);
    
    workload_state->work_func_state = work_func_state;

    (workload_state->jobs_remaining)--;
    /*Set ret to 1 if there are no more jobs left*/
    ret = (workload_state->jobs_remaining <= 0);
    return ret;
}

int workload_uninit(void *state)
{
    PeSoRTA_sqrwav_t *workload_state = (PeSoRTA_sqrwav_t*)state;
    
    if(NULL != workload_state)
    {
        free(workload_state);
    }
    
    return 0;
}


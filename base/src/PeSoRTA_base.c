#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "PeSoRTA.h"
#include "PeSoRTA_helper.h"

typedef struct PeSoRTA_base_s
{
    int32_t jobcount;
    int32_t jobcompleted;
} PeSoRTA_base_t;

/*
    - a simple function that returns the name and any description of the workload as a static string
*/
char *workload_name(void)
{
    return "base";
}

/*
"-J: number of jobs \n"\
*/
static int PeSoRTA_base_parse_config(  char *configfile_name, 
                                       int32_t *job_count)
{
    int ret = 0;
    FILE *configfile_p;

	/*parsing variables*/
    char *optstring = "J:";
    int  opt;
    char *optarg;

    /*Open the config file*/
    configfile_p = fopen(configfile_name, "r");
    if(NULL == configfile_p)
    {
        fprintf(stderr, "ERROR: PeSoRTA_base_parse_config) fopen failed\n");
        ret = -1;
        goto exit0;
    }

    /*Parse the file, line by line*/
    while(!feof(configfile_p))
    {
        ret = PeSoRTA_getconfigopt(configfile_p, optstring, &opt, &optarg);
        if(ret == -1)
        {
            if(!feof(configfile_p))
            {
                fprintf(stderr, "ERROR: PeSoRTA_base_parse_config) "
                                "PeSoRTA_getconfigopt failed\n");
                ret = -1;
                goto exit1;
            }
            else
            {
                ret = 0;
                continue;
            }
        }
        
        if(opt == -1)
        {
                fprintf(stderr, "ERROR: PeSoRTA_base_parse_config) config file "
                                "contains bad line:\n\t\"%s\"\n", optarg);                
                ret = -1;
                free(optarg);
                goto exit1;
        }

        if(optarg == NULL)
        {
            fprintf(stderr, "ERROR: PeSoRTA_base_parse_config) config file contains "
                            "valid option (%c) without argument!\n", (int)opt);
            ret = -1;
            goto exit1;
        }
        
        switch(opt)
        {
            case 'J':
                *job_count = (int32_t)strtol(optarg, NULL, 0);
                free(optarg);
                break;
        }/*switch(opt)*/
    }/*while(!feof(configfile_p))*/
    
exit1:
    fclose(configfile_p);
    
exit0:
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
    
    int32_t job_count = 1000;
    
    PeSoRTA_base_t *workload_state = NULL;
    
    /*allocate space for the workload state*/
    workload_state = calloc(1, sizeof(PeSoRTA_base_t));
    if(NULL == workload_state)
    {
        fprintf(stderr, "ERROR: (base) workload_init) calloc failed to allocate "
                        "memory for a PeSoRTA_base_t object.\n");
        ret = -1;
        goto error0;
    }
    
    /*read the config file*/
    if(NULL != configfile)
    {
        ret = PeSoRTA_base_parse_config(    configfile, 
                                            &job_count);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: (base) workload_init) "
                            "PeSoRTA_base_parse_config failed\n");
            goto error1;
        }
    }
    
    /*Setup the workload_state data structure*/
    workload_state->jobcount    = job_count;
    workload_state->jobcompleted= 0;
    
    *state_p = workload_state;
    *job_count_p = job_count;
    
    return 0;

error1:
    free(workload_state);
error0:
    *state_p = NULL;
    *job_count_p = 0;
    return -1;
}

/*
    Do the actual "computation"
*/
int perform_job(void *state)
{
    int ret;
    
    PeSoRTA_base_t *workload_state = (PeSoRTA_base_t*)state;

    if((workload_state->jobcompleted) >= (workload_state->jobcount))
    {
        ret = 1;
    }
    else
    {
        (workload_state->jobcompleted)++;
        ret = 0;
    }
    
    return ret;
}

/*
    
*/
int workload_uninit(void *state)
{
    PeSoRTA_base_t *workload_state = (PeSoRTA_base_t*)state;
    
    if(NULL == workload_state)
    {
        goto exit0;
    }

    /*Free the main workload_state data structure*/
    free(workload_state);       

exit0:
    return 0;
}


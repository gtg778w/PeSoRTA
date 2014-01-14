#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "PeSoRTA.h"
#include "PeSoRTA_helper.h"
#include "membound.h"

typedef struct PeSoRTA_membound_s
{
    int32_t jobcount;
    int32_t jobcompleted;
    membound_t membound;
} PeSoRTA_membound_t;

/*
    - a simple function that returns the name and any description of the workload as a static string
*/
char *workload_name(void)
{
    return "membound";
}

/*
"-d: name of data file \n"\
"-g: graph index \n"\
"-i: loop iterations \n"\
"-j: number of jobs \n"\
*/
static int PeSoRTA_membound_parse_config(   char    *configfile_name, 
                                            char    **datafile_name_p,
                                            int32_t *graph_index_p,
                                            int64_t *loop_iterations_p,
                                            int32_t *job_count_p)
{
    int ret;
    FILE *configfile_p;

	/*parsing variables*/
    char *optstring = "d:g:i:j:";
    int  opt;
    char *optarg;

    char * const default_filename = "./membound_input.dat";
    char *datafile_name = default_filename;
    int32_t graph_index = 0;
    int64_t loop_iterations = 1000000;
    int32_t job_count = 10000;

    /*Open the config file*/
    configfile_p = fopen(configfile_name, "r");
    if(NULL == configfile_p)
    {
        fprintf(stderr, "ERROR: PeSoRTA_membound_parse_config) fopen failed\n");
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
                fprintf(stderr, "ERROR: PeSoRTA_membound_parse_config) "
                                "PeSoRTA_getconfigopt failed\n");
                goto error1;
            }
            else
            {
                continue;
            }
        }
        
        if(opt == -1)
        {
                fprintf(stderr, "ERROR: PeSoRTA_membound_parse_config) config file "
                                "contains bad line:\n\t\"%s\"\n", optarg);
                free(optarg);
                goto error1;
        }

        if(optarg == NULL)
        {
            fprintf(stderr, "ERROR: PeSoRTA_membound_parse_config) config file contains "
                            "valid option (%c) without argument!\n", (int)opt);
            ret = -1;
            goto error1;
        }
    
        switch(opt)
        {
            case 'd':
                datafile_name = optarg;
                break;
            case 'g':
                graph_index = (int32_t)strtol(optarg, NULL, 0);
                free(optarg);
                break;
            case 'i':
                loop_iterations = (int64_t)strtoll(optarg, NULL, 0);
                free(optarg);
                break;
            case 'j':
                job_count = (int32_t)strtol(optarg, NULL, 0);
                free(optarg);
                break;
        }/*switch(opt)*/
    }/*while(!feof(configfile_p))*/    
    
    /*Set the output variables*/
    *datafile_name_p = datafile_name;
    *graph_index_p = graph_index;
    *loop_iterations_p = loop_iterations;
    *job_count_p = job_count;
    
    fclose(configfile_p);
    
    return 0;
    
error1:
    if(default_filename != datafile_name)
    {
        free(datafile_name);
    }
    fclose(configfile_p);
error0:
    return -1;
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
    
    char *datafile_name;
    int32_t graph_index;
    int64_t loop_iterations;
    int32_t job_count;
    
    PeSoRTA_membound_t *workload_state = NULL;
    
    /*allocate space for the workload state*/
    workload_state = calloc(1, sizeof(PeSoRTA_membound_t));
    if(NULL == workload_state)
    {
        fprintf(stderr, "ERROR: (membound) workload_init) calloc failed to allocate "
                        "memory for a PeSoRTA_membound_t object.\n");
        ret = -1;
        goto error0;
    }
    
    /*read the config file*/
    if(NULL != configfile)
    {
        ret = PeSoRTA_membound_parse_config(configfile,
                                            &datafile_name,
                                            &graph_index,
                                            &loop_iterations,
                                            &job_count);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: (membound) workload_init) "
                            "PeSoRTA_membound_parse_config failed\n");
            goto error1;
        }
    }
    
    /*initialize the membound workload*/
    ret = membound_init(&(workload_state->membound),
                        datafile_name,
                        graph_index,
                        loop_iterations);
    if(ret < 0)
    {
            fprintf(stderr, "ERROR: (membound) workload_init) "
                            "membound_init failed\n");
            goto error2;   
    }
    
    /*Setup the workload_state data structure*/
    workload_state->jobcount    = job_count;
    workload_state->jobcompleted= 0;
    
    *state_p = workload_state;
    *job_count_p = job_count;
    
    /*Free temporarily allocated memory*/
    free(datafile_name);
    
    return 0;

error2:
    free(datafile_name);
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
    
    PeSoRTA_membound_t *workload_state = (PeSoRTA_membound_t*)state;

    if((workload_state->jobcompleted) >= (workload_state->jobcount))
    {
        ret = 1;
    }
    else
    {
        membound_mainloop(&(workload_state->membound));
        (workload_state->jobcompleted)++;
        ret = 0;
    }
    
    return ret;
}

int workload_uninit(void *state)
{
    PeSoRTA_membound_t *workload_state = (PeSoRTA_membound_t*)state;
    
    if(NULL == workload_state)
    {
        goto exit0;
    }

    /*Free membound-rlated resources*/
    membound_free(&(workload_state->membound));

    /*Free the main workload_state data structure*/
    free(workload_state);       

exit0:
    return 0;
}


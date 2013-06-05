#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "PeSoRTA.h"
#include "PeSoRTA_helper.h"

#include "sphinxwrapper.h"
#include "sw_wav.h"

typedef struct PeSoRTA_cmusphinx_s
{
    sw_data_t   *sw_data_p;
    
    int16_t     *data;
    size_t      total_read;
    size_t      total_decoded;
    
    size_t      frame_length;
} PeSoRTA_cmusphinx_t;

/*
    - a simple function that returns the name and any description of the workload as a static string
*/
char *workload_name(void)
{
    return "cmusphinx";
}

/*
"-I: input file name (file name) \n"\
"-C: config file name for cmusphinx \n"\
"-s: the silence threshold (ms)\n"\
"-f: frame-rate (frames per second)\n"\
*/
static int PeSoRTA_cmusphinx_parse_config(  char *configfile_name, 
                                            char **input_file_name,
                                            char **config_file_name,
                                            double *silence_thresh,
                                            double *frame_rate)
{
    int ret = 0;
    FILE *configfile_p;

	/*parsing variables*/
    char *optstring = "I:C:s:f:";
    int  opt;
    char *optarg;

    /*Set initial values of the output variables*/
    *input_file_name = NULL;
    *config_file_name = NULL;
    
    /*Leave silence_thresh and frame rate untouched*/

    /*Open the config file*/
    configfile_p = fopen(configfile_name, "r");
    if(NULL == configfile_p)
    {
        fprintf(stderr, "ERROR: PeSoRTA_cmusphinx_parse_config) fopen failed\n");
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
                fprintf(stderr, "ERROR: PeSoRTA_cmusphinx_parse_config) "
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
                fprintf(stderr, "ERROR: PeSoRTA_cmusphinx_parse_config) config file "
                                "contains bad line:\n\t\"%s\"\n", optarg);                
                ret = -1;
                free(optarg);
                goto exit1;
        }

        if(optarg == NULL)
        {
            fprintf(stderr, "ERROR: PeSoRTA_cmusphinx_parse_config) config file contains "
                            "valid option (%c) without argument!\n", (int)opt);
            ret = -1;
            goto exit1;
        }
        
        switch(opt)
        {
            case 'I':
                *input_file_name = optarg;
                break;

            case 'C':
                *config_file_name = optarg;
                break;
                
            case 's':
                *silence_thresh = (double)strtod(optarg, NULL);
                free(optarg);
                break;
            
            case 'f':
                *frame_rate = (double)strtod(optarg, NULL);
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
    
    PeSoRTA_cmusphinx_t *workload_state;
    
    char    *input_file_name;
    char    *config_file_name;
        
    /*default values for the frame rate and silence threshold*/
    double  silence_thresh = 50.0;
    double  frame_rate = 20.0;
    
    /*Local variables for processing the wave file*/
    FILE* filep;
    sw_wavheader_t wavheader;
    int16_t *buffer = NULL;
    size_t ret_size;
    size_t buffer_size = 0;
    size_t total_read = 0;

    sw_data_t *p_sw_data = NULL;

    /*allocate space for the workload state*/
    workload_state = calloc(1, sizeof(PeSoRTA_cmusphinx_t));
    if(NULL == workload_state)
    {
        fprintf(stderr, "ERROR: (cmusphinx) workload_init) malloc failed to allocate "
                        "memory for a PeSoRTA_cmusphinx_t object.\n");
        ret = -1;
        goto error0;
    }
    
    /*read the config file*/
    ret = PeSoRTA_cmusphinx_parse_config(   configfile, 
                                            &input_file_name,
                                            &config_file_name,
                                            &silence_thresh,
                                            &frame_rate);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: (cmusphinx) workload_init) "
                        "PeSoRTA_cmusphinx_parse_config failed\n");
        goto error1;
    }
    
    /*Validate the obtained argument*/
    if(NULL == input_file_name)
    {
        fprintf(stderr, "ERROR: (cmusphinx) workload_init) "
                        "the PeSoRTA_cmusphinx config file must specify an input wav "
                        "file\n");
        goto error2;
    }
    
    if(silence_thresh < (1.0/(double)SW_DEFAULT_SAMPLE_RATE))
    {
        fprintf(stderr, "ERROR: (cmusphinx) workload_init) the PeSoRTA_cmusphinx config "
                        "file must specify an input wav file\n");
        goto error2;
    }
    
    /*Open the input file*/
    filep = fopen(input_file_name, "r");
    if(NULL == filep)
    {
        fprintf(stderr, "ERROR: (cmusphinx) workload_init) Failed to open the input file "
                        "\"%s\"\n", input_file_name);
        perror("ERROR: (cmusphinx) workload_init) fopen failed");
        goto error2;
    }
    
    /*Read the WAV header*/
    ret_size = fread(&wavheader, sizeof(sw_wavheader_t), 1, filep);
    if(ret_size < 1)
    {
        fprintf(stderr, "ERROR: (cmusphinx) workload_init) failed to read the header "
                        "from the wav file \"%s\"\n", input_file_name);
        if(ret_size < 0)
        {
            perror("ERROR: (cmusphinx) workload_init) fread failed");
        }
        goto error3;
    }
    
    /*Verify the wav header*/
    ret = sw_verify_wav(&wavheader);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: (cmusphinx) workload_init) the wav file, \"%s\" is not "
                        "properly formatted.\n", input_file_name);
        goto error3;
    }

    /*Allocate space for the buffer*/
    buffer_size = sw_get_wav_samples(&wavheader);
    buffer = (int16_t*)malloc(buffer_size*sizeof(int16_t));
    if(NULL == buffer)
    {
        fprintf(stderr, "ERROR: (cmusphinx) workload_init) Failed to allocate space to"
                        " read in samples from the wav file.\n");
        perror("ERROR: (cmusphinx) workload_init) malloc failed");
        goto error3;
    }
    
    /*Read the samples into the buffer*/
    ret_size = fread(   buffer, 
                        sizeof(int16_t), 
                        buffer_size, 
                        filep);
    if(ret_size < buffer_size)
    {
        fprintf(stderr, "ERROR: (cmusphinx) workload_init) fread failed to read %li "
                        "samples\n", buffer_size);
        if(ret_size < 0)
        {
            perror("ERROR: (cmusphinx) workload_init) fread failed");
        }
        goto error4;
    }

    total_read = total_read + ret_size;
    
    /*Initialize the sw_data_t object*/
    ret = allocate_sw_data( &p_sw_data, 
                            config_file_name, 
                            (int32_t)((double)SW_DEFAULT_SAMPLE_RATE * silence_thresh / 1000.0));
    if(ret < 0)
    {
        fprintf(stderr, "ERROR (cmusphinx) workload_init): allocate_sw_data failed\n");
        goto error4;
    }

    /*Callibrate the silence filter*/
    ret = sw_calib_silence(p_sw_data, buffer, total_read);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR (cmusphinx) workload_init): sw_calib_silence failed\n");
        goto error5;
    }

    /*free up and close used and unnecessary objects*/
    fclose(filep);

    if(NULL != input_file_name)
    {   
        free(input_file_name);
    }

    if(NULL != config_file_name)
    {  
        free(config_file_name);
    }
    
    /*Setup the workload_state data structure*/
    workload_state->sw_data_p = p_sw_data;
    workload_state->data = buffer;
    workload_state->total_read = total_read;
    workload_state->total_decoded = 0;
    workload_state->frame_length = (int32_t)(((double)SW_DEFAULT_SAMPLE_RATE)/frame_rate);
    
    *state_p = workload_state;
    /*Total number of frames rounded up*/
    *job_count_p = (total_read + (workload_state->frame_length) - 1)/(workload_state->frame_length);
    
    return 0;
error5:
    free_sw_data(&p_sw_data);
error4:
    free(buffer);
error3:
    fclose(filep);
error2:
    /*free the options strings*/
    if(NULL != input_file_name)
    {   
        free(input_file_name);
    }
    if(NULL != config_file_name)
    {  
        free(config_file_name);
    }
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
    int ret = 0;
    
    PeSoRTA_cmusphinx_t *workload_state = (PeSoRTA_cmusphinx_t*)state;
    sw_data_t *p_sw_data;

    int16_t *data;
    size_t total_read;
    size_t total_decoded;
    
    size_t samples_remaining;
    size_t frame_length;
    
    char *hyp;
    
    /*Check that the obtained state is valid*/
    if(NULL == workload_state)
    {
        ret = -1;
        goto exit0;
    }
    
    /*Unpack the structure*/
    p_sw_data = workload_state->sw_data_p;

    data = workload_state->data;
    total_read = workload_state->total_read;
    total_decoded = workload_state->total_decoded;
    
    samples_remaining = total_read - total_decoded;
    frame_length = workload_state->frame_length;
    
    /*Reduce the frame length if there are not enough samples left to decode*/
    frame_length =  (frame_length > samples_remaining)?
                          samples_remaining : frame_length;
    
    /*Decode the frame*/
    ret = sw_decode_speech(p_sw_data, &(data[total_decoded]), frame_length, &hyp);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR (sphinx main) perform_job) sw_decode_speech failed\n");
        ret = -1;
        goto exit0;
    }

    /*If a hyp was obtained, free it*/
    if(NULL != hyp)
    {
        free(hyp);
    }
    
    /*Update the number of samples decoded so far*/
    total_decoded = total_decoded + frame_length;
    
    /*If the all the samples have been decoded extract the last hyp*/
    if(total_decoded == total_read)
    {
        ret = sw_extractlast_hyp(p_sw_data,&hyp);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR (sphinx main) perform_job) "
                            "sw_extractlast_hyp failed\n");
            ret = -1;
            goto exit0;
        }
    
        /*If a hyp was obtained, free it*/
        if(NULL != hyp)
        {
            free(hyp);
        }
    }
    
    /*Update the workload state*/
    workload_state->total_decoded = total_decoded;
    
exit0:
    return ret;
}

/*

*/
int workload_uninit(void *state)
{
    PeSoRTA_cmusphinx_t *workload_state = (PeSoRTA_cmusphinx_t*)state;
    
    if(NULL == workload_state)
    {
        goto exit0;
    }

    /*Free the sphinx wrapper object*/
    free_sw_data(&(workload_state->sw_data_p));

    /*Free the samples*/
    if(NULL != workload_state->data)
    {
        free(workload_state->data);
    }

    /*Free the main workload_state data structure*/
    free(workload_state);       

exit0:
    return 0;
}


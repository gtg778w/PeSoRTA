#include <string.h>
#include "sphinxwrapper.h"

/*If reentrancy is expected, a lock should be applied to these variables before 
calling a function that uses sw_ad_read*/
static int16_t *sw_ad_buffer = NULL;
static int32_t  sw_ad_buffer_size = 0;

static int32_t sw_ad_read(ad_rec_t * ad, int16_t * buf, int32_t max)
{
    size_t read_amount = 0;
    
    read_amount = (max < sw_ad_buffer_size)? max : sw_ad_buffer_size;
    memcpy(buf, sw_ad_buffer, read_amount * sizeof(int16_t));
    
    sw_ad_buffer_size = sw_ad_buffer_size - read_amount;
    if(sw_ad_buffer_size > 0)
    {
        sw_ad_buffer = &(sw_ad_buffer[read_amount]);
    }
    else
    {
        sw_ad_buffer = NULL;
    }
    
    return read_amount;
}

int allocate_sw_data(   sw_data_t   **pp_sw_data, 
                        char    *psconfig_filename,
                        int32_t silence_thresh)
{
    sw_data_t *p_sw_data;
    cmd_ln_t *config;
    
    p_sw_data = (sw_data_t*)calloc(1, sizeof(sw_data_t));
    if(NULL == p_sw_data)
    {
        fprintf(stderr, "ERROR (cmusphinx) allocate_sw_data): calloc failed to allocate "
                        "memory for an sw_data_t object");
        perror("");
        goto error0;
    }

    p_sw_data->state = SW_STATE_SILENCE;
    p_sw_data->last_speech_sample = -1;
    p_sw_data->silence_thresh = silence_thresh;
    
    /* Disable all the logging */
    err_set_logfp(NULL);

    /*Create an empty cmd_ln_t object to initialize the ps_decoder_t object.
    Hopefully this will result in default values.*/
    config = cmd_ln_init(NULL, ps_args(), TRUE, NULL);
    if(NULL == config)
    {
        fprintf(stderr, "ERROR (cmusphinx) allocate_sw_data): cmd_ln_init failed\n");
        goto error1;
    }
    
    /*If a separate config file was provided, use it*/
    if(NULL != psconfig_filename)
    {
        if(NULL == cmd_ln_parse_file_r(config, ps_args(), psconfig_filename, TRUE))
        {
            fprintf(stderr, "ERROR (cmusphinx) allocate_sw_data): cmd_ln_parse_file_r "
                            "failed\n");
            goto error2;
        }
    }
    
    /*Initialize the decoder with the above configuration options*/
    p_sw_data->ps = ps_init(config);
    if(NULL == p_sw_data->ps)
    {
        fprintf(stderr, "ERROR (cmusphinx) allocate_sw_data): ps_init failed\n");
        goto error2;
    }
    
    /*Initialize the fake a/d device*/
    p_sw_data->ad_rec.sps   = SW_DEFAULT_SAMPLE_RATE;
    p_sw_data->ad_rec.bps   = sizeof(int16);

    /*Allocate the silence filtering module with the fake a/d device*/
    p_sw_data->cont = cont_ad_init(&(p_sw_data->ad_rec), sw_ad_read);
    if(NULL == p_sw_data->cont)
    {
        fprintf(stderr, "ERROR (cmusphinx) allocate_sw_data): calloc failed to allocate "
                        "memory for an sw_data_t object\n");
        goto error3;
    }
    
    /*Initialization is done*/
    cmd_ln_free_r(config);
    
    /*Set the output variable *pp_sw_data to the allocated and configured object*/
    *pp_sw_data = p_sw_data;
    
    return 0;

error3:
    ps_free(p_sw_data->ps);
error2:
    cmd_ln_free_r(config);
error1:
    free(p_sw_data);
error0:
    return -1;
}

void free_sw_data(sw_data_t **pp_sw_data)
{
    cont_ad_close((*pp_sw_data)->cont);
    ps_free((*pp_sw_data)->ps);
    free(*pp_sw_data);
    *pp_sw_data = NULL;
}

int sw_calib_silence(sw_data_t *p_sw_data, int16_t *calib_data, size_t data_size)
{
    int ret = 0;
    size_t required_size =  cont_ad_calib_size(p_sw_data->cont);
    size_t calibed_size = 0;
    
    required_size = (data_size < required_size)? data_size : required_size;
    
    do
    {
        ret = cont_ad_calib_loop(   p_sw_data->cont, 
                                    &(calib_data[calibed_size]),
                                    (int32_t)required_size);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR (cmusphinx) sw_calib_silence)  \"cont_ad_calib_loop\" "
                            "failed\n");
            goto exit0;
        }
        else
        {
            calibed_size += required_size;
            
            if( (data_size - calibed_size) < required_size)
            {
                required_size = data_size - calibed_size;
            }
        }

    }while( (1 == ret) && (required_size > 0));
    
exit0:
    return ret;
}

int sw_decode_speech(   sw_data_t *p_sw_data, 
                        int16_t *speech_data, 
                        size_t data_size, 
                        char **p_hyp_out)
{
    int ret = 0;
    
    char const* hyp = NULL;
    
    int16_t filtered_speech[(p_sw_data->cont->spf)];
    int32_t filter_buffer_size = p_sw_data->cont->spf;
    int32_t filtered_size = 0;
    
    cont_ad_t   *cont = p_sw_data->cont;
    
    sw_state_t  state = p_sw_data->state;
    
    ps_decoder_t    *ps = p_sw_data->ps;
    
    int32_t last_speech_sample = p_sw_data->last_speech_sample;
    int32_t samples_since_speech;
    
    int32_t silence_thresh = p_sw_data->silence_thresh;
    
    const char* uttid;
    
    char    *hyp_out = NULL;
    char    *helper = NULL;
    
    sw_ad_buffer = speech_data;
    sw_ad_buffer_size = data_size;
    
    while(sw_ad_buffer_size > 0)
    {
        filtered_size = cont_ad_read(cont, filtered_speech, filter_buffer_size);
        if(filtered_size < 0)
        {
            fprintf(stderr, "ERROR (cmusphinx) sw_decode_speech) cont_ad_read failed\n");
            ret = -1;
            goto exit0;
        }
        
        switch(state)
        {
            case SW_STATE_SILENCE:
                if(filtered_size > 0)
                {
                    /*Starting a new utterance*/
                    state = SW_STATE_SPEECH;

                    /*Update the last non-silent sample to be read*/
                    last_speech_sample = cont->read_ts;
                    
                    /*Start a new utterance*/
                    ret = ps_start_utt(ps, NULL);
                    if(ret < 0)
                    {
                        fprintf(stderr, "ERROR (cmusphinx) sw_decode_speech) ps_start_utt "
                                        "failed\n");
                        ret = -1;
                        goto exit0;
                    }
                    
                    /*Process the non-silent samples read so far*/
                    ret = ps_process_raw(   ps, 
                                            filtered_speech, 
                                            filter_buffer_size, 
                                            FALSE, 
                                            FALSE);
                    if(ret < 0)
                    {
                        fprintf(stderr, "ERROR (cmusphinx) sw_decode_speech) "
                                        "ps_process_raw failed\n");
                        ret = -1;
                        goto exit0;
                    }
                }
                else
                {
                    /*Wait till the next piece of data*/    
                }
                break;

            case SW_STATE_SPEECH:
                if(filtered_size > 0)
                {
                    /*Process the next set of non-silent samples*/
                    ret = ps_process_raw(   ps, 
                                            filtered_speech, 
                                            filter_buffer_size, 
                                            FALSE, 
                                            FALSE);
                    if(ret < 0)
                    {
                        fprintf(stderr, "ERROR (cmusphinx) sw_decode_speech) "
                                        "ps_process_raw failed\n");
                        ret = -1;
                        goto exit0;
                    }
                    
                    /*Update the last set of non-silent samples to be read*/
                    last_speech_sample = cont->read_ts;
                }
                else
                {
                    /*Check if it has been silent long enough to transition to the SILENCE 
                    state*/
                    samples_since_speech = cont->read_ts - last_speech_sample;
                    if(samples_since_speech > silence_thresh)
                    {
                        /*Transition to the SILENCE state*/
                        state = SW_STATE_SILENCE;
                        
                        /*Signal the end of an utterance*/
                        ret = ps_end_utt(ps);
                        if(ret < 0)
                        {
                            fprintf(stderr, "ERROR (cmusphinx) sw_decode_speech) "
                                            "ps_end_utt failed\n");
                            ret = -1;
                            goto exit0;
                        }
                    
                        /*Get the hypothesis*/
                        hyp = ps_get_hyp(ps, NULL, &uttid);
                        if(NULL != hyp)
                        {
                            /*Add the newly obtained hypthesis to the total output 
                            hypothesis*/
                            if(NULL == hyp_out)
                            {
                                hyp_out = strdup(hyp);
                                if(NULL == hyp_out)
                                {
                                    fprintf(stderr, "ERROR (cmusphinx) sw_decode_speech) "
                                                    "strdup failed\n");
                                    ret = -1;
                                    goto exit0;
                                }
                            }
                            else
                            {
                                helper = (char*)realloc(hyp_out, 
                                                        (strlen(hyp_out) + strlen(hyp) + 1));
                                if(NULL == helper)
                                {
                                    fprintf(stderr, "ERROR (cmusphinx) sw_decode_speech) "
                                                    "realloc failed to allocate memory "
                                                    "for the hypothesis string\n");
                                    ret = -1;
                                    goto exit0;
                                }
                                else
                                {
                                    hyp_out = helper;
                                    strcat(hyp_out, hyp);
                                }
                            }/*if(NULL == hyp_out) outer*/
                        }/*if(NULL != hyp)*/
                    }/*if(samples_since_speech > silence_thresh)*/
                }/*if(filtered_size > 0)*/
                break;

            default:
                fprintf(stderr, "ERROR (cmusphinx) sw_decode_speech) sw_data object in"
                                "invalid state\n");
                ret = -1;
                goto exit0;
                break;
        }/*switch(state)*/
        
    }/*sw_ad_buffer_size > 0*/
    
    ret = 0;
    
exit0:
    p_sw_data->state = state;
    p_sw_data->last_speech_sample = last_speech_sample;
    *p_hyp_out = hyp_out; 
    sw_ad_buffer = NULL;
    sw_ad_buffer_size = 0;
    
    return ret;
}

int sw_extractlast_hyp( sw_data_t *p_sw_data, 
                        char **p_hyp_out)
{
    int ret = 0;
    
    sw_state_t      state = p_sw_data->state;
    ps_decoder_t    *ps = p_sw_data->ps;

    const char* uttid;
    
    char const*     hyp = NULL;
    char            *hyp_out = NULL;
    
    switch(state)
    {
        case SW_STATE_SILENCE:
            /*Nothing to do*/
            break;
        
        case SW_STATE_SPEECH:
            /*Transition to the SILENCE state*/
            state = SW_STATE_SILENCE;
            
            /*Signal the end of an utterance*/
            ret = ps_end_utt(ps);
            if(ret < 0)
            {
                fprintf(stderr, "ERROR (cmusphinx) sw_extractlast_hyp) "
                                "ps_end_utt failed\n");
                ret = -1;
                goto exit0;
            }
        
            /*Get the hypothesis*/
            hyp = ps_get_hyp(ps, NULL, &uttid);
            if(NULL != hyp)
            {
                hyp_out = strdup(hyp);
                if(NULL == hyp_out)
                {
                    fprintf(stderr, "ERROR (cmusphinx) sw_extractlast_hyp) "
                                    "strdup failed\n");
                    ret = -1;
                    goto exit0;
                }                
            }
            
            break;

        default:
            fprintf(stderr, "ERROR (cmusphinx) sw_extractlast_hyp) "
                            "sw_data object in invalid state\n");
            ret = -1;
            goto exit0;
            break;
            
    }/*switch(state)*/
    
    /*Update the workload state*/
    p_sw_data->state = state;
    
    /*Output the hyp*/
    *p_hyp_out = hyp_out; 

    /*No error has occured*/
    ret = 0;
exit0:
    return ret;
}

#ifdef TEST_SW_WRAPPER

#include <stdio.h>
#include "sw_wav.h"

/*
gcc -Wall -Wmissing-prototypes -o ~/Desktop/sphinxwrapper -D TEST_SW_WRAPPER  sphinxwrapper.c -lpocketsphinx -lsphinxad -lsphinxbase -I /usr/local/include/sphinxbase/ -I /usr/local/include/pocketsphinx/
*/

int main(int argc, char** argv)
{
    int ret = 0;
    
    FILE* filep;
    sw_wavheader_t wavheader;
    int16_t *buffer = NULL;
    size_t ret_size;
    size_t buffer_size = 0;
    size_t total_read = 0;
    size_t total_decoded = 0;
    
    sw_data_t *p_sw_data = NULL;
    
    char *hyp;
    
    /*Check for the correct number of arguments*/
    if(2 != argc)
    {
        fprintf(stderr, "Usage: %s <wav file name>\n", argv[0]);
        ret = -1;
        goto exit0;
    }
    
    /*Assume the first argument is the name of a wav file and open it*/
    filep = fopen(argv[1], "r");
    if(NULL == filep)
    {
        fprintf(stderr, "ERROR: failed to open file \"%s\" for reading\n", argv[1]);
        perror("ERROR: fopen failed in main");
        ret = -1;
        goto exit0;
    }
    
    /*Read in the wav file header*/
    ret_size = fread(&wavheader, sizeof(sw_wavheader_t), 1, filep);
    if(ret_size < 1)
    {
        fprintf(stderr, "ERROR: failed to read the header from the wav file \"%s\"",
                        argv[1]);
        if(ret_size < 0)
        {
            perror("ERROR: fread failed in main");
        }
        ret = -1;
        goto exit1;
    }
    
    /*Print the information in the header*/
    sw_print_wav(stdout, &wavheader);
    
    /*Verify the format of the wav file*/
    ret = sw_verify_wav(&wavheader);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: the wav file, \"%s\" is not properly formatted.\n",
                        argv[1]);
        ret = -1;
        goto exit2;
    }
    
    buffer_size = sw_get_wav_samples(&wavheader);
    buffer = (int16_t*)malloc(buffer_size*sizeof(int16_t));
    if(NULL == buffer)
    {
        fprintf(stderr, "ERROR: Failed to allocate space to read in samples from "
                        "file.\n");
        ret = -1;
        goto exit2;                
    }
    
    ret_size = fread(   buffer, 
                        sizeof(int16_t), 
                        buffer_size, 
                        filep);
    if(ret_size < buffer_size)
    {
        fprintf(stderr, "ERROR: fread failed after reading to read %li samples\n",
                        buffer_size);
        goto exit2;
    }
    /*else*/

    total_read = total_read + ret_size;
    
    /*Initialize the sw_data_t object*/
    ret = allocate_sw_data(&p_sw_data, NULL, (SW_DEFAULT_SAMPLE_RATE/40));
    if(ret < 0)
    {
        fprintf(stderr, "ERROR (cmusphinx) main): allocate_sw_data failed\n");
        goto exit2;
    }

    /*Callibrate the silence filter*/
    ret = sw_calib_silence(p_sw_data, buffer, total_read);
    if(ret == 1)
    {
        fprintf(stderr, "ERROR (cmusphinx) main) The amount of data available int not "
                        "sufficient for sw_calib_silence\n");
        goto exit3;
    }
    else if (ret < 0)
    {
        fprintf(stderr, "ERROR (cmusphinx) main) sw_calib_silence failed\n");
        goto exit3;
    }

    printf( "Total length of audio = %li:%f\n",
            (total_read/(16000*60)), 
            (float)(total_read%(16000*60))/(float)(16000));

    for(total_decoded = 0; total_decoded < total_read; total_decoded += (16000/20))
    {
        ret = sw_decode_speech(p_sw_data, &(buffer[total_decoded]), (16000/20), &hyp);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR (sphinx main) main) sw_decode_speech failed\n");
            goto exit3;
        }

        if(NULL != hyp)
        {
            printf("\r %li) \t%s \n", total_decoded, hyp);
            free(hyp);
            hyp = NULL;
        }
        else
        {
            printf("\r %li) ", total_decoded);
        }
    }
    
    /*Extract the last hypothesis*/  
    ret = sw_extractlast_hyp(p_sw_data,&hyp);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR (sphinx main) main) sw_extractlast_hyp failed\n");
        goto exit3;
    }
    
    /*Print the last hypohtesis*/
    if(NULL != hyp)
    {
        printf("\r %li) \t%s \n", total_decoded, hyp);
        free(hyp);
        hyp = NULL;
    }

exit3:
    free_sw_data(&p_sw_data);
exit2:
    if(NULL != buffer)
    {
        free(buffer);
    }
exit1:
    fclose(filep);
exit0:
    return ret;    
}

#endif

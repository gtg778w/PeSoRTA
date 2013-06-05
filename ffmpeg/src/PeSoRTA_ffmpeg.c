#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "PeSoRTA.h"
#include "PeSoRTA_helper.h"

#include "ffmpegwrapper.h"

typedef enum
{
    PeSoRTA_FFMPEG_DECODE = 0,
    PeSoRTA_FFMPEG_ENCODE
} PeSoRTA_ffmpeg_type_t;

typedef struct PeSoRTA_ffmpeg_s
{
    PeSoRTA_ffmpeg_type_t coder_type;
    enum AVMediaType      media_type;
    char    *file_name;
    
    /*Union has to be named for C99 compliance*/
    union
    {
        fw_decoder_t    decoder;
        fw_encoder_t    encoder;
    } coder;
    
    union
    {
        AVFrame     frame;
        AVPacket    packet;
    } output;
    
    fw_eparams_t params;
    
} PeSoRTA_ffmpeg_t;

/*
    - a simple function that returns the name and any description of the workload as a static string
*/
char *workload_name(void)
{
    return "ffmpeg";
}

/*
"-I: input file name (file name)\n"\
"-M: media type (decoder only)  \n"\
"\n the following parameters are encoder specific:\n"\
"-C: codec name (codec name)    \n"\
"-b: bit rate (bits per second) \n"\
"-m: motion estimation method   \n"\
"-w: width                      \n"\
"-h: height                     \n"\
"-g: gop size                   \n"\
"-B: maximum number of B frames \n"\
"-f: sample/pixel format        \n"\
"-c: channel layout             \n"\
"-s: sample rate                \n"\
*/
static int PeSoRTA_ffmpeg_parse_config(char *configfile_name, PeSoRTA_ffmpeg_t *workload_state)
{
    int ret = 0;
    FILE *configfile_p;

	/*parsing variables*/
    char *optstring = "I:C:M:b:m:w:h:g:B:f:c:s:";
    int  opt;
    char *optarg;
    
    /*define some options flags*/
    int I_flag = 0;
    int C_flag = 0;
    int M_flag = 0;    
    char *media_type_s = NULL;
    int b_flag = 0;
    int m_flag = 0;
    /*int w_flag = 0;*/
    /*int h_flag = 0;*/
    /*int g_flag = 0;*/
    /*int B_flag = 0;*/
    int f_flag = 0;
    /*int s_flag = 0;*/

    /*Open the config file*/
    configfile_p = fopen(configfile_name, "r");
    if(NULL == configfile_p)
    {
        fprintf(stderr, "ERROR: PeSoRTA_ffmpeg_parse_config) fopen failed\n");
        ret = -1;
        goto exit0;
    }

    /*Workload_state should already be zeroed out*/

    /*Initialize the encoder parameters*/
    DEFALUT_EPARAMS(&(workload_state->params));

    /*Parse the file, line by line*/
    while(!feof(configfile_p))
    {
        ret = PeSoRTA_getconfigopt(configfile_p, optstring, &opt, &optarg);
        if(ret == -1)
        {
            if(!feof(configfile_p))
            {
                fprintf(stderr, "ERROR: PeSoRTA_ffmpeg_parse_config) "
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
                fprintf(stderr, "ERROR: PeSoRTA_ffmpeg_parse_config) config file "
                                "contains bad line:\n\t\"%s\"\n", optarg);                
                ret = -1;
                free(optarg);
                goto exit1;
        }

        if(optarg == NULL)
        {
            fprintf(stderr, "ERROR: PeSoRTA_ffmpeg_parse_config) config file contains "
                            "valid option (%c) without argument!\n", (int)opt);
            ret = -1;
            goto exit1;
        }
        
        switch(opt)
        {
            case 'I':
                I_flag = 1;
                workload_state->file_name = optarg;
                break;

            case 'C':
                C_flag = 1;
                workload_state->params.codec_name = optarg;
                break;

            case 'M':
                M_flag = 1;
                media_type_s = optarg;
                break;

            case 'b':
                b_flag = 1;
                workload_state->params.bit_rate = (int)strtoul(optarg, NULL, 0);
                free(optarg);
                break;

            case 'm':
                m_flag = 1;
                workload_state->params.me_method_s = optarg;
                break;

            case 'w':
                workload_state->params.width = (int)strtoul(optarg, NULL, 0);
                free(optarg);
                break;
        
            case 'h':
                workload_state->params.height = (int)strtoul(optarg, NULL, 0);
                free(optarg);
                break;
            
            case 'g':
                workload_state->params.gop_size = (int)strtoul(optarg, NULL, 0);
                free(optarg);
                break;

            case 'B':
                workload_state->params.max_b_frames = (int)strtoul(optarg, NULL, 0);
                free(optarg);
                break;
                
            case 'f':
                f_flag = 1;
                workload_state->params.format = optarg;
                break;
                
            case 's':
                workload_state->params.sample_rate = (int)strtoul(optarg, NULL, 0);
                free(optarg);
                break;
        }/*switch(opt)*/
    }/*while(!feof(configfile_p))*/

    /*The I flag must be specified for both types of coders*/
    if(0 == I_flag)
    {
        fprintf(stderr, "ERROR (ffmpeg) PeSoRTA_ffmpeg_parse_config) : Options file must "
                        "specify input media file. \n");
        ret = -1;
        goto exit2;
    }    
    
    /*Either the C flag or the M flag must be specified*/
    if(0 == C_flag)
    {
        if(0 == M_flag)
        {
            fprintf(stderr, "ERROR (ffmpeg) PeSoRTA_ffmpeg_parse_config) : Options file "
                            "must specify the M option for the decoder or the C option "
                            "for the encoder. \n");
            ret = -1;
            goto exit2;
        }
        else
        {
            /*If the M flag is specified, this is a decoding workload, and a valid 
            media type must be specified*/
            workload_state->coder_type = PeSoRTA_FFMPEG_DECODE;
            
            if(0 == strcmp(media_type_s, "audio"))
            {
                workload_state->media_type = AVMEDIA_TYPE_AUDIO;
            }
            else if(0 == strcmp(media_type_s, "video"))
            {
                workload_state->media_type = AVMEDIA_TYPE_VIDEO;
            }
            else
            {
                fprintf(stderr, "ERROR (ffmpeg) PeSoRTA_ffmpeg_parse_config) : Invalid "
                                "argument for the -M option: must be \"audio\" or "
                                "\"video\".\n");
                ret = -1;
                goto exit2;
            }
        }
    }
    else
    {
        /*If the C flag is specified, this is an encoding workload*/
        workload_state->coder_type = PeSoRTA_FFMPEG_ENCODE;

        /*The bit-rate flag must be specified for the encoder*/
        if(0 == b_flag)
        {
            fprintf(stderr, "ERROR (ffmpeg) PeSoRTA_ffmpeg_parse_config) : Options file must "
                            "specify a bit rate. \n");
            ret = -1;
            goto exit2;
        }
     
        /*prevent the codec name from being freed*/
        C_flag = 0;
        
        /*prevent the motion estimation method from being freed*/
        m_flag = 0;
    }

    /*prevent the pixel or sample format from being freed*/    
    f_flag = 0;

    /*prevent the input file name from being freed*/
    I_flag = 0;

exit2:
    if( 1 == I_flag){ free(workload_state->file_name); }
    if( 1 == C_flag){ free(workload_state->params.codec_name); }
    if( 1 == M_flag){ free(media_type_s); }
    if( 1 == m_flag){ free(workload_state->params.me_method_s); }
    if( 1 == f_flag){ free(workload_state->params.format); }
    
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
    
    PeSoRTA_ffmpeg_t *workload_state;
    
    fw_init();
    
    /*allocate space for the workload state*/
    workload_state = calloc(1, sizeof(PeSoRTA_ffmpeg_t));
    if(NULL == workload_state)
    {
        fprintf(stderr, "ERROR: (ffmpeg) workload_init) malloc failed to allocate space "
                        "for a PeSoRTA_ffmpeg_t object.\n");
        ret = -1;
        goto error0;
    }
    
    /*read the config file*/
    ret = PeSoRTA_ffmpeg_parse_config(configfile, workload_state);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: (ffmpeg) workload_init) PeSoRTA_ffmpeg_parse_config "
                        "failed\n");
        goto error1;
    }
    
    /*setup the coder*/
    if(PeSoRTA_FFMPEG_DECODE == workload_state->coder_type)
    {
        ret = fw_init_decoder(  workload_state->file_name,
                                &(workload_state->coder.decoder),
                                workload_state->media_type, 
                                1/*batched_read*/);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: (ffmpeg) workload_init) fw_init_decoder failed\n");
            goto error2;
        }
        
        /*Initialize the output frame*/
        avcodec_get_frame_defaults(&(workload_state->output.frame));

    }
    else /* (PeSoRTA_FFMPEG_ENCODE == workload_state->coder_type) */
    {
        ret = fw_init_encoder(  workload_state->file_name,
                                &(workload_state->coder.encoder),
                                &(workload_state->params) );
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: (ffmpeg) workload_init) fw_init_encoder failed\n");
            goto error2;
        }
        
        /*Initialize the output packet*/
        av_init_packet(&(workload_state->output.packet));
        /* packet data will be allocated by the encoder*/
        workload_state->output.packet.data = NULL;
        workload_state->output.packet.size = 0;
    }
    
    /*set return values*/
    *state_p = workload_state;

    /*set the job count to the number of packets for the decoder and the number of frames
    for the encoder*/
    if(PeSoRTA_FFMPEG_DECODE == workload_state->coder_type)
    {
        *job_count_p = workload_state->coder.decoder.packets_read;
    }
    else /* (PeSoRTA_FFMPEG_ENCODE == workload_state->coder_type) */
    {
        *job_count_p = workload_state->coder.encoder.frames_available;
    }
    
    return 0;
    
error2:
    /*free the options strings*/
    if(NULL != workload_state->file_name)
    {   
        free(workload_state->file_name);
    }
    if(NULL != workload_state->params.codec_name)
    {  
        free(workload_state->params.codec_name);
    }
    if(NULL != workload_state->params.me_method_s)
    {
        free(workload_state->params.me_method_s);
    }    
    if(NULL != workload_state->params.format)
    {
        free(workload_state->params.format);
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
    int got_frame;
    int consumed_frame;
    int got_packet;

    fw_decoder_t    *pDec;
    AVFrame         *pFrame;
    fw_encoder_t    *pEnc;
    AVPacket        *pPkt;
    
    PeSoRTA_ffmpeg_t *workload_state = (PeSoRTA_ffmpeg_t*)state;
    if(NULL == workload_state)
    {
        ret = -1;
        goto exit0;
    }    
        
    if(PeSoRTA_FFMPEG_DECODE == workload_state->coder_type)
    {
        pDec    = &(workload_state->coder.decoder);
        pFrame  = &(workload_state->output.frame);
        ret = fw_decode_nxtpkt( pDec, 
                                pFrame,
                                &got_frame);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: (ffmpeg) perform_job) fw_decode_nxtpkt failed\n");
            goto exit0;
        }
        
        /*Return 1 if there are no more frames*/
        ret = !(got_frame);
    }
    else  /* (PeSoRTA_FFMPEG_ENCODE == workload_state->coder_type) */
    {
        pEnc  = &(workload_state->coder.encoder);
        pPkt  = &(workload_state->output.packet);

        do
        {
            /*Get the next encoded packet*/
            ret = fw_encode_step(   pEnc,
                                    &consumed_frame,
                                    pPkt,
                                    &got_packet);
            if(ret < 0)
            {
                fprintf(stderr, "ERROR: (ffmpeg) perform_job) fw_encode_step failed\n");
                
                goto exit0;
            }
            
            /*If a packet is produced, free the data and reinitialize the packet*/
            if(0 != got_packet)
            {
                av_free_packet(pPkt);
                av_init_packet(pPkt);
                pPkt->data = NULL; // packet data will be allocated by the encoder
                pPkt->size = 0;
            }

        }while( (0 == consumed_frame) && (0 == pEnc->nomore_packets));
        
        /*Return 1 if there are no more packets*/
        ret = !(!(pEnc->nomore_packets));
    }
    
exit0:
    return ret;
}

int workload_uninit(void *state)
{
    PeSoRTA_ffmpeg_t *workload_state = (PeSoRTA_ffmpeg_t*)state;
    
    if(NULL == workload_state)
    {
        goto exit0;
    }

    /*uninit the coder*/
    if(PeSoRTA_FFMPEG_DECODE == workload_state->coder_type)
    {
        /*free the output frame data*/
        fw_free_copied_data(workload_state->media_type, 
                            &(workload_state->output.frame));
        /*free the decoder*/
        fw_free_decoder(&(workload_state->coder.decoder));
    }
    else  /* (PeSoRTA_FFMPEG_ENCODE == workload_state->coder_type) */
    {
        /*free the otuput packet*/
        av_free_packet(&(workload_state->output.packet));
        /*free the encoder*/
        fw_free_encoder(&(workload_state->coder.encoder));
    }
    
    /*free the options strings*/
    if(NULL != workload_state->file_name)
    {   
        free(workload_state->file_name);
    }
    if(NULL != workload_state->params.codec_name)
    {  
        free(workload_state->params.codec_name);
    }
    if(NULL != workload_state->params.me_method_s)
    {
        free(workload_state->params.me_method_s);
    }    
    if(NULL != workload_state->params.format)
    {
        free(workload_state->params.format);
    }

    /*free the main workload_state data structure*/
    free(workload_state);
exit0:
    return 0;
}


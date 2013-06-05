#include <stdio.h>
#include "ffmpegwrapper.h"

void fw_print_encoder_list(FILE* fstream)
{
    int i;
    AVCodec *pCodec;

    fprintf(fstream, "****************************************"
            "****************************************\n");
    fprintf(fstream, "Supported list of encoders:\n");
    fprintf(fstream, "****************************************"
            "****************************************\n");

    for(pCodec = av_codec_next(NULL), i = 0; 
        NULL != pCodec; 
        i++, pCodec = av_codec_next(pCodec))
    {
        printf( "\n%i) %s, %s ", i, pCodec->name,
                av_get_media_type_string(pCodec->type));
        
        if(av_codec_is_encoder(pCodec))
        {
            fprintf(fstream, "encoder ");
        }
        
        if(av_codec_is_decoder(pCodec))
        {
            fprintf( fstream, "decoder ");
        }
        
        fprintf( fstream, "\n");

        /*switch(pCodec->type)
        {
            case AVMEDIA_TYPE_VIDEO:
                break;
                
            case AVMEDIA_TYPE_AUDIO:
                break;
            default:
                break;
        }*/
    }

    fprintf(fstream, "****************************************"
            "****************************************\n");
    fprintf(fstream, "****************************************"
            "****************************************\n");
}

int fw_init_preproc(AVCodecContext  *pCodecCtx_src,
                    AVCodec         *pCodec_dst,
                    AVCodecContext  *pCodecCtx_dst,
                    fw_preproc_state_t    *pPreproc,
                    fw_eparams_t    *pEparams)
{
    int ret;
    
    void*   preproc_state;

    pPreproc->media_type= pCodec_dst->type;
    
    switch(pPreproc->media_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            ret = fw_audio_init_preproc(pCodecCtx_src,
                                        pCodec_dst,
                                        pCodecCtx_dst,
                                        &preproc_state,
                                        pEparams);
            if(ret < 0)
            {
                fprintf(stderr, "ERROR: fw_audio_init_preproc failed in "
                                "fw_init_preproc\n");
                goto error0;
            }
            else
            {
                pPreproc->preproc_state = preproc_state;
                pPreproc->preproc       = fw_audio_preproc;
                pPreproc->end_preproc   = fw_audio_end_preproc;
                pPreproc->alloc_preproc_frame   = fw_audio_alloc_preproc_frame;
                pPreproc->free_preproc_frame    = fw_audio_free_preproc_frame;
            }
            
            break;
            
        case AVMEDIA_TYPE_VIDEO:
            ret = fw_video_init_preproc(pCodecCtx_src,
                                        pCodec_dst,
                                        pCodecCtx_dst,
                                        &preproc_state,
                                        pEparams);
            if(ret < 0)
            {
                fprintf(stderr, "ERROR: fw_video_init_preproc failed in "
                                "fw_init_preproc\n");
                goto error0;
            }
            else
            {
                pPreproc->preproc_state = preproc_state;
                pPreproc->preproc       = fw_video_preproc;
                pPreproc->end_preproc   = fw_video_end_preproc;
                pPreproc->alloc_preproc_frame   = fw_video_alloc_preproc_frame;
                pPreproc->free_preproc_frame    = fw_video_free_preproc_frame;
            }
            
            break;
            
        default:
            fprintf(stderr, "ERROR: Unsupported media type. Can not initialize "
                            "the preprocessor in fw_init_preproc\n");
            goto error0;
    }
    
    return 0;
error0:
    pPreproc->preproc_state = NULL;
    return -1;
}

void fw_free_preproc(fw_preproc_state_t    *pPreproc)
{
    switch(pPreproc->media_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            fw_audio_free_preproc(&(pPreproc->preproc_state));
            pPreproc->preproc_state = NULL;
            break;
        case AVMEDIA_TYPE_VIDEO:
            fw_video_free_preproc(&(pPreproc->preproc_state));
            pPreproc->preproc_state = NULL;
            break;
        default:
            fprintf(stderr, "WARNING: Unsupported media type found in fw_preproc_state_t "
                            "object in fw_free_preproc. The preproc_state field will not "
                            "be freed. Memory leak may result.\n");
    }
    
    pPreproc->preproc       = NULL;
    pPreproc->end_preproc   = NULL;
    pPreproc->alloc_preproc_frame   = NULL;
    pPreproc->free_preproc_frame    = NULL;
}

int fw_init_encoder(  char            *input_filename,
                      fw_encoder_t    *pEnc,
                      fw_eparams_t    *pParams)
{
    int ret;
    
    char  *codec_name;    
    enum AVMediaType    media_type;
    
    AVCodec         *pCodec_dst;

    fw_decoder_t    decoder;

    AVCodecContext  *pCodecCtx_src;

    AVFrame         **pFrameArray = NULL;
    void            *pVoid = NULL;
    uint64_t        frames_available;
    uint64_t        frm_i;
    int             got_frame;

    AVCodecContext  *pCodecCtx_dst;

    fw_preproc_state_t *pPreproc = &(pEnc->preproc);
    
    AVFrame         *pFramePreenc = NULL;

    fw_encode_t     encode = NULL;

    /*Extract the encoding parameters*/
    codec_name = pParams->codec_name;
        
    /*Find the codec.*/
    pCodec_dst = avcodec_find_encoder_by_name(codec_name);
    if(NULL == pCodec_dst) 
    {
        fprintf(stderr, "ERROR: avcodec_find_encoder_by_name failed to find codec of "
                        "of name \"%s\" in fw_init_encoder\n", codec_name);
        goto error0;
    }

    /*Determine the media type*/
    media_type = pCodec_dst->type;

    /*Given the media type, setup the encoder function pointers*/
    switch(media_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            encode = avcodec_encode_audio2;
            break;

        case AVMEDIA_TYPE_VIDEO:
            encode = avcodec_encode_video2;
            break;

        default:
            fprintf(stderr, "ERROR fw_init_encoder: The media type of the AVCodec object "
                            "for \"%s\" is currently supported by fw_init_encoder.\n",
                            codec_name);
            goto error0;
    }

    /*Try to open the input file and setup the decoder*/
    memset(&decoder, 0, sizeof(fw_decoder_t));
    ret = fw_init_decoder(  input_filename, 
                            &decoder,
                            media_type,
                            FW_NO_BATCHED_READ);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: fw_init_decoder failed in fw_init_encoder\n");
        goto error0;
    }
    pCodecCtx_src = decoder.pCodecCtx;

    /*Read in all the decoded frames*/
    for(got_frame = 1, frames_available = 0, frm_i = 0; got_frame != 0; )
    {
        if(frames_available <= frm_i)
        {
            frames_available += 128;
            
            /*Allocate enough space for frame pointers for all the frames*/
            pVoid = realloc(pFrameArray, frames_available*sizeof(AVFrame*));
            if(NULL == pVoid)
            {
                fprintf(stderr, "ERROR: realloc failed to allocate space for the "
                                "array of AVFrame pointers in fw_init_encoder\n");
                frames_available -= 128;
                goto error2;
            }
            
            pFrameArray = (AVFrame**)pVoid;                
            memset(&(pFrameArray[frm_i]), 0, 
                (sizeof(AVFrame*) * (frames_available-frm_i)));
        }
    
        if(NULL == pFrameArray[frm_i])
        {
            pFrameArray[frm_i] = avcodec_alloc_frame();
            if(NULL == pFrameArray[frm_i])
            {
                fprintf(stderr, "ERROR: avcodec_alloc_frame failed to allocate memory "
                                "for a new AVFrame object in fw_init_encoder\n");
                goto error2;
            }
        }
    
        ret = fw_decode_nxtpkt( &decoder, 
                                pFrameArray[frm_i],
                                &got_frame);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: fw_decode_nxtpkt failed in fw_init_encoder\n");
            goto error2;
        }
        
        if(got_frame != 0)
        {           
            frm_i++;
        }
    }

    frames_available = frm_i;
    if(NULL != pFrameArray[frm_i])
    {
        fw_free_decoded_data(&decoder, pFrameArray[frm_i]);
        avcodec_free_frame(&(pFrameArray[frm_i]));
    }


    /*Setup the encoder*/
    
    /*Allocate a codec context*/
    pCodecCtx_dst = avcodec_alloc_context3(pCodec_dst);
    if(NULL == pCodecCtx_dst)
    {
        fprintf(stderr, "ERROR: avcodec_alloc_context3 failed to allocate memory"
                        "for a codec context in fw_init_encoder\n");
        goto error2;
    }
    
    /*Setup the preprocessor and parameters for the codec*/
    ret = fw_init_preproc(pCodecCtx_src,
                          pCodec_dst,
                          pCodecCtx_dst,
                          pPreproc,
                          pParams);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: fw_init_preproc failed in fw_init_encoder\n");
        goto error3;
    }
        
    /*Allocate the AVFrame object to be used for the encoder*/
    ret = pPreproc->alloc_preproc_frame(&pFramePreenc,
                                        pPreproc->preproc_state);
    if (ret < 0)
    {
        fprintf(stderr, "ERROR: alloc_preproc_frame failed in fw_init_encoder\n");
        goto error4;
    }

    /*Nothing left to do with the decoder. Free it.*/
    fw_free_decoder(&decoder);
    
    /*Assign the new values to the encoder structure*/
    pEnc->pCodec    = pCodec_dst;
	pEnc->pCodecCtx = pCodecCtx_dst;

    pEnc->pFrameArray       = pFrameArray;
    pEnc->frames_available  = frames_available;
    
    /* The following is implicitly true*/
    /* pEnc->pPreproc          = *pPreproc */;
    
    pEnc->frame_preprocing  = 0;
    pEnc->frames_preproced  = 0;
    pEnc->nomore_eframes    = 0;
    pEnc->pFramePreenc      = pFramePreenc;
    pEnc->encode            = encode;
    pEnc->nomore_packets    = 0;

    return 0;
    
    /*Error-related undo operations*/    
error4:
    fw_free_preproc(pPreproc);
error3:
    avcodec_close(pCodecCtx_dst);
    av_free(pCodecCtx_dst);
error2:
    for(frm_i = 0; frm_i < frames_available; frm_i++)
    {
        if(NULL != pFrameArray[frm_i])
        {
            fw_free_copied_data(media_type, pFrameArray[frm_i]);
            avcodec_free_frame(&(pFrameArray[frm_i]));
        }
        else
        {
            break;
        }
    }
    
    if(NULL != pFrameArray)
    {
        free(pFrameArray);
    }
/*error1:*/
    fw_free_decoder(&decoder);
error0:
    
    pEnc->pCodec    = NULL;
	pEnc->pCodecCtx = NULL;

    pEnc->pFrameArray       = NULL;
    pEnc->frames_available  = 0;
    
    pEnc->frame_preprocing  = 0;
    pEnc->frames_preproced  = 0;
    pEnc->nomore_eframes    = 0;
    pEnc->pFramePreenc      = NULL;
    pEnc->encode            = NULL;
    pEnc->nomore_packets    = 0;
    
    return -1;
}

int fw_encode_step( fw_encoder_t *pEnc,
                    int          *frame_consumed,
                    AVPacket     *pPacket,
                    int          *packet_produced)
{
    int ret = 0;

    AVCodecContext      *pCodecCtx      = pEnc->pCodecCtx;

    AVFrame         	**pFrameArray   = pEnc->pFrameArray;
    uint64_t            frames_available= pEnc->frames_available;
    fw_preproc_state_t  *pPreproc       = &(pEnc->preproc);
    int                 frame_preprocing= pEnc->frame_preprocing;
    uint64_t            frames_preproced= pEnc->frames_preproced;
    int                 nomore_eframes  = pEnc->nomore_eframes;
    AVFrame         	*pFramePreenc   = pEnc->pFramePreenc;

    /*This is not done explicitly to prevent confusion*/
    /*fw_encode_t         encode          = pEnc->encode;*/
    int                 nomore_packets  = pEnc->nomore_packets;
    
    /*Assume no frames are consumed and no packets are produced.*/
    int consumed_src_frame = 0;
    *frame_consumed = 0;
    int got_preproced_frame = 0;
    *packet_produced = 0;

    /*Check if there are any packets to encode*/
    if(nomore_packets != 0)
    {
        goto exit0;
    }
    
    /*Check if there are additional frames to preprocess*/
    if( (frames_preproced < frames_available) || (frame_preprocing != 0))
    {
        /*Start or continue preprocessing the relevant frame*/
        ret = pPreproc->preproc(pPreproc->preproc_state,
                                pFrameArray[frames_preproced],
                                &consumed_src_frame,
                                pFramePreenc,
                                &got_preproced_frame);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: the preproc function of the fw_preproc_state_t "
                            "object failed in fw_encode_step.\n");
            goto exit0;
        }
        
        /*Check if the frame was consumed*/
        if(consumed_src_frame)
        {
            /*Notify the caller that the frame was consumed*/
            *frame_consumed = 1;
            
            /*Increment the index of the current frame*/
            frames_preproced++;
            /*Reset the frame_preprocing flag*/
            frame_preprocing = 0;
        }
    }
    else /*(frames_preproced >= frames_available) && (frame_preprocing == 0)*/
    {
        /*There are no more frames remaining to preprocess*/
        
        /*Check if there is anything left to extract from the preprocessor*/
        if(nomore_eframes == 0)
        {
            /*Try to extract any remaining data from the preprocessor*/
            pPreproc->end_preproc(  pPreproc->preproc_state,
                                    pFramePreenc,
                                    &got_preproced_frame);
            /*Test if any residual data was extracted*/   
            if(0 == got_preproced_frame)
            {
                /*There was no residual data to extract*/
                nomore_eframes = 1;
            }
        }
    }
    
    /*Chcek if there is a frame to encode*/
    if(0 != got_preproced_frame)
    {
        ret = pEnc->encode( pCodecCtx, 
                            pPacket, 
                            pFramePreenc, 
                            packet_produced);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: tne encode function of the fw_encoder_t object "
                            "failed to encode the next packet in fw_encode_step\n");
            goto exit0;
        }
    }
    else /*(0 == got_preproced_frame)*/
    {
        /*Don't have a new frame to encode.*/
        
        /*Check if there are any more frames to encode at all.*/
        if(nomore_eframes == 1)
        {
            /*Don't have any additional frames to encode*/
            
            /*Extract any residual packets*/
            ret = pEnc->encode( pCodecCtx, 
                                pPacket, 
                                NULL, 
                                packet_produced);
            if(ret < 0)
            {
                fprintf(stderr, "ERROR: tne encode function of the fw_encoder_t object "
                                "failed to encode the next packet in fw_encode_step\n");
                goto exit0;
            }
            
            /*Check if any residual packets were extracted*/
            if(*packet_produced == 0)
            {
                /*There are no more packets left*/
                nomore_packets = 1;
            }
        }
        /*else There may be additional frames to encode*/
    }

    pEnc->frame_preprocing= frame_preprocing;
    pEnc->frames_preproced= frames_preproced;
    pEnc->nomore_eframes  = nomore_eframes;
    pEnc->pFramePreenc    = pFramePreenc;
    pEnc->nomore_packets  = nomore_packets;

exit0:
    return ret;
}

void fw_free_encoder(fw_encoder_t    *pEnc)
{
    uint64_t    frm_i;
    
    pEnc->preproc.free_preproc_frame(pEnc->pFramePreenc);
    fw_free_preproc(&(pEnc->preproc));

    /*it is possible that avcodec_close is called twice against the codec context for 
    the encoder.*/
    avcodec_close(pEnc->pCodecCtx);
    av_free(pEnc->pCodecCtx);

    for(frm_i = 0; frm_i < pEnc->frames_available; frm_i++)
    {
        if(NULL != (pEnc->pFrameArray[frm_i]))
        {
            fw_free_copied_data(pEnc->preproc.media_type, pEnc->pFrameArray[frm_i]);
            avcodec_free_frame(&(pEnc->pFrameArray[frm_i]));
        }
        else
        {
            break;
        }
    }
    free(pEnc->pFrameArray);
    
    pEnc->pCodec    = NULL;
	pEnc->pCodecCtx = NULL;

    pEnc->pFrameArray       = NULL;
    pEnc->frames_available  = 0;	

    pEnc->frame_preprocing  = 0;
    pEnc->frames_preproced  = 0;
    pEnc->nomore_eframes    = 0;
    pEnc->pFramePreenc      = NULL;
    pEnc->encode            = NULL;
    pEnc->nomore_packets    = 0;
}

#ifdef TEST_FW_ENCODER
#include <stdlib.h>
#include <errno.h>

/*
gcc -Wall -O2 -D TEST_FW_ENCODER -o ./fw_encoder_test ./fw_decoder.c ./fw_encoder.c ./fw_audio.c ./fw_video.c -lavformat -lswresample -lswscale -lavcodec -lpostproc -lavfilter -lavutil -lgsm -lmp3lame -lopencore-amrnb -lopus -lspeex -lvorbis -lvorbisenc -lvpx -lx264  -lz -lm
*/

char * usage_string = " <source file name> < encoder name > <bit rate>";

int main(int argc, char** argv)
{
    int ret = 0;

    char *filename;
    char *codec_name;
    long  bit_rate_long;
    
    fw_eparams_t eparams;
    fw_encoder_t encoder;
    
    AVPacket    packet;
    int     frame_encoded = 0;
    int     packet_produced = 0;
    
    int packet_count;
    int frame_count;
    
    fw_init();

    /*zero out the encoder*/
    memset(&encoder, 0, sizeof(fw_encoder_t));

    if(argc == 1)
    {
        fw_print_encoder_list(stdout);
        ret = 0;
        goto exit0;
    }

    /*parse options*/
    if(argc != 4)
    {
        fprintf(stderr, "ERROR: Usage: %s %s\n", argv[0], usage_string);
        ret = -1;
        goto exit0;
    }

    filename = argv[1];
    codec_name = argv[2];
    
    errno = 0;
    bit_rate_long = strtol(argv[3], NULL, 0);
    if(errno != 0)
    {
        fprintf(stderr, "ERROR: Failed to parse argument 3\n");
        perror("ERROR: strtol failed in main");
        ret = -1;
        goto exit0;
    }
    
    /*Set the encoding parameters*/
    DEFALUT_EPARAMS(&eparams);
    eparams.codec_name = codec_name;
    eparams.bit_rate = (int)bit_rate_long;
    eparams.me_method_s = "zero";
    
    /*Initialize the encoder*/
    ret = fw_init_encoder(  filename,
                            &encoder,
                            &eparams);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: fw_init_encoder failed in main\n");
        ret = -1;
        goto exit0;
    }

    packet_count = 0;
    frame_count = 0;

    /*Initialize the packet to encode*/
    av_init_packet(&packet);
    packet.data = NULL; // packet data will be allocated by the encoder
    packet.size = 0;

    do
    {
        /*Get the next encoded packet*/
        ret = fw_encode_step(   &encoder,
                                &frame_encoded,
                                &packet,
                                &packet_produced);
        if(ret < 0)
        {
            fprintf(stderr, "ERROR: fw_encode_step failed in main\n");
            ret = -1;
            goto exit1;
        }
        
        if(packet_produced != 0)
        {
            packet_count++;
            av_free_packet(&packet);
            av_init_packet(&packet);
            packet.data = NULL; // packet data will be allocated by the encoder
            packet.size = 0;
        }
        
        if(frame_encoded)
        {
            frame_count++;
        }
        
        printf("\r frames encoded = %i, (%lu/%lu), packets produced = %i, flags = 0x%x", 
                frame_count, encoder.frames_preproced, encoder.frames_available,
                packet_count, 
                (   ((encoder.frame_preprocing) << 8) | 
                    ((encoder.nomore_eframes) << 4) |
                    ((encoder.nomore_packets) << 0)   ) );
                    
    }while(0 == encoder.nomore_packets);

    /*Free the packet*/
    av_free_packet(&packet);
    printf("\n\n");
    
exit1:
    fw_free_encoder(&encoder);    
exit0:
    return ret;
}

#endif


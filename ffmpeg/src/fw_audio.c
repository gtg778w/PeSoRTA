#include <stdio.h>
#include "ffmpegwrapper.h"

/*
    For non-matching parameters,
        pick the desired parameter
            
    For macthing parameters, disregard the desired parameter
        pick the parameter from the source
        if the source parameter is not an allowable parameter for the codec
            throw an error
            
    non-matching parameters
        bit_rate -> any value is allowable
        
    matching parameters
        sample_fmt
        channel_layout
        sample_rate
        frame_size
*/
int fw_audio_init_preproc(   AVCodecContext     *pCodecCtx_src,
                             AVCodec            *pCodec_dst,
                             AVCodecContext     *pCodecCtx_dst,
                             void               **p_preproc_state,
                             fw_eparams_t       *eparams)
{
    int ret;
    int i;

    /*These variables will be copied into the preproc_state*/
    uint64_t            channel_layout_src;
    enum AVSampleFormat smpl_fmt_src;
    int                 smpl_rate_src;
    
    uint64_t            channel_layout_dst;
    enum AVSampleFormat smpl_fmt_dst;
    int                 smpl_rate_dst;

    /*These variables are for computing optimal encoder parameters*/
    enum AVSampleFormat const *allowed_fmts;

    uint64_t    const *allowed_layouts;
    uint64_t    max_channels;
    uint64_t    max_channels_layout;
    uint64_t    current_channels;

    int         const *supported_samplerates;
    int         max_sample_rate;
    
    /*The following parameters are for the resampler and resampled buffer*/
    struct SwrContext   *pSwrCtx;
    
    uint8_t     **swrdata;
    int         swrframe_isplanar;
    int         swrframe_planes;
    int         buffersize_swrdata;
    
    int         framesize_dst;
    
    /*cast the preprocessor_state to the an fw_audio_preproc_t*/
    fw_audio_preproc_t  *fw_audio_preproc_p = malloc(sizeof(fw_audio_preproc_t));
    if(NULL == fw_audio_preproc_p)
    {
        fprintf(stderr, "ERROR: malloc failed to allocate memory for the "
                        "fw_audio_preproc_t structure in fw_audio_init_preproc");
        goto error0;
    }

    
    /*check if the source and destination sample_fmt is compatible*/
    smpl_fmt_src = pCodecCtx_src->sample_fmt;
    if(NULL != eparams->format)
    {
        smpl_fmt_dst = av_get_sample_fmt(eparams->format);
        if(AV_SAMPLE_FMT_NONE == smpl_fmt_dst)
        {
            fprintf(stderr, "WARNING fw_audio_init_preproc: the user-specified sample "
                            "format \"%s\" was not recognized. Defaulting to "
                            "AV_SAMPLE_FMT_NONE.\n", eparams->format);            
        }
    }
    else
    {
        smpl_fmt_dst = smpl_fmt_src;
        /*check if there is a set of allowed destination formats*/
        allowed_fmts = pCodec_dst->sample_fmts;
        if(NULL != allowed_fmts)
        {
            /*loop through the allowed destination formats and try to find a match*/
            for(i = 0; smpl_fmt_src != allowed_fmts[i]; i++)
            {
                /*if the loop reaches the end of the array
                  and the desired AVSampleFormat has not
                  been found, throw a warning*/
                if( -1 == allowed_fmts[i])
                {
                    smpl_fmt_dst = allowed_fmts[0];
                    fprintf(stderr, "WARNING: the destination codec does not support the "
                                    "source sample format. Defaulting to %s in "
                                    "fw_audio_preproc\n", 
                                    av_get_sample_fmt_name(smpl_fmt_dst));
                    break;
                }
            }
        }
    }
    
    
    channel_layout_src = pCodecCtx_src->channel_layout;
    /*check if the source channel_layout is unset*/
    if(0 == channel_layout_src)
    {
        /*then set it to the default based on the number of channels*/
        channel_layout_src = 
            av_get_default_channel_layout(pCodecCtx_src->channels);
        pCodecCtx_src->channel_layout = channel_layout_src;
    }

    /*check if the source and destination channel_layout is compatible*/
    channel_layout_dst = channel_layout_src;
    allowed_layouts = pCodec_dst->channel_layouts;
    /*check if there is a set of allowed destination channel layouts*/
    if(NULL != allowed_layouts)
    {
        /*loop through the allowed destination formats and try to find a match.
        Also, try to find an allowed channel layout with at most the same
        number of channels as the source layout*/
        max_channels_layout = allowed_layouts[0];
        max_channels = av_get_channel_layout_nb_channels(max_channels_layout);
        for(i = 0; channel_layout_src != allowed_layouts[i]; i++)
        {
            current_channels = av_get_channel_layout_nb_channels(allowed_layouts[i]);
            if((max_channels < current_channels) && 
                (current_channels <= pCodecCtx_src->channels))
            {
                max_channels = current_channels;
                max_channels_layout = allowed_layouts[i];
            }
            /*if the loop reaches the end of the array
              and the desired parameter value has not
              been found, throw a warning*/
            if( 0 == allowed_layouts[i])
            {
                /*no more channel layout left to test for the destination codec*/
                channel_layout_dst = max_channels_layout;
                fprintf(stderr, "WARNING: the destination codec does not support the "
                                "source channel layout. Defaulting to first supported "
                                "layout with %li channels in fw_audio_preproc\n",
                                max_channels);
                break;
            }
        }            
    }
    
    /*check if the source and destination sample_rate is compatible*/
    smpl_rate_src = pCodecCtx_src->sample_rate;
    if(0 != eparams->sample_rate)
    {
        smpl_rate_dst = eparams->sample_rate;
    }
    else
    {
        smpl_rate_dst = smpl_rate_src;
        supported_samplerates = pCodec_dst->supported_samplerates;
        /*check if there is a set of allowed sample rates*/
        if(NULL != supported_samplerates)
        {
            /*loop through the allowed destination sample rates and try to find a match.
            Also, try to find the maximum allowed sample rate*/
            max_sample_rate = supported_samplerates[0];
            for(i = 0; smpl_rate_src != supported_samplerates[i]; i++)
            {
                if(max_sample_rate < supported_samplerates[i])
                {
                    max_sample_rate = supported_samplerates[i];
                }
                
                /*if the loop reaches the end of the array
                  and the desired parameter value has not
                  been found, pick the maximum rate*/
                if( 0 == supported_samplerates[i])
                {
                    smpl_rate_dst = max_sample_rate;
                    fprintf(stderr, "WARNING: the destination codec does not support the"
                                    "source sample_rate. Defaulting to the maximum"
                                    "sample rate %i in fw_audio_preproc\n", smpl_rate_dst);
                    break;
                }
            }
        }
    }
    
    /*Set the properties of the encoding codec context*/
    pCodecCtx_dst->sample_fmt = smpl_fmt_dst;
    pCodecCtx_dst->channel_layout = channel_layout_dst;
    pCodecCtx_dst->channels = av_get_channel_layout_nb_channels(channel_layout_dst);
    pCodecCtx_dst->sample_rate = smpl_rate_dst;
    
    /*Set properties based on the passed encoder parameters*/
    pCodecCtx_dst->bit_rate = eparams->bit_rate;
    
    /*Open the codec with the given parameters*/
    if(avcodec_open2(pCodecCtx_dst, pCodec_dst, NULL) < 0)
    {
        fprintf(stderr, "ERROR: failed to open the destination codec in "                                
                                "fw_audio_preproc\n");
        goto error0;
    }
    
    /*Allocate a resampler context*/
    pSwrCtx = swr_alloc_set_opts(   NULL,
                                    (pCodecCtx_dst->channel_layout),
		                            (pCodecCtx_dst->sample_fmt),
		                            (pCodecCtx_dst->sample_rate),
		                            (pCodecCtx_src->channel_layout),
		                            (pCodecCtx_src->sample_fmt),
		                            (pCodecCtx_src->sample_rate),
		                            0,
                                    NULL);
    if(NULL == pSwrCtx)
    {
        fprintf(stderr, "ERROR: swr_alloc_set_opts failed to allocate memory for the "
                        "resampler context or set the associated options in"
                        "fw_audio_preproc\n");
        goto error1;
    }

    /*Initialize the resampling context */
    ret = swr_init(pSwrCtx);
    if(ret < 0) 
    {
        fprintf(stderr, "ERROR: swr_init failed in fw_audio_preproc\n");
        goto error2;
    }
    
    /*compute the number of planes in the resampled data*/
    swrframe_isplanar = av_sample_fmt_is_planar(pCodecCtx_dst->sample_fmt);
    swrframe_planes =  (swrframe_isplanar != 0)? pCodecCtx_dst->channels : 1;

    /*allocate the pointer to the different planes of the resampled data*/
    swrdata = (uint8_t**)av_malloc(sizeof(uint8_t*) * swrframe_planes);
    if(NULL == swrdata)
    {
        fprintf(stderr, "ERROR: av_malloc failed to allocate memory for swrdata "
                        "in fw_audio_preproc\n");
        goto error2;
    }

    /*assume the number of samples in the resampled buffer is equal to the encoder
    frame size*/
    framesize_dst = pCodecCtx_dst->frame_size;
    if(0 == framesize_dst)
    {
        /*If the encoder doesn't have a frame size, just set it to 1024*/
        framesize_dst = 1024;
    }
    buffersize_swrdata = framesize_dst;
    
    /*allocate the planes in the resampled data*/
    /*allocate enough memory for the resampled data to handle the largest src frame*/
    ret = av_samples_alloc( swrdata,
		                    NULL, /*don't need line size*/
		                    (pCodecCtx_dst->channels),
		                    buffersize_swrdata,
		                    (pCodecCtx_dst->sample_fmt),
                            0);
    if(ret < 0)
    {
        fprintf(stderr, "ERROR: av_samples_alloc failed to allocate memory for "
                        "%i samples for the swrdata planes in fw_audio_preproc\n", 
                        buffersize_swrdata);
        goto error3;
    }
    
    fw_audio_preproc_p->channel_layout_src  = channel_layout_src;
    fw_audio_preproc_p->smpl_fmt_src        = smpl_fmt_src;
    fw_audio_preproc_p->smpl_rate_src       = smpl_rate_src; 

    fw_audio_preproc_p->channel_layout_dst  = channel_layout_dst;
    fw_audio_preproc_p->smpl_fmt_dst        = smpl_fmt_dst;
    fw_audio_preproc_p->smpl_rate_dst       = smpl_rate_dst;
    
    fw_audio_preproc_p->pSwrCtx         = pSwrCtx;
    fw_audio_preproc_p->swrdata         = swrdata;
    fw_audio_preproc_p->buffersize_swrdata  = buffersize_swrdata;
    fw_audio_preproc_p->framesize_swrdata   = 0;
    fw_audio_preproc_p->issilent_swrdata    = 0;
    fw_audio_preproc_p->offset_swrdata  = 0;
    fw_audio_preproc_p->framesize_dst   = framesize_dst;
    fw_audio_preproc_p->offset_dst      = 0;

    *p_preproc_state = (void*)fw_audio_preproc_p; 
    return 0;

error3:
    av_freep(&swrdata);
error2:
    swr_free(&pSwrCtx);
error1:
    avcodec_close(pCodecCtx_dst);
error0:
    if(NULL != fw_audio_preproc_p)
    {
        free(fw_audio_preproc_p);
    }
    *p_preproc_state = NULL;
    return -1;
}

int fw_audio_alloc_preproc_frame(AVFrame **ppFrame_output,
                                 void    *preproc_state)
{
    int ret;
    
    AVFrame *pFrame_output;
    
    int         frame_buffer_size;
    uint16_t    *frame_buffer;
    
    fw_audio_preproc_t *preproc_p = (fw_audio_preproc_t*)preproc_state;
    
    int channels_dst;
    
    /*Allocate the AVFrame object to be used for the encoder*/
    pFrame_output = avcodec_alloc_frame();
    if(pFrame_output == NULL) 
    {
        fprintf(stderr, "ERROR: avcodec_alloc_frame failed "
                        "in fw_audio_alloc_preproc_frame\n");
        goto error0;
    }
    
    /*Fill in some of the fields*/
    pFrame_output->nb_samples = preproc_p->framesize_dst;
    pFrame_output->format = preproc_p->smpl_fmt_dst;
    pFrame_output->channel_layout = preproc_p->channel_layout_dst;
    
    /*Calculate the number of channels in the channel layout*/
    channels_dst = av_get_channel_layout_nb_channels(preproc_p->channel_layout_dst);
    /* calculate the size of the frame buffer in bytes */
    frame_buffer_size = av_samples_get_buffer_size( NULL,
                                                    channels_dst, 
                                                    preproc_p->framesize_dst,
                                                    preproc_p->smpl_fmt_dst, 
                                                    0);
    
    /* allocate memory for the frame buffer*/
    frame_buffer = av_malloc(frame_buffer_size);
    if (NULL == frame_buffer) 
    {
        fprintf(stderr, "ERROR: av_malloc failed for frame_buffer in "
                        "fw_audio_alloc_preproc_frame\n");
        goto error1;
    }

    /* setup the data pointers in the AVFrame */
    ret = avcodec_fill_audio_frame( pFrame_output, 
                                    channels_dst,
                                    preproc_p->smpl_fmt_dst,
                                    (const uint8_t*)frame_buffer, 
                                    frame_buffer_size,
                                    0);
    if (ret < 0) 
    {
        fprintf(stderr, "ERROR: avcodec_fill_audio_frame failed in "
                        "fw_audio_alloc_preproc_frame\n");
        goto error2;
    }

    *ppFrame_output = pFrame_output;

    return 0;

error2:
    av_freep(&frame_buffer);
error1:
    avcodec_free_frame(&pFrame_output);
error0:
    return -1;
}

void fw_audio_free_preproc_frame(AVFrame *pFrame)
{
    av_freep(&(pFrame->data[0]));
    avcodec_free_frame(&pFrame);
}

/*
    It is assumed that pAVFrame_dst was allocated using the function
    fw_audio_alloc_preproc_frame
*/
int fw_audio_preproc(   void    *preproc_state,
                        AVFrame *pAVFrame_src,
                        int     *frame_consumed,
                        AVFrame *pAVFrame_dst,
                        int     *frame_produced)
{
    int ret = 0;

    /*Extract the contents of the fw_audio_preproc_t data structure*/
    fw_audio_preproc_t  *preproc_p = (fw_audio_preproc_t*)preproc_state;

    int                 smpl_rate_src       = preproc_p->smpl_rate_src;
    
    enum AVSampleFormat smpl_fmt_dst        = preproc_p->smpl_fmt_dst;
    int                 smpl_rate_dst       = preproc_p->smpl_rate_dst;
    int                 channels_dst;

    struct SwrContext   *pSwrCtx        = preproc_p->pSwrCtx;
    uint8_t             **swrdata       = preproc_p->swrdata;
    int                 buffersize_swrdata  = preproc_p->buffersize_swrdata;
    int                 framesize_swrdata   = preproc_p->framesize_swrdata;
    int                 offset_swrdata  = preproc_p->offset_swrdata;
    int                 issilent_swrdata    = preproc_p->issilent_swrdata;
    int                 framesize_dst   = preproc_p->framesize_dst;
    int                 offset_dst      = preproc_p->offset_dst;

    int framesize_src;
    
    int data_available;
    int space_available;
    int copy_amount;

    /*Begin by assuming that no frame is consumed or produced*/
    *frame_consumed = 0;
    *frame_produced = 0;

    /*Check if a valid swrdata buffer is present*/
    if((NULL == swrdata) || (NULL == swrdata[0]))
    {
        fprintf(stderr, "ERROR: the preproc_state passed as the first argument does "
                        "not contain a pointer to a valid resampled buffer "
                        "in fw_audio_preproc\n");
        ret = -1;
        goto exit0;
    }

    /*Determine the number of destination channels*/
    channels_dst = av_get_channel_layout_nb_channels(preproc_p->channel_layout_dst);
    
    /*Check if a new frame has to be resampled*/
    if(0 == framesize_swrdata)
    {
        /*Check if there is a source frame available*/
        if(NULL != pAVFrame_src)
        {
            /*Get the number of samples in the frame*/
            framesize_src = pAVFrame_src->nb_samples;

            /*Compute the number of samples in the resampled frame*/
            framesize_swrdata = av_rescale_rnd( framesize_src, 
                                                smpl_rate_dst, 
                                                smpl_rate_src, 
                                                AV_ROUND_UP);

            /*Make sure that the memory available for the resampled data is sufficient*/
            if(buffersize_swrdata < framesize_swrdata)
            {
                /*Otherwise, increase the resampled buffer size*/
                buffersize_swrdata = framesize_swrdata;

                /*Free the previous data buffer*/
                av_freep(&(swrdata[0]));

                /*Allocate the new buffer*/            
                ret = av_samples_alloc( swrdata,
		                                NULL, /*don't need line size*/
		                                channels_dst,
		                                buffersize_swrdata,
		                                smpl_fmt_dst,
                                        0);
                if(ret < 0)
                {
                    fprintf(stderr, "ERROR: av_samples_alloc failed to allocate memory "
                                    "(%i samples) for the swrdata planes in "
                                    "fw_audio_preproc\n", buffersize_swrdata);
                    framesize_swrdata = 0;
                    buffersize_swrdata = 0;
                    ret = -1;
                    goto exit1;
                }
            }

            /*Resample a new frame.*/
            ret = swr_convert(  pSwrCtx, 
                                swrdata, 
                                framesize_swrdata,
                                /*casting from "uint8_t **" to "const uint8_t **" */ 
                                (const uint8_t **)pAVFrame_src->extended_data, 
                                framesize_src);
            if (ret < 0) 
            {
                fprintf(stderr, "ERROR: swr_convert failed in fw_audio_preproc\n");
                framesize_swrdata = 0;
                ret = -1;
                goto exit1;
            }

            /*Indicate that the source is not silent*/
            issilent_swrdata = 0;
            
            /*Update the resample buffer offset*/
            offset_swrdata = 0;
        }
        else /*(NULL == pAVFrame_src)*/
        {
            /*Don't have a new source frame to resample and copy into the buffer.*/

            /*Fill the resample buffer with silence instead.*/
            ret = av_samples_set_silence(   swrdata,
                                            0, /*offset*/
                                            buffersize_swrdata,
                                            channels_dst,
                                            smpl_fmt_dst);
            if(ret < 0)
            {
                fprintf(stderr, "ERROR: av_samples_set_silence failed in "
                                "fw_audio_preproc\n");
                ret = -1;
                goto exit1;
            }

            /*Update the resample buffer framesize*/
            framesize_swrdata = 0;
            
            /*Update the resample buffer offset*/
            offset_swrdata = 0;
            
            /*indicate that the source is silent*/
            issilent_swrdata = 1;
        }
    }

    /*Copute how many samples can be copied from the resample buffer
    into the data buffer of the output frame.*/
    data_available = framesize_swrdata - offset_swrdata;
    space_available = framesize_dst - offset_dst;
    copy_amount = (space_available < data_available)? 
                    space_available : data_available;

    /*Copy the samples from the resample buffer into 
    the data buffer of the output frame*/
    av_samples_copy( pAVFrame_dst->extended_data,
                     swrdata,
                     offset_dst,
                     offset_swrdata,
                     copy_amount,
                     channels_dst,
                     smpl_fmt_dst);     

    /*Update the offset values*/
    offset_swrdata += copy_amount;
    offset_dst += copy_amount;

    /*Check if the resample buffer is empty*/
    if(offset_swrdata == framesize_swrdata)
    {
        /*Only set the frame_consumed flag if
        a non-silent source frame was resampled*/
        if(0 == issilent_swrdata)
        {
            /*The source frame is consumed*/
            *frame_consumed = 1;
            /*There is no more data left in the
            resampled buffer*/
            framesize_swrdata = 0;
        }
        
        offset_swrdata = 0;
    }

    /*Check if the buffer in the output frame is full*/
    if(offset_dst == framesize_dst)
    {
        /*The output frame is ready*/
        *frame_produced = 1;
        /*Reset the offset for the next output frame*/
        offset_dst = 0;
    }

exit1:
    /*Restore the content of the fw_audio_preproc_t object*/
    preproc_p->swrdata          = swrdata;
    preproc_p->buffersize_swrdata   = buffersize_swrdata;
    preproc_p->framesize_swrdata    = framesize_swrdata;
    preproc_p->offset_swrdata   = offset_swrdata;
    preproc_p->issilent_swrdata = issilent_swrdata;
    preproc_p->framesize_dst    = framesize_dst;
    preproc_p->offset_dst       = offset_dst;
        
exit0:
    return ret;
}

/*
    This function should only be called when the frame_consumed flag is set
    for the last frame in the input stream
*/
void fw_audio_end_preproc(  void    *preproc_state,
                            AVFrame *pAVFrame_dst,
                            int     *frame_produced)
{
    fw_audio_preproc_t *preproc_p = (fw_audio_preproc_t*)preproc_state;

    pAVFrame_dst->nb_samples = preproc_p->offset_dst;
    preproc_p->offset_dst = 0;            
    
    if(pAVFrame_dst->nb_samples > 0)
    {
        *frame_produced = 1;
    }
    else
    {
        *frame_produced = 0;
    }
}
                        
void fw_audio_free_preproc(void **p_preproc_state)
{
    fw_audio_preproc_t  *fw_audio_preproc_p = (fw_audio_preproc_t*)(*p_preproc_state);
    
    if(NULL != fw_audio_preproc_p->swrdata[0])
    {
        av_freep(&(fw_audio_preproc_p->swrdata[0]));
    }
    
    av_freep(&(fw_audio_preproc_p->swrdata));
    swr_free(&(fw_audio_preproc_p->pSwrCtx));

    free(fw_audio_preproc_p);
    *p_preproc_state = NULL;
}

int fw_audio_frame_copy(AVFrame *pFrame_src, AVFrame * pFrame_dst)
{
    int ret;

    int allocate_buffer;    
    uint8_t *buffer_frame;
    int     frame_buffer_size;
    

    if(NULL != (pFrame_dst->data[0]))
    {
        allocate_buffer = 0;
        
        allocate_buffer = (pFrame_dst->nb_samples != pFrame_src->nb_samples);
        
        allocate_buffer = allocate_buffer || 
                            (pFrame_dst->format != pFrame_src->format);
        
        allocate_buffer = allocate_buffer || 
                            (pFrame_dst->channel_layout != pFrame_src->channel_layout);

        if(0 != allocate_buffer)
        {
            av_freep(&(pFrame_dst->data[0]));
        }
    }
                    
    /*Assign relevant properties to the new frame*/
    pFrame_dst->nb_samples     = pFrame_src->nb_samples;
    pFrame_dst->format         = pFrame_src->format;
    pFrame_dst->sample_rate    = pFrame_src->sample_rate;
    pFrame_dst->channel_layout = pFrame_src->channel_layout;
    pFrame_dst->channels       = pFrame_src->channels;
    
    if(NULL == (pFrame_dst->data[0]))
    {
        /* Calculate the size of the frame buffer in bytes */
        frame_buffer_size = av_samples_get_buffer_size( NULL,
                                                        pFrame_dst->channels, 
                                                        pFrame_dst->nb_samples,
                                                        pFrame_dst->format,
                                                        0);

        if(frame_buffer_size > 0)
        {
            /*Allocate space for the buffer*/
            buffer_frame = av_malloc(frame_buffer_size);
            if (NULL == buffer_frame) 
            {
                fprintf(stderr, "ERROR: av_malloc failed to allocate memory "
                                "for the frame buffer in fw_audio_frame_copy\n");
                goto error0;
            }

            /*Setup the data pointers in the AVFrame */
            ret = avcodec_fill_audio_frame( pFrame_dst, 
                                            pFrame_dst->channels,
                                            pFrame_dst->format,
                                            buffer_frame, 
                                            frame_buffer_size, 
                                            0);
            if (ret < 0) 
            {
                fprintf(stderr, "ERROR: avcodec_fill_audio_frame failed in "
                                "fw_audio_frame_copy\n");
                goto error1;
            }
        }
    }
    
    if(pFrame_dst->nb_samples > 0)
    {
        /*Copy the samples from the source frame into the copied frame*/
        av_samples_copy(pFrame_dst->extended_data,
                        pFrame_src->extended_data,
                        0,
                        0,
                        pFrame_dst->nb_samples,
                        pFrame_dst->channels,
                        pFrame_dst->format);
    }
    
    return 0;

error1:
    av_freep(&buffer_frame);
error0:
    return -1;
}

void fw_audio_free_copied_data(AVFrame *pFrame)
{
    if(pFrame->nb_samples > 0)
    {
        av_freep(&(pFrame->data[0]));
    }
}


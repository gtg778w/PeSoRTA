#include <stdio.h>
#include <string.h>
#include "ffmpegwrapper.h"

enum Motion_Est_ID fw_video_get_me_method(char * method_name)
{
    enum Motion_Est_ID me_method = 0;
    
    if(0 == strcasecmp(method_name, "zero"))
    {
        me_method = ME_ZERO;
    }
    else if(0 == strcasecmp(method_name, "full"))
    {
        me_method = ME_FULL;
    }
    else if(0 == strcasecmp(method_name, "log"))
    {
        me_method = ME_LOG;
    }
    else if(0 == strcasecmp(method_name, "phods"))
    {
        me_method = ME_PHODS;
    }
    else if(0 == strcasecmp(method_name, "epzs"))
    {
        me_method = ME_EPZS;
    }
    else if(0 == strcasecmp(method_name, "x1"))
    {
        me_method = ME_X1;
    }
    else if(0 == strcasecmp(method_name, "hex"))
    {
        me_method = ME_HEX;
    }
    else if(0 == strcasecmp(method_name, "umh"))
    {
        me_method = ME_UMH;
    }
    else if(0 == strcasecmp(method_name, "tesa"))
    {
        me_method = ME_TESA;
    }
    
    return me_method;
}

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
int fw_video_init_preproc(   AVCodecContext     *pCodecCtx_src,
                             AVCodec            *pCodec_dst,
                             AVCodecContext     *pCodecCtx_dst,
                             void               **p_preproc_state,
                             fw_eparams_t       *eparams)
{
    int ret;
    
    int i;
    
    AVRational timebase_src = pCodecCtx_src->time_base;
    AVRational framerate_src;
    const AVRational *supported_framerates = pCodec_dst->supported_framerates;
    AVRational timebase_dst;
    
    enum AVPixelFormat pixfmt_src = pCodecCtx_src->pix_fmt;
    const enum AVPixelFormat *supported_pixfmts = pCodec_dst->pix_fmts;
    enum AVPixelFormat pixfmt_dst;
    
    struct SwsContext   *pSwsCtx = NULL;
    
    fw_video_preproc_t  *preproc_p = NULL;
    
    timebase_dst = timebase_src;
    framerate_src.num = timebase_src.den;
    framerate_src.den = timebase_src.num;
    /*Check if the destination codec supports the source frame rate*/
    if(NULL != supported_framerates)
    {
        for(i = 0; 
            0 != av_cmp_q(framerate_src, supported_framerates[i]);
            i++)
	    {
	        if( (0 == supported_framerates[i].num) &&
	            (0 == supported_framerates[i].den))
            {
                fprintf(stderr, "ERROR fw_video_init_preproc: the destination codec does not "
                                "support the source frame rate (%i/%i)fps.",
                                framerate_src.num, framerate_src.den);
                goto error0;
            }
	    }
    }
    
    /*Check if the destination codec supports the source pixel format*/
    pixfmt_dst = pixfmt_src;
    if(NULL != eparams->format)
    {
        pixfmt_dst = av_get_pix_fmt(eparams->format);
        if(PIX_FMT_NONE == pixfmt_dst)
        {
            fprintf(stderr, "WARNING fw_video_init_preproc: the user-specified pixel "
                            "format \"%s\" was not recognized. Defaulting to "
                            "PIX_FMT_NONE.\n", eparams->format);
        }
    }
    else
    {
        if(NULL != supported_pixfmts)
        {
            for(i = 0; 
                (supported_pixfmts[i] != pixfmt_src);
                i++)
            {
                pixfmt_dst = supported_pixfmts[0];
                if(-1 == supported_pixfmts[i])
                {
                    fprintf(stderr, "WARNING fw_video_init_preproc: the destination codec "
                                    "does not support the source pixel format.\n");
                    break;
                }
            }
        }
    }
    
    /*Set the encoder parameters to be the same as the source parameters*/
    pCodecCtx_dst->time_base= timebase_dst;
    pCodecCtx_dst->pix_fmt = pixfmt_dst;
    
    pCodecCtx_dst->width =  (0 != eparams->width)?  
                            eparams->width : pCodecCtx_src->width;
    pCodecCtx_dst->height = (0 != eparams->height)?  
                            eparams->height : pCodecCtx_src->height;
    
    /*Set user_specified or default gop and B-frame settings */
    if(eparams->gop_size >= 0)
    {
        pCodecCtx_dst->gop_size = eparams->gop_size;
    }
    else
    {
        pCodecCtx_dst->gop_size = 10;
    }
    
    if(eparams->max_b_frames > 0)
    {
        pCodecCtx_dst->max_b_frames = eparams->max_b_frames;
    }
    else
    {
        pCodecCtx_dst->max_b_frames=0;
    }
    
    /*Set the remaining parameters according to user specification*/
    pCodecCtx_dst->bit_rate = eparams->bit_rate;
    
    /*Set the user-specified or default motion estimation method*/
    if(NULL != eparams->me_method_s)
    {
        pCodecCtx_dst->me_method = fw_video_get_me_method(eparams->me_method_s);
        if(0 == pCodecCtx_dst->me_method)
        {
            fprintf(stderr, "WARNING fw_video_init_preproc: the motion estimation method "
                            "\"%s\" was not recognized. Defaulting to ME_ZERO.\n", 
                            eparams->me_method_s);
        }
        pCodecCtx_dst->me_method = ME_ZERO;
    }
    else
    {
        pCodecCtx_dst->me_method = ME_ZERO;
    }
    
    /*An additional option related to AV_CODEC_ID_H264*/
    if(AV_CODEC_ID_H264 == pCodec_dst->id)
    {
        av_opt_set(pCodecCtx_dst->priv_data, "preset", "slow", 0);
    }
    
    /*Open the codec context*/
    ret = avcodec_open2(pCodecCtx_dst, pCodec_dst, NULL);
    if (ret < 0) 
    {
        fprintf(stderr, "ERROR: avcodec_open2 failed in fw_video_init_preproc\n");
        goto error0;
    }
    
    /*Allocate the rescaler*/
    pSwsCtx = sws_getContext(   pCodecCtx_src->width, pCodecCtx_src->height, pixfmt_src,
                                pCodecCtx_dst->width, pCodecCtx_dst->height, pixfmt_dst,
                                SWS_BILINEAR, NULL, NULL, NULL);
    if(NULL == pSwsCtx)
    {
        fprintf(stderr, "ERROR: sws_getContext failed in fw_video_init_preproc\n");
        goto error0;
    }
    
    /*Allocate space for the preprocessor*/
    preproc_p = (fw_video_preproc_t*)malloc(sizeof(fw_video_preproc_t));
    if(NULL == preproc_p)
    {
        fprintf(stderr, "ERROR: malloc faile to allocate memory for an"
                        "fw_video_preproc_t object in fw_video_init_preproc\n");
        goto error1;
    }
    
    /*Fill in the fw_video_preproc_t object*/
    preproc_p->pix_fmt_src  = pixfmt_src;
    preproc_p->pix_fmt_dst  = pixfmt_dst;
    preproc_p->width        = pCodecCtx_dst->width;
    preproc_p->height       = pCodecCtx_dst->height;
    preproc_p->pSwsCtx      = pSwsCtx;
    
    /*Set the p_preproc_state output variable*/
    *p_preproc_state = preproc_p;
    return 0;
    
error1:
    sws_freeContext(pSwsCtx);
error0:
    *p_preproc_state = NULL;
    return -1;
}

int fw_video_alloc_preproc_frame(AVFrame **ppFrame_output,
                                 void    *preproc_state)
{
    int ret;

    fw_video_preproc_t  *preproc_p = (fw_video_preproc_t*)preproc_state;
    
    AVFrame *pFrame_output;
    
    /*Allocate the AVFrame object to be used for the encoder*/
    pFrame_output = avcodec_alloc_frame();
    if(pFrame_output == NULL) 
    {
        fprintf(stderr, "ERROR: avcodec_alloc_frame failed in "
                        "fw_video_alloc_preproc_frame\n");
        goto error0;
    }

    /*Fill in some of the fields*/
    pFrame_output->format = preproc_p->pix_fmt_dst;
    pFrame_output->width  = preproc_p->width;
    pFrame_output->height = preproc_p->height;

    /*Allocate the image buffer*/
    ret = av_image_alloc(   pFrame_output->data, 
                            pFrame_output->linesize, 
                            preproc_p->width, 
                            preproc_p->height,
                            preproc_p->pix_fmt_dst, 
                            32);
    if (ret < 0) 
    {
        fprintf(stderr, "ERROR: av_image_alloc failed to allocate an image buffer in "
                        "fw_video_alloc_preproc_frame\n");
        goto error1;
    }
    
    *ppFrame_output = pFrame_output;

    return 0;
error1:
    avcodec_free_frame(&pFrame_output);    
error0:
    return -1;
}

void fw_video_free_preproc_frame(AVFrame *pFrame)
{
    av_freep(&(pFrame->data[0]));
    avcodec_free_frame(&pFrame);
}

/*
    It is assumed that pAVFrame_dst was allocated using the function
    fw_video_alloc_preproc_frame
*/
int fw_video_preproc(   void    *preproc_state,
                        AVFrame *pFrame_src,
                        int     *frame_consumed,
                        AVFrame *pFrame_dst,
                        int     *frame_produced)
{
    int ret = 0;
    
    fw_video_preproc_t  *preproc_p = (fw_video_preproc_t*)preproc_state;
    
    int dst_frame_valid = 1;
    
    *frame_consumed = 0;
    *frame_produced = 0;
    
    /*Check the destination frame parameters*/
    dst_frame_valid = dst_frame_valid && 
                        (pFrame_dst->format == preproc_p->pix_fmt_dst);
    
    dst_frame_valid = dst_frame_valid && 
                    (pFrame_dst->width == preproc_p->width);

    dst_frame_valid = dst_frame_valid && 
                    (pFrame_dst->height == preproc_p->height);
    
    if(0 == dst_frame_valid)
    {
        fprintf(stderr, "ERROR fw_video_preproc: the destination frame parameters are "
                        "not valid. Allocate the destination frame of the preprocessor "
                        "with fw_video_alloc_preproc_frame\n");
        ret = -1;
        goto exit0;
    }

    sws_scale( 	preproc_p->pSwsCtx,
		        (const uint8_t * const*)pFrame_src->data,
		        pFrame_src->linesize,
		        0,
		        pFrame_src->height,
		        pFrame_dst->data,
		        pFrame_dst->linesize);
    
    /*Increment the PTS value*/
    if((0 == pFrame_src->pts) || (AV_NOPTS_VALUE == pFrame_src->pts))
    {
        (pFrame_dst->pts)++;
    }
    else
    { 
        pFrame_dst->pts = pFrame_src->pts;
    }
    
    /*For the video preprocessor, unless you exit with an error, the src frame is always
    consumed and an output frame is always produced*/
    *frame_consumed = 1;
    *frame_produced = 1;
exit0:
    return ret;
}

/*
    This function should only be called when the frame_consumed flag is set
    for the last frame in the input stream
*/
void fw_video_end_preproc(  void    *preproc_state,
                            AVFrame *pAVFrame_dst,
                            int     *frame_produced)
{
    /*This type of function is only relevant for audio and not video*/
    /*The video preprocessor encodes frame to frame, and is therefore stateless
    and has no residual output after processing the last input frame*/
    *frame_produced = 0;
}
                        
void fw_video_free_preproc(void **p_preproc_state)
{
    fw_video_preproc_t  *preproc_p = (fw_video_preproc_t*)(*p_preproc_state);
    
    /*Free the image rescaler*/
    sws_freeContext(preproc_p->pSwsCtx);
    
    /*Free the fw_video_preproc_t object*/
    free(preproc_p);
    
    /*Set the preprocessor state to NULL*/
    *p_preproc_state = NULL;
}

int fw_video_frame_copy(AVFrame *pFrame_src, AVFrame * pFrame_dst)
{
    int ret;
    int allocate_buffer;
    
    if(NULL != (pFrame_dst->data[0]))
    {
        allocate_buffer = 0;
        
        allocate_buffer = allocate_buffer ||
                            (pFrame_dst->format != pFrame_src->format);
        
        allocate_buffer = allocate_buffer ||
                            (pFrame_dst->width != pFrame_src->width);
        
        allocate_buffer = allocate_buffer ||
                            (pFrame_dst->height != pFrame_src->height);
        
        if(0 != allocate_buffer)
        {
            av_freep(&(pFrame_dst->data[0]));
        }
    }

    /*Assign relevant properties to the new frame*/
    pFrame_dst->format = pFrame_src->format;
    pFrame_dst->width  = pFrame_src->width;
    pFrame_dst->height = pFrame_src->height;
    pFrame_dst->pts    = pFrame_src->pts;

    if((pFrame_dst->width > 0) && (pFrame_dst->height) > 0)
    {
        if(NULL == pFrame_dst->data[0])
        {
            /*Allocate an image buffer for the new frame*/
            ret = av_image_alloc(   pFrame_dst->data, 
                                    pFrame_dst->linesize, 
                                    pFrame_dst->width, 
                                    pFrame_dst->height, 
                                    pFrame_dst->format, 
                                    32);
            if (ret < 0)
            {
                fprintf(stderr, "ERROR: avcodec_alloc_frame failed to allocate memory for the "
                                "image buffer in the copied frame in fw_video_frame_copy\n");
                goto error0;
            }
        }

        av_image_copy(  pFrame_dst->data, 
                        pFrame_dst->linesize, 
                        (const uint8_t**)pFrame_src->data,
                        pFrame_src->linesize,
                        pFrame_src->format, pFrame_src->width, pFrame_src->height);
    }
    
    return 0;

error0:
    return -1;
}

void fw_video_free_copied_data(AVFrame *pFrame)
{
    if((pFrame->width > 0) && (pFrame->height) > 0)
    {
        av_freep(&(pFrame->data[0]));
    }
}


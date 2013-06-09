#include <stdio.h>
#include "ffmpegwrapper.h"

int av_registered = 0;

int fw_frame_copy(  enum AVMediaType    media_type,
                    AVFrame             *pFrame_src,
                    AVFrame             *pFrame_dst)
{
    int ret;
    
    switch(media_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            ret = fw_audio_frame_copy(pFrame_src, pFrame_dst);
            break;

        case AVMEDIA_TYPE_VIDEO:
            ret = fw_video_frame_copy(pFrame_src, pFrame_dst);
            break;
            
        default:
            fprintf(stderr, "ERROR fw_frame_copy: The media type of the AVCodec object "
                            " of the source AVCodecContext object is not currently "
                            " supported by fw_frame_copy.\n");
            ret = -1;
    }
    
    return ret;
}

void fw_free_copied_data(   enum AVMediaType    media_type,
                            AVFrame             *pFrame)
{
    
    switch(media_type)
    {
        case AVMEDIA_TYPE_AUDIO:
            fw_audio_free_copied_data(pFrame);
            break;

        case AVMEDIA_TYPE_VIDEO:
            fw_video_free_copied_data(pFrame);
            break;
            
        default:
            fprintf(stderr, "ERROR fw_free_copied_data: The media type of the "
                            "AVCodec object of the source AVCodecContext object is not "
                            "currently supported by fw_free_copied_frame.\n");
    }
}

int fw_init_decoder(char                *filename, 
                    fw_decoder_t        *pDec, 
                    enum AVMediaType    media_type, 
                    unsigned char       batched_read)
{
    AVFormatContext *pFormatCtx;

    int             strm_desired;
    AVStream        *pStream;
    fw_decode_t     decode;

    AVCodecContext  *pCodecCtx;

    AVCodec         *pCodec;

    AVPacket        *pPackets;
    void            *p_dummy;
    uint64_t        packet_space;
    uint64_t        pkt_i;

    /* open the file */
    pFormatCtx = NULL;
    if(avformat_open_input(&pFormatCtx, filename, NULL, NULL) !=0)
    {
        fprintf(stderr, "ERROR: avformat_open_input failed in fw_init_decoder\n");
        goto error0;
    }

    /* Retrieve stream information */
    if(avformat_find_stream_info(pFormatCtx, NULL)<0)
    {
        fprintf(stderr, "ERROR: avformat_find_stream_info failed in fw_init_decoder\n");
        goto error1;
    }    

    /* Search for the first stream with the desired media type*/
    strm_desired = av_find_best_stream(pFormatCtx, media_type, -1, -1, NULL, 0);
    if (strm_desired < 0) 
    {
        /* Didn't find a video stream */
        fprintf(stderr, "ERROR: av_find_best_stream failed to find a stream "
                        "of the desired media type (%s) in fw_init_decoder\n",
                        av_get_media_type_string(media_type));
        goto error1;
    }
    else
    {
        pStream = pFormatCtx->streams[strm_desired];
        pCodecCtx = pStream->codec;
    }
    
    /*determine the best decoding function*/
    switch(media_type)
    {
        case AVMEDIA_TYPE_VIDEO:
            decode = avcodec_decode_video2;
            break;
        case AVMEDIA_TYPE_AUDIO:
            decode = avcodec_decode_audio4;
            break;
        default:
            decode = NULL;
            fprintf(stderr, "ERROR: no decoder function known for the media_type passed "
                            "to fw_init_decoder\n");
            goto error1;
    }
    
    /* Setup the codec */
    /* Find the decoder for the video stream */
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(NULL == pCodec)
    {
        fprintf(stderr, "ERROR: failed to find an appropriate codec in "
                        "fw_init_decoder\n");
        goto error1; // Codec not found
    }    

    /* Open the codec */
    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0)
    {
        fprintf(stderr, "ERROR: avcodec_open failed in fw_init_decoder\n");
        goto error1; // Could not open codec
    }

    /* Hack to correct wrong frame rates that seem to be generated by some codecs */
    if(pCodecCtx->time_base.num>1000 && pCodecCtx->time_base.den==1)
    {
        pCodecCtx->time_base.den=1000;
    }

    /*Check if a batched read should be performed*/
    if(batched_read != FW_NO_BATCHED_READ)
    {
        packet_space = 128;
        pPackets =     (AVPacket*)calloc(packet_space, sizeof(AVPacket));
        if(NULL == pPackets)
        {
            fprintf(stderr, "ERROR: calloc failed to allocate space for"
                            " %li AVPacket objects in fw_init_decoder\n", 
                            packet_space);
            goto error2;
        }
        
        /*initialize the first packet, which is the next packet*/
        pkt_i = 0;
        av_init_packet(&pPackets[pkt_i]);
        pPackets[pkt_i].data = NULL;
        pPackets[pkt_i].size = 0;
        
        while(av_read_frame(pFormatCtx, &(pPackets[pkt_i]))>=0)
        {
            if( pPackets[pkt_i].stream_index == strm_desired)
            {
                pkt_i++;
                
                if(pkt_i == packet_space)
                {
                    packet_space = packet_space + 128;
                    
                    p_dummy = realloc(pPackets, (packet_space*sizeof(AVPacket)));
                    if(NULL == p_dummy)
                    {
                        fprintf(stderr, "ERROR: realloc failed to allocate space for"
                                        " %li AVPacket objects in fw_init_decoder\n", 
                                        packet_space);
                        goto error3;
                    }
                    else
                    {
                        pPackets = p_dummy;
                        memset(&(pPackets[pkt_i]), 0, 
                                sizeof(AVPacket)*(packet_space-pkt_i));
                    }
                }
                
                /*initialize the next packet*/
                av_init_packet(&pPackets[pkt_i]);
                pPackets[pkt_i].data = NULL;
                pPackets[pkt_i].size = 0;        
            }
        }
        
        /*Free excess allocated space*/
        p_dummy = realloc(pPackets, (pkt_i*sizeof(AVPacket)));
        if(NULL == p_dummy)
        {
            fprintf(stderr, "ERROR: realloc failed to resize space for"
                            " %li AVPacket objects in fw_init_decoder\n", 
                            packet_space);
            goto error3;
        }
        else
        {
            pPackets = p_dummy;
            packet_space = pkt_i;
        }
    }
    else
    {
        pPackets = NULL;
        packet_space = 0;
        pkt_i = 0;
    }

    /*fill in the fw_decoder data structure*/
    pDec->pFormatCtx = pFormatCtx;
    pDec->pStream = pStream;
    pDec->media_type = media_type;
    pDec->decode = decode;
    pDec->pCodec = pCodec;
    pDec->pCodecCtx = pCodecCtx;
    pDec->stream_index = strm_desired;

    pDec->batched_read = batched_read;
    pDec->pPackets = pPackets;
    pDec->packets_read = packet_space;
    pDec->packets_decoded = 0;

    pDec->frames_available = pStream->nb_frames;
    pDec->frames_decoded = 0;

    return 0;

    /*undo operations for error conditions at 
      different stages*/
error3:
    for(; pkt_i >= 0; pkt_i--)
    {
        av_free_packet(&(pPackets[pkt_i]));
    }
    free(pPackets);
error2:
    avcodec_close(pCodecCtx);
error1:
       /* Close the video file */
    avformat_close_input(&pFormatCtx);
error0:
    return -1;
}

int fw_decode_nxtpkt(fw_decoder_t   *pDec, 
                     AVFrame        *pFrame,
                     int            *pgot_frame)
{
    int ret = 0;

    int got_frame;

    AVFormatContext *pFormatCtx  = pDec->pFormatCtx;
    AVCodecContext  *pCodecCtx   = pDec->pCodecCtx;

    fw_decode_t     decode  = pDec->decode;
    
    unsigned char batched_read  = pDec->batched_read;
    AVPacket *packet_buffer = pDec->pPackets;
    AVPacket *pPkt          = NULL;
    uint64_t packets_decoded= pDec->packets_decoded;
    uint64_t packets_read   = pDec->packets_read;

    AVPacket Pkt;
    int got_pkt;

    AVFrame frame_local;

    avcodec_get_frame_defaults(&frame_local);

    if(batched_read != FW_NO_BATCHED_READ)
    {
        /*loop until a complete frame is decoded or an attempt is made to extract
        any remaining buffered frames*/
        
        do /*while((got_pkt != 0) && (got_frame == 0))*/
        {
            if(packets_decoded == packets_read)
            {
                /*There are no more packets*/
                got_pkt = 0;
            
                /*Pass an empty packet to the decoder to extract any buffered frames*/
                /*initialize the next packet*/
                av_init_packet(&Pkt);
                Pkt.data = NULL;
                Pkt.size = 0;
                ret = decode(pCodecCtx, &frame_local, &got_frame, &Pkt);
                if(ret < 0)
                {
                    fprintf(stderr, "ERROR: the decoder function returned an error "
                                    "while trying to extract any buffered frames in "
                                    "fw_decode_nxtpkt\n");
                    ret = -1;
                    goto error0;
                }
            }
            else /*packets_decoded != packets_read*/
            {
                pPkt = &(packet_buffer[packets_decoded]);
            
                /*Have an additional packet to decode*/
                got_pkt = 1;
            
                ret = decode(pCodecCtx, &frame_local, &got_frame, pPkt);
                if(ret < 0)
                {
                    fprintf(stderr, "ERROR: the decoder function returned an error "
                                    "value in fw_decode_nxtpkt\n");
                    ret = -1;
                    goto error0;
                }
                else
                {
                    packets_decoded++;
                }
            }
            
            if(0 != got_frame)
            {
                ret = fw_frame_copy(pDec->media_type,
                                    &frame_local,
                                    pFrame);
                                      
                if(frame_local.extended_data != frame_local.data)
                {
                    av_freep(frame_local.extended_data);
                }
            }
            
        }while((got_pkt != 0) && (got_frame == 0));
    }
    else /*batched_read == FW_NO_BATCHED_READ*/
    {
        av_init_packet(&Pkt);
        Pkt.data = NULL;
        Pkt.size = 0;

        /*loop until a complete frame is decoded or an attempt is made to extract
        any remaining buffered frames*/
        do
        {
            if(av_read_frame(pFormatCtx, &Pkt) < 0)
            {
                /*No packets were read, so setup the parameters to extract the 
                last buffered frame from the decoder*/
                Pkt.data = NULL;
                Pkt.size = 0;
                got_pkt = 0;
            }
            else
            {
                /*A valid packet was received*/
                got_pkt = 1;
                packets_read++;
            }
            
            ret = decode(pCodecCtx, &frame_local, &got_frame, &Pkt);
            if(ret < 0)
            {
                fprintf(stderr, "ERROR: the decoder function returned an error "
                                "while trying to extract any buffered frames in "
                                "fw_decode_nxtpkt\n");
                if(got_pkt)
                {
                    av_free_packet(&Pkt);
                }
                
                ret = -1;
                goto error0;
            }
            else
            {
                packets_decoded++;
             
                if(0 != got_frame)
                {
                    ret = fw_frame_copy(pDec->media_type,
                                        &frame_local,
                                        pFrame);
                                          
                    if(frame_local.extended_data != frame_local.data)
                    {
                        av_freep(frame_local.extended_data);
                    }
                }
             
                if(got_pkt)
                {
                    av_free_packet(&Pkt);
                }
            }
            
        }while((got_pkt != 0) && (got_frame == 0));
    }
    
    if(got_frame)
    {
        (pDec->frames_decoded)++;
    }
error0:
    pDec->packets_decoded = packets_decoded;
    pDec->packets_read    = packets_read;    
    *pgot_frame = got_frame;
    return ret;
}

void fw_free_decoder(fw_decoder_t *pDec)
{
    AVPacket    *pPackets;
    uint64_t    packets_read;
    uint64_t    pkt_i;

	AVCodecContext  	*pCodecCtx;
	AVFormatContext 	*pFormatCtx;
    
    if(pDec->batched_read != FW_NO_BATCHED_READ)
    {
        pPackets = pDec->pPackets;
        packets_read = pDec->packets_read;
        for(pkt_i = 0; pkt_i < packets_read; pkt_i++)
        {
            av_free_packet(&(pPackets[pkt_i]));
        }
        free(pPackets);
    }
    
    pCodecCtx = pDec->pCodecCtx;
    avcodec_close(pCodecCtx);
    
    pFormatCtx = pDec->pFormatCtx;
    avformat_close_input(&pFormatCtx);

    memset(pDec, 0, sizeof(fw_decoder_t));    
}

#ifdef TEST_FW_DECODER

/* gcc -Wall -O3 -o fw_decoder_test ./fw_decoder.c ./fw_video.c ./fw_audio.c -D TEST_FW_DECODER -lavformat -lswresample -lswscale -lavcodec -lpostproc -lavfilter -lavutil -lgsm -lmp3lame -lopencore-amrnb -lopus -lspeex -lvorbis -lvorbisenc -lvpx -lx264  -lz -lm*/

char * usage_string = " <file name> < audio | video > [batched]";

int main(int argc, char** argv)
{
    int ret = 0;
    char *filename;

    fw_decoder_t decoder;
    AVFrame   *pFrame;

    int got_frame;

    unsigned char batched;
    
    fw_init();

    memset(&decoder, 0, sizeof(fw_decoder_t));
    
    /*parse options*/
    if((argc < 3) || (argc > 4))
    {
        fprintf(stderr, "Usage: %s %s\n", argv[0], usage_string);
        ret = -1;
        goto error0;
    }
    
    filename = argv[1];
   
    if(argc == 4)
    {
        if(strcmp(argv[3], "batched") == 0)
        {
            batched = 1;
        }
        else
        {
            fprintf(stderr, "Usage: %s %s\n", argv[0], usage_string);
            ret = -1;
            goto error0;
        }
    }
    else
    {
        batched = 0;
    }
     
    if(0 == strcmp(argv[2], "audio"))
    {
        ret = fw_audio_init_decoder(filename, &decoder, batched);
        if(ret < 0)
        {
            fprintf(stderr, "fw_audio_init_decoder failed in main\n");
            ret = -1;
            goto error0;
        }
    }
    else
    {
        if(0 == strcmp(argv[2], "video"))
        {
            ret = fw_video_init_decoder(filename, &decoder, batched);
            if(ret < 0)
            {
                fprintf(stderr, "fw_video_init_decoder failed in main\n");
                ret = -1;
                goto error0;
            }   
        }
        else
        {
            fprintf(stderr, "Usage: %s %s\n", argv[0], usage_string);
            ret = -1;
            goto error0;
        }
    }

    /*allocate an AVFrame object*/
    pFrame = fw_alloc_frame();
    if(NULL == pFrame)
    {
        fprintf(stderr, "fw_alloc_frame failed in main");
        ret = -1;
        goto error1;
    }

    /*do the decoding*/
    do
    {
        ret = fw_decode_nxtpkt( &decoder, 
                                pFrame,
                                &got_frame);
        if(ret < 0)
        {
            fprintf(stderr, "fw_decode_nxtpkt failed in main\n");
            ret = -1;
            goto error2;
        }

    }while(got_frame != 0);

    printf("Read %li packets.\n", decoder.packets_read);
    printf("Processed %li packets.\n", decoder.packets_decoded);
    printf("%li frames available.\n", decoder.frames_available);
    printf("Decoded %li frames.\n", decoder.frames_decoded);
        
error2:
    fw_free_copied_data(decoder.media_type, pFrame);
    fw_free_frame(&pFrame);
error1:
    fw_free_decoder(&decoder);
error0:
    return ret;
}

#endif

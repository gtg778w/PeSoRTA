#include "ffmpegwrapper.h"

/* gcc -Wall -O3 -o fw_getjobperiods ./fw_getjobperiods.c ./fw_decoder.c ./fw_video.c ./fw_audio.c -lavformat -lswresample -lswscale -lavcodec -lpostproc -lavfilter -lavutil -lgsm -lmp3lame -lopencore-amrnb -lopus -lspeex -lvorbis -lvorbisenc -lvpx -lx264  -lz -lm*/

char * usage_string = " <file name>";

int main(int argc, char** argv)
{
    int ret = 0;
    char *filename;

    fw_decoder_t decoder;
    AVFrame   *pFrame;

    int got_frame;

    unsigned char batched;
    
    double sample_rate;
    double frame_samples;
    double frame_duration;
    
    fw_init();

    memset(&decoder, 0, sizeof(fw_decoder_t));
    
    /*parse options*/
    if(argc != 2)
    {
        fprintf(stderr, "Usage: %s %s\n", argv[0], usage_string);
        ret = -1;
        goto error0;
    }
    
    /*Get the input file name*/
    filename = argv[1];
   
    /*Read one packet at a time*/
    batched = 0;
     
    /*Initialize the decoder*/
    ret = fw_audio_init_decoder(filename, &decoder, batched);
    if(ret < 0)
    {
        fprintf(stderr, "fw_audio_init_decoder failed in main\n");
        ret = -1;
        goto error0;
    }

    /*Get the sample rate*/
    sample_rate = (double)(decoder.pCodecCtx->sample_rate);
    if(sample_rate <= 0)
    {
        fprintf(stderr, "Invalid sample rate: %f\n", sample_rate);
        ret = -1;
        goto error1;
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
    printf("\n\n\tframe durations:\n");
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

        /*Get the number of samples in the frame*/
        frame_samples = (double)(pFrame->nb_samples);
        
        frame_duration = frame_samples / sample_rate;
        printf("\t%f, ", frame_duration);
    }while(got_frame != 0);
    printf("\n\n");
    
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


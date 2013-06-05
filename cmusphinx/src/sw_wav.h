
typedef struct sw_wavheader_s
{
    char    ChunkID[4];
    int32_t ChunkSize;
    char    Format[4];
    
    char    Subchunk1ID[4];
    int32_t Subchunk1Size;
    int16_t AudioFormat;
    int16_t NumChannels;
    int32_t SampleRate;
    int32_t ByteRate;
    int16_t BlockAlign;
    int16_t BitsPerSample;
    
    char    Subchunk2ID[4];
    int32_t Subchunk2Size;
} sw_wavheader_t;

static inline int sw_verify_wav(sw_wavheader_t *wavheader_p)
{
    int ret;
    
    ret = strncmp(wavheader_p->ChunkID, "RIFF", 4);
    if(0 != ret)
    {
        ret = -1;
        goto exit0;
    }
    
    ret = strncmp(wavheader_p->Format, "WAVE", 4);
    if(0 != ret)
    {
        ret = -1;
        goto exit0;
    }
    
    ret = strncmp(wavheader_p->Subchunk1ID, "fmt ", 4);
    if(0 != ret)
    {
        ret = -1;
        goto exit0;
    }

    if(16 != wavheader_p->Subchunk1Size)
    {
        ret = -1;
        goto exit0;
    }
    
    if(1 != wavheader_p->AudioFormat)
    {
        ret = -1;
        goto exit0;
    }
    
    if(1 != wavheader_p->NumChannels)
    {
        ret = -1;
        goto exit0;
    }
    
    if(16000 != wavheader_p->SampleRate)
    {
        ret = -1;
        goto exit0;
    }
    
    if(16 != wavheader_p->BitsPerSample)
    {
        ret = -1;
        goto exit0;
    }
    
    ret = strncmp(wavheader_p->Subchunk2ID, "data", 4);
    if(ret != 0)
    {
        ret = -1;
        goto exit0;
    }
    
    ret = 0;
    
exit0:
    return ret;
}

static inline size_t sw_get_wav_samples(sw_wavheader_t *wavheader_p)
{
    return (wavheader_p->Subchunk2Size)/(wavheader_p->BlockAlign);
}

static inline void sw_print_wav(FILE* filep, sw_wavheader_t *wavheader_p)
{
    fprintf(filep, "\tChunkID: \"%c%c%c%c\"\n",
                    wavheader_p->ChunkID[0],
                    wavheader_p->ChunkID[1],
                    wavheader_p->ChunkID[2],
                    wavheader_p->ChunkID[3]);

    fprintf(filep, "\tFormat: \"%c%c%c%c\"\n",
                    wavheader_p->Format[0],
                    wavheader_p->Format[1],
                    wavheader_p->Format[2],
                    wavheader_p->Format[3]);

    fprintf(filep, "\tSubchunk1ID: \"%c%c%c%c\"\n",
                    wavheader_p->Subchunk1ID[0],
                    wavheader_p->Subchunk1ID[1],
                    wavheader_p->Subchunk1ID[2],
                    wavheader_p->Subchunk1ID[3]);
    
    fprintf(filep, "\tSubchunk1Size: %i\n",
                    wavheader_p->Subchunk1Size);
    
    fprintf(filep, "\tAudioFormat: %i\n",
                    wavheader_p->AudioFormat);

    fprintf(filep, "\tNumChannels: %i\n",
                    wavheader_p->NumChannels);

    fprintf(filep, "\tSampleRate: %i\n",
                    wavheader_p->SampleRate);

    fprintf(filep, "\tBitsPerSample: %i\n",
                    wavheader_p->BitsPerSample);

    fprintf(filep, "\tSubchunk2ID: \"%c%c%c%c\"\n",
                    wavheader_p->Subchunk2ID[0],
                    wavheader_p->Subchunk2ID[1],
                    wavheader_p->Subchunk2ID[2],
                    wavheader_p->Subchunk2ID[3]);

    fprintf(filep, "\tTotal Samples: %li\n",
                    sw_get_wav_samples(wavheader_p));
}



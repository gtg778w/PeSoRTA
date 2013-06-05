#include <stdlib.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"

extern int av_registered;

/* Register all formats and codecs*/
#define fw_init()           \
do{                         \
	if(av_registered == 0)  \
	{                       \
		av_register_all();  \
		av_registered = 1;  \
	}                       \
}while(0)

/*
    Decoding related structures, functions, and definitions.
*/

typedef int (*fw_decode_t)(  AVCodecContext*, 
                                AVFrame*,	
                                int *, 
                                const AVPacket *avpkt);

typedef struct fw_decoder_s
{
   	AVFormatContext 	*pFormatCtx;
	AVStream            *pStream;
	enum AVMediaType    media_type;
	fw_decode_t         decode;
	AVCodec         	*pCodec;
	AVCodecContext  	*pCodecCtx;	
	int                 stream_index;

    unsigned char       batched_read;
	AVPacket        	*pPackets;
	uint64_t	        packets_read;
	uint64_t            packets_decoded;

    uint64_t            frames_available;	
	uint64_t            frames_decoded;
} fw_decoder_t;

enum {
    FW_NO_BATCHED_READ  = 0,    
    FW_BATCHED_READ  = 1
};

int fw_init_decoder(char                *filename, 
                    fw_decoder_t        *pDec, 
                    enum AVMediaType    media_type, 
                    unsigned char       batched_read);

#define fw_audio_init_decoder(filename, pDec, batched_read) \
        fw_init_decoder(filename, pDec, AVMEDIA_TYPE_AUDIO, batched_read)

#define fw_video_init_decoder(filename, pDec, batched_read) \
        fw_init_decoder(filename, pDec, AVMEDIA_TYPE_VIDEO, batched_read)

#define fw_alloc_frame() avcodec_alloc_frame()
#define fw_free_frame(ppFrame) avcodec_free_frame(ppFrame)

int fw_frame_copy( enum AVMediaType    media_type,
                   AVFrame             *pFrame_src,
                   AVFrame             *pFrame_dst);

void fw_free_copied_data(  enum AVMediaType    media_type,
                            AVFrame             *pFrame);

#define fw_free_decoded_data(pDec, pFrame) \
        fw_free_copied_data((pDec)->media_type, pFrame)

int  fw_audio_frame_copy(AVFrame *pFrame_src, AVFrame *pFrame_dst);
void fw_audio_free_copied_data(AVFrame *pFrame);

int  fw_video_frame_copy(AVFrame *pFrame_src, AVFrame *pFrame_dst);
void fw_video_free_copied_data(AVFrame *pFrame);

int fw_decode_nxtpkt(fw_decoder_t   *pDec, 
                     AVFrame        *pFrame,
                     int            *pgot_frame);

void fw_free_decoder(fw_decoder_t *pDec);

/*
    General encoding related structures, functions, and definitions.
*/

typedef struct fw_eparams_s
{
    char *codec_name;
    
    int bit_rate;
    
    /*
    enum Motion_Est_ID 
    {
        ME_ZERO = 1,
        ME_FULL,
        ME_LOG,
        ME_PHODS,
        ME_EPZS,
        ME_X1,
        ME_HEX,
        ME_UMH,
        ME_TESA,
        ME_ITER=50,
    };
    */
    char    *me_method_s;

    int     width;
    int     height;
    int     gop_size;
    int     max_b_frames;

    char    *format;
    
    int     sample_rate;
} fw_eparams_t;

#define DEFALUT_EPARAMS(fw_eparams_p)\
do{\
        (fw_eparams_p)->codec_name  = NULL; \
        (fw_eparams_p)->bit_rate    = 0;    \
        (fw_eparams_p)->me_method_s = NULL; \
        (fw_eparams_p)->width       = 0;    \
        (fw_eparams_p)->height      = 0;    \
        (fw_eparams_p)->gop_size    = -1;   \
        (fw_eparams_p)->max_b_frames= 0;    \
        (fw_eparams_p)->format      = NULL; \
        (fw_eparams_p)->sample_rate = 0;    \
}while(0)

/*
    Audio preprocessing related structures, functions, and definitions.
*/

typedef struct fw_audio_preproc_s
{
    uint64_t            channel_layout_src;
    enum AVSampleFormat smpl_fmt_src;
    int                 smpl_rate_src;
    
    uint64_t            channel_layout_dst;
    enum AVSampleFormat smpl_fmt_dst;
    int                 smpl_rate_dst;
    
    struct SwrContext   *pSwrCtx;
    uint8_t             **swrdata;
    int                 buffersize_swrdata;
    int                 framesize_swrdata;
    int                 offset_swrdata;
    int                 issilent_swrdata;
    int                 framesize_dst;
    int                 offset_dst;
} fw_audio_preproc_t;

int fw_audio_init_preproc(   AVCodecContext     *pCodecCtx_src,
                             AVCodec            *pCodec_dst,
                             AVCodecContext     *pCodecCtx_dst,
                             void               **preproc_state,
                             fw_eparams_t       *eparams);

int fw_audio_preproc(   void    *preproc_state,
                        AVFrame *pAVFrame_src,
                        int     *frame_consumed,
                        AVFrame *pAVFrame_dst,
                        int     *frame_produced);

void fw_audio_end_preproc(  void    *preproc_state,
                            AVFrame *pAVFrame_dst,
                            int     *frame_produced);

void fw_audio_free_preproc(void **p_preproc_state);

int fw_audio_alloc_preproc_frame(AVFrame **ppFrame_output,
                                 void    *preproc_state);
void fw_audio_free_preproc_frame(AVFrame *pFrame);

/*
    Video preprocessing related structures, functions, and definitions.
*/

typedef struct fw_video_preproc_s
{
    enum AVPixelFormat  pix_fmt_src;
    enum AVPixelFormat  pix_fmt_dst;

    int width;
    int height;
    
    struct SwsContext   *pSwsCtx;
} fw_video_preproc_t;

enum Motion_Est_ID fw_video_get_me_method(char * method_name);

int fw_video_init_preproc(   AVCodecContext     *pCodecCtx_src,
                             AVCodec            *pCodec_dst,
                             AVCodecContext     *pCodecCtx_dst,
                             void               **preproc_state,
                             fw_eparams_t       *eparams);

int fw_video_preproc(   void    *preproc_state,
                        AVFrame *pAVFrame_src,
                        int     *frame_consumed,
                        AVFrame *pAVFrame_dst,
                        int     *frame_produced);

void fw_video_end_preproc(  void    *preproc_state,
                            AVFrame *pAVFrame_dst,
                            int     *frame_produced);

void fw_video_free_preproc(void **p_preproc_state);

int fw_video_alloc_preproc_frame(AVFrame **ppFrame_output,
                                 void    *preproc_state);

void fw_video_free_preproc_frame(AVFrame *pFrame);

/*
General preprocesing related structures and definitions
*/

typedef int (*fw_preproc_t)(void*,
                            AVFrame*,
                            int*,
                            AVFrame*,
                            int*);

typedef void (*fw_end_preproc_t)(   void*,
                                    AVFrame*,
                                    int*);

typedef int (*fw_alloc_preproc_frame_t)(AVFrame**,
                                        void*);

typedef void (*fw_free_preproc_frame_t)(AVFrame*);

typedef struct fw_preproc_state_s
{
    void                *preproc_state;
    enum AVMediaType    media_type;
    fw_preproc_t        preproc;
    fw_end_preproc_t    end_preproc;
    fw_alloc_preproc_frame_t    alloc_preproc_frame;
    fw_free_preproc_frame_t     free_preproc_frame;
} fw_preproc_state_t;

int fw_init_preproc(AVCodecContext  *pCodecCtx_src,
                    AVCodec         *pCodec_dst,
                    AVCodecContext  *pCodecCtx_dst,
                    fw_preproc_state_t    *pPreproc,
                    fw_eparams_t    *pEparams);

void fw_free_preproc(fw_preproc_state_t    *pPreproc);

typedef int (*fw_encode_t)( AVCodecContext*,
                            AVPacket *avpkt,
                            const AVFrame*,
                            int *);

typedef struct fw_encoder_s
{
	AVCodec         	*pCodec;
	AVCodecContext  	*pCodecCtx;

    AVFrame         	**pFrameArray;
    uint64_t            frames_available;
    
    fw_preproc_state_t  preproc;
    /*This following flag is set if 
    the preprocessor hasn't yet processed
    the last frame passed to it*/
    int                 frame_preprocing;
    uint64_t            frames_preproced;
    
    /*This following flag is set if there 
    are no more frames left to encode*/
    int                 nomore_eframes; 
    
    AVFrame         	*pFramePreenc;
    fw_encode_t         encode;
    /*This following is always set until
    the encoder if no longer supplied with
    new frames and no more packets are 
    extracted from it*/
    int                 nomore_packets;
} fw_encoder_t;

void fw_print_encoder_list(FILE *fstream);

int fw_init_encoder(char            *input_filename,
                    fw_encoder_t    *pEnc,
                    fw_eparams_t    *pParams);

int fw_encode_step( fw_encoder_t *pEnc,
                    int          *frame_consumed,
                    AVPacket     *pPacket,
                    int          *packet_produced);

void fw_free_encoder(fw_encoder_t *pEnc);


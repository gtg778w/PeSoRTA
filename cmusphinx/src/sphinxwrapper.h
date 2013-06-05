#include <stdint.h>

/*Header files related to the sphinx libraries*/
#include <sphinxbase/err.h>
#include <sphinxbase/ad.h>
#include <sphinxbase/cont_ad.h>
#include <pocketsphinx/pocketsphinx.h>

#define SW_DEFAULT_SAMPLE_RATE (16000)

typedef enum
{
    SW_STATE_NONE = 0,
    SW_STATE_SILENCE,
    SW_STATE_SPEECH,
} sw_state_t;

typedef struct sw_data_s
{
    /*State for detecting speech*/
    sw_state_t  state;
    int32_t     last_speech_sample;
    
    /*Silence filter from sphinx base*/
    cont_ad_t   *cont;
    ad_rec_t    ad_rec;
    
    int32_t     silence_thresh;
    
    /*PocketSphix speech decoder state*/
    ps_decoder_t *ps;
    
} sw_data_t;

int allocate_sw_data(   sw_data_t   **pp_sw_data, 
                        char        *psconfig_filename,
                        int32_t silence_thresh);

void free_sw_data(sw_data_t **pp_sw_data);

int sw_calib_silence(sw_data_t *p_sw_data, int16_t *calib_data, size_t data_size);

int sw_decode_speech(   sw_data_t *p_sw_data, 
                        int16_t *speech_data, 
                        size_t data_size, 
                        char **p_hyp_out);

int sw_extractlast_hyp( sw_data_t *p_sw_data, 
                        char **p_hyp_out);


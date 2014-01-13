#ifndef SQRWAV_HEADER
#define SQRWAV_HEADER

#include <stdint.h>

struct sqrwav_struct
{
    uint64_t    period;
    double      duty_cycle;
    uint64_t    minimum_nominal_value;
    uint64_t    maximum_nominal_value;    
    double      noise_ratio;

    int64_t    index;
    uint64_t   rangen_state;
};

#define LCG_MAX (double)(~((uint64_t)0))

static uint64_t inline sqrwav_next(struct sqrwav_struct *sqrwav_p)
{
    uint64_t aliased_index;
    uint64_t duty_cycle_length;
    uint64_t nominal_value;

    double max_noise, noise;

    uint64_t output;

    duty_cycle_length = 
        (uint64_t)(sqrwav_p->duty_cycle * (double)sqrwav_p->period);

    aliased_index = (sqrwav_p->index) % (sqrwav_p->period);
    nominal_value = (aliased_index < duty_cycle_length)?
                    sqrwav_p->maximum_nominal_value:
                    sqrwav_p->minimum_nominal_value;
    (sqrwav_p->index)++;

    //generate the next random number (Knuth's 64bit LCG taken from wikipedia)
    sqrwav_p->rangen_state = (sqrwav_p->rangen_state) * 6364136223846793005
                                + 1442695040888963407;

    max_noise = sqrwav_p->noise_ratio * (double)nominal_value;
    noise = (max_noise * (double)(sqrwav_p->rangen_state))/LCG_MAX;

    output = nominal_value + noise;    
    return output;
}

#endif


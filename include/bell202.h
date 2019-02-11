#ifndef _BELL202_H
#define _BELL202_H

#include <stdio.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI (3.141592654f)
#endif

typedef struct {
    int input_rate;

    resamp_rrrf rs;
    float *resamp_buff;

    firfilt_rrrf mark_cs, mark_sn;
    firfilt_rrrf space_cs, space_sn;

    firfilt_rrrf mark_filt, space_filt;
    firfilt_rrrf flag_corr;

    resamp_rrrf sps2;
    symsync_rrrf sync;

    int buff_idx;
    float *cos_mark, *sin_mark, *cos_space, *sin_space;
    float *mark_buff, *space_buff;
    float *data_filt_taps;
    float mark_max, mark_min, space_max, space_min;
    float mark_scale;
    float space_scale;
    float last_bit;

    FILE *out;
} bell202_t;

bool bell202_init(bell202_t *modem, int input_rate);
bool bell202_process(bell202_t *modem, float samp, float *out_bit);
void bell202_destroy(bell202_t *modem);

#endif

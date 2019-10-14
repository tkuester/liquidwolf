/**
 * @file bell202.h
 * @brief Struct definitions for the modem
 */
#ifndef _BELL202_H
#define _BELL202_H

#include <stdio.h>
#include <stdbool.h>

#ifndef M_PI
#define M_PI (3.141592654f)
#endif

/**
 * @brief The Bell 202 modem object
 *
 * This contains all the "state" of the modem, including filter taps,
 * sample indexes, and liquid dsp objects.
 */
typedef struct {
    int input_rate; /**< @brief The incoming sample rate */

    /**
     * @name Stage 1: Input resampling
     * @{
     */
    resamp_rrrf rs; /**< @brief The input resampler */
    /**
     * @brief Holds resampler output
     * 
     * In most cases, the input rate will be > the internal rate of 13200
     * sps. In the event it isn't (ie: 8000 Hz audio), the resampler may
     * output more than one sample, which is stored here.
     */
    float *resamp_buff;
    ///@}

    /**
     * @name Stage 2: Audio filtering
     * @{
     */
    float *cos_mark, *sin_mark, *cos_space, *sin_space;
    /** @brief mark / space filters (re+im) */
    firfilt_rrrf mark_cs, mark_sn;
    firfilt_rrrf space_cs, space_sn;
    ///@}

    /**
     * @name Stage 3: Magnitude detection
     * @{
     */
    int buff_idx;
    float *mark_buff, *space_buff;
    float mark_max, mark_min, space_max, space_min;
    float mark_scale, space_scale;
    float *data_filt_taps;

    /** @brief mark / space magnitude filters */
    firfilt_rrrf mark_filt, space_filt;
    firfilt_rrrf flag_corr;
    ///@}

    /**
     * @name Stage 4: Clock Recovery
     * @{
     */
    float last_bit;
    resamp_rrrf sps2; /**< @brief Resample to 4 sps */
    symsync_rrrf sync; /**< @brief Clock recovery */
    ///@}

    FILE *out;
} bell202_t;

bool bell202_init(bell202_t *modem, int input_rate);
bool bell202_process(bell202_t *modem, float samp, float *out_bit);
void bell202_destroy(bell202_t *modem);

#endif

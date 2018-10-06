#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <liquid/liquid.h>
#include "bell202.h"
#include "util.h"

const int baud_rate = 1200;
const int mark = 1200;
const int space = 2200;
const int output_rate = 13200; // (1200: 11, 2200: 6)
#define win_sz (output_rate / baud_rate)
#define SCALE_WIN_LEN (9)

bool bell202_init(bell202_t *modem, int input_rate) {
    if(modem == NULL) return false;
    memset(modem, 0, sizeof(bell202_t));

    modem->input_rate = input_rate;

    // Stage 1: Resample input to 13200 Hz
    {
        float r = 1.0f * output_rate / modem->input_rate;
        unsigned int h_len = 13;
        float bw = 4.0f * baud_rate / modem->input_rate;
        float slsl = 60.0f;
        unsigned int npfb = 32;
        modem->rs = resamp_rrrf_create(r, h_len, bw, slsl, npfb);
        if(modem->rs == NULL) goto fail;

        modem->resamp_buff = (float *)malloc((int)ceil(r) * sizeof(float));
        if(modem->resamp_buff == NULL) goto fail;
    }

    // Stage 2: Generate filter taps for mark/space detection
    modem->cos_mark = malloc(sizeof(float) * win_sz);
    modem->sin_mark = malloc(sizeof(float) * win_sz);
    modem->cos_space = malloc(sizeof(float) * win_sz);
    modem->sin_space = malloc(sizeof(float) * win_sz);
    if(!modem->cos_mark || !modem->sin_mark || !modem->cos_space || !modem->sin_space) goto fail;

    for(int i = 0; i < win_sz; i++) {
        //float win = sin(1.0f * M_PI * i / win_sz);
        float win = 1.0;
        modem->cos_mark[i] = cosf(2 * M_PI * mark * i / output_rate) * win;
        modem->sin_mark[i] = sinf(2 * M_PI * mark * i / output_rate) * win;
        modem->cos_space[i] = cosf(2 * M_PI * space * i / output_rate) * win;
        modem->sin_space[i] = sinf(2 * M_PI * space * i / output_rate) * win;
    }
    normalize(modem->cos_mark, win_sz);
    normalize(modem->sin_mark, win_sz);
    normalize(modem->cos_space, win_sz);
    normalize(modem->sin_space, win_sz);

    modem->mark_cs = firfilt_rrrf_create(modem->cos_mark, win_sz);
    modem->mark_sn = firfilt_rrrf_create(modem->sin_mark, win_sz);
    modem->space_cs = firfilt_rrrf_create(modem->cos_space, win_sz);
    modem->space_sn = firfilt_rrrf_create(modem->sin_space, win_sz);
    if(!modem->mark_cs || !modem->mark_sn || !modem->space_cs || !modem->space_sn) goto fail;

    // Stage 3.1 - "AGC" scaling for mark/space channels
    modem->mark_max = modem->space_max = 1;
    modem->mark_min = modem->space_min = -1;
    modem->mark_buff = malloc(sizeof(float) * win_sz * SCALE_WIN_LEN);
    modem->space_buff = malloc(sizeof(float) * win_sz * SCALE_WIN_LEN);
    if(!modem->mark_buff || !modem->space_buff) goto fail;
    memset(modem->mark_buff, 0, sizeof(float) * win_sz * SCALE_WIN_LEN);
    memset(modem->space_buff, 0, sizeof(float) * win_sz * SCALE_WIN_LEN);

    // Stage 3: 1200 Hz filter for mark/space channel
    {
        float As = 60.0f;
        float fc = (float)baud_rate / output_rate;
        float ft = (float)baud_rate / output_rate;
        float mu = 0.0f;
        const unsigned int df_len = estimate_req_filter_len(ft, As);

        modem->data_filt_taps = malloc(sizeof(float) * df_len);
        if(!modem->data_filt_taps) goto fail;

        // Design the 1200 Hz filter, scale to gain of 1.0 
        liquid_firdes_kaiser(df_len, fc, As, mu, modem->data_filt_taps);
        normalize(modem->data_filt_taps, df_len);

        modem->mark_filt = firfilt_rrrf_create(modem->data_filt_taps, df_len); 
        modem->space_filt = firfilt_rrrf_create(modem->data_filt_taps, df_len); 
        if(!modem->mark_filt || !modem->space_filt) goto fail;
    }

    float sync_search[] = {-0.327787071466, -0.863485574722, -0.846625030041, -0.237066477537,
                           0.509203135967, 0.92675024271, 0.967760741711, 0.90081679821,
                           0.890751242638, 0.906954228878, 0.90988856554, 0.908331274986,
                           0.909126937389, 0.910285055637, 0.910628736019, 0.910407721996,
                           0.910471260548, 0.911236822605, 0.911468148232, 0.911164462566,
                           0.91139292717, 0.912554085255, 0.912695467472, 0.911213994026,
                           0.910359799862, 0.912545442581, 0.908676445484, 0.896155297756,
                           0.916578114033, 0.974584817886, 0.887850761414, 0.431282520294,
                           -0.297088474035, -0.840644657612, -0.802261173725, -0.211893334985};

    normalize(sync_search, ASIZE(sync_search));
    modem->flag_corr = firfilt_rrrf_create(sync_search, ASIZE(sync_search));
    if(!modem->flag_corr) goto fail;

    // Stage 4: Resample to 4 sps for clock recovery
    {
        const int sps = 4;
        unsigned int h_len = 13;
        float bw2 = 2.0 * baud_rate / output_rate;
        float slsl = 60.0;
        float r2 = (float)sps * baud_rate / output_rate;
        unsigned int npfb = 32;
        if(r2 > 1) goto fail; // Logic in stage 4 expects no more than 1 output sample
        modem->sps2 = resamp_rrrf_create(r2, h_len, bw2, slsl, npfb);
        if(!modem->sps2) goto fail;

        // Stage 5: Clock Recovery
        modem->sync = symsync_rrrf_create_rnyquist(LIQUID_FIRFILT_RRC, sps, 7, (4.0 * baud_rate / output_rate), 32);
        if(!modem->sync) goto fail;
    }

    // Stage 6: Differential decoding, HDLC
    modem->last_bit = 0;

    /*
    modem->out = fopen("out.f32", "w");
    if(!modem->out) goto fail;
    */
    modem->out = NULL;

    return true;

fail:
    bell202_destroy(modem);
    return false;
}

bool bell202_process(bell202_t *modem, float samp, float *out_bit) {
    unsigned int num_resamp;

    // Stage 1: Resample to 13200 Hz
    resamp_rrrf_execute(modem->rs, samp, modem->resamp_buff, &num_resamp);

    for(int j = 0; j < num_resamp; j++) {
        // Stage 2: Update re/im correlators for mark/space
        float mark_mag, space_mag;
        {
            float re, im;
            firfilt_rrrf_push(modem->mark_cs, modem->resamp_buff[j]);
            firfilt_rrrf_execute(modem->mark_cs, &re);
            firfilt_rrrf_push(modem->mark_sn, modem->resamp_buff[j]);
            firfilt_rrrf_execute(modem->mark_sn, &im);
            mark_mag = sqrtf(re * re + im * im);

            firfilt_rrrf_push(modem->space_cs, modem->resamp_buff[j]);
            firfilt_rrrf_execute(modem->space_cs, &re);
            firfilt_rrrf_push(modem->space_sn, modem->resamp_buff[j]);
            firfilt_rrrf_execute(modem->space_sn, &im);
            space_mag = sqrtf(re * re + im * im);
        }

        // Stage 3: Filter mark signal
        firfilt_rrrf_push(modem->mark_filt, mark_mag);
        firfilt_rrrf_execute(modem->mark_filt, &mark_mag);
        modem->mark_buff[modem->buff_idx] = mark_mag;
        firfilt_rrrf_push(modem->space_filt, space_mag);
        firfilt_rrrf_execute(modem->space_filt, &space_mag);
        modem->space_buff[modem->buff_idx] = space_mag;

        modem->buff_idx += 1;
        modem->buff_idx %= SCALE_WIN_LEN * win_sz;

        // Stage 3.1: Calculate AGC scaling parameters
        if(modem->buff_idx == 0) {
            minmax(modem->mark_buff, SCALE_WIN_LEN * win_sz, &modem->mark_min, &modem->mark_max);
            modem->mark_scale = 1.0 / (modem->mark_max - modem->mark_min);
            modem->mark_scale = (modem->mark_scale > 10 ? 10 : modem->mark_scale);

            minmax(modem->space_buff, SCALE_WIN_LEN * win_sz, &modem->space_min, &modem->space_max);
            modem->space_scale = 1.0 / (modem->space_max - modem->space_min);
            modem->space_scale = (modem->space_scale > 10 ? 10 : modem->space_scale);
        }

        // Scale waveforms
        mark_mag -= (modem->mark_max + modem->mark_min) / 2;
        mark_mag *= modem->mark_scale;

        space_mag -= (modem->space_max + modem->space_min) / 2;
        space_mag *= modem->space_scale;

        // Data = space - mark, differential waveforms
        float data_mag = space_mag - mark_mag;

        /*
        fwrite(&modem->resamp_buff[j], sizeof(float), 1, modem->out);
        fwrite(&mark_mag, sizeof(float), 1, modem->out);
        fwrite(&space_mag, sizeof(float), 1, modem->out);
        fwrite(&data_mag, sizeof(float), 1, modem->out);
        */

        // Stage 4: Resample to 4 sps
        unsigned int num_written2;
        resamp_rrrf_execute(modem->sps2, data_mag, &data_mag, &num_written2);
        if(num_written2 == 0) continue;

        // Stage 4.1 - Calculate correlation of flag
        float flag_corr_mag;
        firfilt_rrrf_push(modem->flag_corr, data_mag);
        firfilt_rrrf_execute(modem->flag_corr, &flag_corr_mag);

        /*
        fwrite(&data_mag, sizeof(float), 1, modem->out);
        fwrite(&modem->flag_corr_mag, sizeof(float), 1, modem->out);
        */

        // Stage 5 - Clock Recovery
        float bit;
        symsync_rrrf_execute(modem->sync, &data_mag, 1, &bit, &num_written2);
        if(num_written2 == 0) continue;
        //float tau = symsync_rrrf_get_tau(sync);

        // Stage 6 - Differential Decoding
        float nrzi = fabs(bit);
        if((bit < 0 && modem->last_bit >= 0) || (bit >= 0 && modem->last_bit < 0)) {
            nrzi *= -1;
        }
        modem->last_bit = bit;

        if(out_bit) *out_bit = nrzi;
        return true;
    }

    return false;
}

void bell202_destroy(bell202_t *modem) {
    if(modem->rs) resamp_rrrf_destroy(modem->rs);
    if(modem->resamp_buff) free(modem->resamp_buff);

    if(modem->sps2) resamp_rrrf_destroy(modem->sps2);

    if(modem->mark_cs) firfilt_rrrf_destroy(modem->mark_cs);
    if(modem->mark_sn) firfilt_rrrf_destroy(modem->mark_sn);
    if(modem->space_cs) firfilt_rrrf_destroy(modem->space_cs);
    if(modem->space_sn) firfilt_rrrf_destroy(modem->space_sn);
    if(modem->mark_filt) firfilt_rrrf_destroy(modem->mark_filt);
    if(modem->space_filt) firfilt_rrrf_destroy(modem->space_filt);
    if(modem->flag_corr) firfilt_rrrf_destroy(modem->flag_corr);
    if(modem->sync) symsync_rrrf_destroy(modem->sync);

    if(modem->cos_mark) free(modem->cos_mark);
    if(modem->sin_mark) free(modem->sin_mark);
    if(modem->cos_space) free(modem->cos_space);
    if(modem->sin_space) free(modem->sin_space);
    if(modem->mark_buff) free(modem->mark_buff);
    if(modem->space_buff) free(modem->space_buff);
    if(modem->data_filt_taps) free(modem->data_filt_taps);

    if(modem->out) fclose(modem->out);
}

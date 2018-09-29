#include <string.h>
#include <math.h>

#include <liquid/liquid.h>
#include "dsp.h"
#include "util.h"
#include "hdlc.h"

const int baud_rate = 1200;
const int mark = 1200;
const int space = 2200;
const int output_rate = 13200; // (1200: 11, 2200: 6)

int input_rate;

resamp_rrrf rs, sps2 = NULL;

firfilt_rrrf mark_cs, mark_sn = NULL;
firfilt_rrrf space_cs, space_sn = NULL;

firfilt_rrrf mark_filt, space_filt = NULL;

symsync_rrrf sync = NULL;
hdlc_state_t hdlc;

int buff_idx;
float *cos_mark, *sin_mark, *cos_space, *sin_space = NULL;
float *mark_buff, *space_buff = NULL;
float *data_filt_taps = NULL;
float mark_max, mark_min, space_max, space_min;
float last_bit;

const float sync_search[] = {-0.327787071466, -0.863485574722, -0.846625030041, -0.237066477537,
                             0.509203135967, 0.92675024271, 0.967760741711, 0.90081679821,
                             0.890751242638, 0.906954228878, 0.90988856554, 0.908331274986,
                             0.909126937389, 0.910285055637, 0.910628736019, 0.910407721996,
                             0.910471260548, 0.911236822605, 0.911468148232, 0.911164462566,
                             0.91139292717, 0.912554085255, 0.912695467472, 0.911213994026,
                             0.910359799862, 0.912545442581, 0.908676445484, 0.896155297756,
                             0.916578114033, 0.974584817886, 0.887850761414, 0.431282520294,
                             -0.297088474035, -0.840644657612, -0.802261173725, -0.211893334985};
float sync_buff[ASIZE(sync_search)];
size_t sync_buff_idx;

bool dsp_init(int _input_rate) {
    input_rate = _input_rate;

    // Stage 1: Resample input to 13200 Hz
    float r = 1.0f * output_rate / input_rate;
    unsigned int h_len = 13;
    float bw = 1.0f * baud_rate / output_rate;
    float slsl = 60.0f;
    unsigned int npfb = 32;
    rs = resamp_rrrf_create(r, h_len, bw, slsl, npfb);
    if(rs == NULL) goto fail;

    // Stage 2: Generate filter taps for mark/space detection
    const int win_sz = (int)(ceil)(1.0f * output_rate / baud_rate);
    cos_mark = malloc(sizeof(float) * win_sz);
    sin_mark = malloc(sizeof(float) * win_sz);
    cos_space = malloc(sizeof(float) * win_sz);
    sin_space = malloc(sizeof(float) * win_sz);
    if(!cos_mark || !sin_mark || !cos_space || !sin_space) goto fail;

    for(int i = 0; i < win_sz; i++) {
        float win = sin(1.0f * M_PI * i / (win_sz - 1));
        cos_mark[i] = cosf(2 * M_PI * mark * i / output_rate) * win;
        sin_mark[i] = sinf(2 * M_PI * mark * i / output_rate) * win;
        cos_space[i] = cosf(2 * M_PI * space * i / output_rate) * win;
        sin_space[i] = sinf(2 * M_PI * space * i / output_rate) * win;
    }
    normalize(cos_mark, win_sz);
    normalize(sin_mark, win_sz);
    normalize(cos_space, win_sz);
    normalize(sin_space, win_sz);

    mark_cs = firfilt_rrrf_create(cos_mark, win_sz);
    mark_sn = firfilt_rrrf_create(sin_mark, win_sz);
    space_cs = firfilt_rrrf_create(cos_space, win_sz);
    space_sn = firfilt_rrrf_create(sin_space, win_sz);
    if(!mark_cs || !mark_sn || !space_cs || !space_sn) goto fail;

    // Stage 3.1 - "AGC" scaling for mark/space channels
    mark_max = space_max = 1;
    mark_min = space_min = -1;
    mark_buff = malloc(sizeof(float) * win_sz * 9);
    space_buff = malloc(sizeof(float) * win_sz * 9);
    if(!mark_buff || !space_buff) goto fail;
    memset(mark_buff, 0, sizeof(mark_buff));
    memset(space_buff, 0, sizeof(space_buff));

    // Stage 3: 1200 Hz filter for mark/space channel
    float As = 60.0f;
    float fc = (float)baud_rate / output_rate;
    float ft = (float)baud_rate / output_rate;
    float mu = 0.0f;
    const unsigned int df_len = estimate_req_filter_len(ft, As);
    data_filt_taps = malloc(sizeof(float) * df_len);
    if(!data_filt_taps) goto fail;

    // Design the 1200 Hz filter, scale to gain of 1.0 
    liquid_firdes_kaiser(df_len, fc, As, mu, data_filt_taps);
    normalize(data_filt_taps, df_len);

    mark_filt = firfilt_rrrf_create(data_filt_taps, df_len); 
    space_filt = firfilt_rrrf_create(data_filt_taps, df_len); 
    if(!mark_filt || !space_filt) goto fail;

    // Stage 4: Resample to 4 sps for clock recovery
    const int sps = 4;
    float bw2 = 2.0 * baud_rate / output_rate;
    float r2 = (float)sps * baud_rate / output_rate;
    sps2 = resamp_rrrf_create(r2, h_len, bw2, slsl, npfb);
    if(!sps2) goto fail;

    memset(sync_buff, 0, sizeof(sync_buff));
    sync_buff_idx = 0;

    // Stage 5: Clock Recovery
    sync = symsync_rrrf_create_rnyquist(LIQUID_FIRFILT_RRC, sps, 7, (4.0 * baud_rate / output_rate), 32);
    if(!sync) goto fail;

    // Stage 6: Differential decoding, HDLC
    last_bit = 0;
    hdlc_init(&hdlc);

    return true;

fail:
    dsp_destroy();
    return false;
}

bool dsp_process(float samp) {
    int num_written;
    float resamp[3];

    // Stage 1: Resample to 13200 Hz
    resamp_rrrf_execute(rs, samp, resamp, &num_written);

    for(int j = 0; j < num_written; j++) {
        float scale;

        // Stage 2: Update re/im correlators for mark/space
        float re, im;
        float mark_mag, space_mag;
        firfilt_rrrf_push(mark_cs, resamp[j]);
        firfilt_rrrf_execute(mark_cs, &re);
        firfilt_rrrf_push(mark_sn, resamp[j]);
        firfilt_rrrf_execute(mark_sn, &im);
        mark_mag = sqrtf(re * re + im * im);

        firfilt_rrrf_push(space_cs, resamp[j]);
        firfilt_rrrf_execute(space_cs, &re);
        firfilt_rrrf_push(space_sn, resamp[j]);
        firfilt_rrrf_execute(space_sn, &im);
        space_mag = sqrtf(re * re + im * im);

        // Stage 3: Filter mark signal
        firfilt_rrrf_push(mark_filt, mark_mag);
        firfilt_rrrf_execute(mark_filt, &mark_mag);
        mark_buff[buff_idx] = mark_mag;
        firfilt_rrrf_push(space_filt, space_mag);
        firfilt_rrrf_execute(space_filt, &space_mag);
        space_buff[buff_idx] = space_mag;

        buff_idx += 1;
        buff_idx %= ASIZE(mark_buff);

        // Stage 3.1: Calculate AGC scaling parameters
        if(buff_idx == 0) {
            minmax(mark_buff, ASIZE(mark_buff), &mark_min, &mark_max);
            minmax(space_buff, ASIZE(space_buff), &space_min, &space_max);
        }

        // Scale waveforms
        float mark_scale = 1.0 / (mark_max - mark_min);
        scale = (mark_scale > 10 ? 10 : mark_scale);
        mark_mag -= (mark_max + mark_min) / 2;
        mark_mag *= mark_scale;

        float space_scale = 1.0 / (space_max - space_min);
        scale = (space_scale > 10 ? 10 : space_scale);
        space_mag -= (space_max + space_min) / 2;
        space_mag *= space_scale;

        // Data = space - mark, differential waveforms
        float data_mag = space_mag - mark_mag;

        // Stage 4: Resample to 4 sps
        unsigned int num_written2;
        resamp_rrrf_execute(sps2, data_mag, &data_mag, &num_written2);
        if(num_written2 == 0) continue;

        // Stage 4.1 - Calculate correlation of flag (TODO: firfilt?)
        sync_buff[sync_buff_idx] = data_mag;
        sync_buff_idx = (sync_buff_idx + 1) % ASIZE(sync_buff);
        float sync_buff_sum = 0;
        for(size_t _sb = 0; _sb < ASIZE(sync_buff); _sb++) {
            sync_buff_sum += sync_search[_sb] * sync_buff[(sync_buff_idx + _sb) % ASIZE(sync_buff)];
        }
        sync_buff_sum /= 26.0f;
        sync_buff_sum = fabs(sync_buff_sum);

        // Stage 5 - Clock Recovery
        float bit;
        symsync_rrrf_execute(sync, &data_mag, 1, &bit, &num_written2);
        if(num_written2 == 0) continue;
        //float tau = symsync_rrrf_get_tau(sync);

        // Stage 6 - Differential Decoding
        float nrzi = fabs(bit);
        if((bit < 0 && last_bit >= 0) || (bit >= 0 && last_bit < 0)) {
            nrzi *= -1;
        }
        last_bit = bit;
        bit = nrzi;

        size_t frame_len;
        bool got_frame = hdlc_execute(&hdlc, nrzi, &frame_len);
        if(!got_frame) continue;
        if((frame_len % 8) != 0) continue; // Discard packets with non multiple of 8 len

        bool got_pkt = false;
        if(crc16_ccitt((float *)&hdlc.samps, frame_len)) {
            got_pkt = true;
        } else {
            flip_smallest((float *)&hdlc.samps, frame_len);
            if(crc16_ccitt((float *)&hdlc.samps, frame_len)) {
                got_pkt = true;
            }
        }

        return got_pkt;
    }

    return false;
}

void dsp_destroy(void) {
    if(rs) resamp_rrrf_destroy(rs);
    if(sps2) resamp_rrrf_destroy(sps2);

    if(mark_cs) firfilt_rrrf_destroy(mark_cs);
    if(mark_sn) firfilt_rrrf_destroy(mark_sn);
    if(space_cs) firfilt_rrrf_destroy(space_cs);
    if(space_sn) firfilt_rrrf_destroy(space_sn);
    if(mark_filt) firfilt_rrrf_destroy(mark_filt);
    if(space_filt) firfilt_rrrf_destroy(space_filt);
    if(sync) symsync_rrrf_destroy(sync);

    if(cos_mark) free(cos_mark);
    if(sin_mark) free(sin_mark);
    if(cos_space) free(cos_space);
    if(sin_space) free(sin_space);
    if(mark_buff) free(mark_buff);
    if(space_buff) free(space_buff);
    if(data_filt_taps) free(data_filt_taps);
}

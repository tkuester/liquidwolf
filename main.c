#include <stdio.h>
#include <string.h>
#include <math.h>

#include <liquid/liquid.h>

#define ASIZE(x) (sizeof(x) / sizeof(x[0]))

void minmax(float *buff, size_t len, float *min, float *max);

void minmax(float *buff, size_t len, float *min, float *max) {
    *max = -INFINITY;
    *min = INFINITY;

    for(size_t i = 0; i < len; i++) {
        if(buff[i] > *max) *max = buff[i];
        if(buff[i] < *min) *min = buff[i];
    }
}

int main(int argc, char **argv) {
    int rc = 1;

    const int baud_rate = 1200;
    const int mark = 1200;
    const int space = 2200;

    FILE *fp = NULL;
    int input_rate = 44100;
    int output_rate = 13200; // (1200: 11, 2200: 6)
    float samps[1024];
    size_t read;
    size_t idx;

    unsigned int h_len = 13;
    float r = 1.0f * output_rate / input_rate;
    float bw = 0.25f;
    float slsl = 60.0f;
    unsigned int npfb = 32;
    resamp_rrrf rs = NULL;

    const unsigned int n = (unsigned int)ceilf(r);
    float resamp[n];
    unsigned int num_written;

    FILE *fout;

    const int win_sz = (int)ceilf(1.0f * output_rate / baud_rate);
    float win[win_sz];
    float cos_mark[win_sz];
    float sin_mark[win_sz];
    float cos_space[win_sz];
    float sin_space[win_sz];

    firfilt_rrrf mark_cs, mark_sn;
    firfilt_rrrf space_cs, space_sn;

    float mark_buff[win_sz * 8], space_buff[win_sz * 8];
    int buff_idx = 0;
    float mark_max = 1, mark_min = -1, space_max = 1, space_min = -1;

    firfilt_rrrf mark_filt, space_filt;
    float fc = 0.12f;
    float ft = 0.08f;
    float As = 60.0f;
    float mu = 0.0f;
    const unsigned int df_len = estimate_req_filter_len(ft, As);
    float data_filt_taps[df_len];
    liquid_firdes_kaiser(df_len, fc, As, mu, data_filt_taps);

    float _kai_scale = 0;
    for(int i = 0; i < df_len; i++) {
        _kai_scale += data_filt_taps[i];
    }
    printf("kai_scale: %f\n", _kai_scale);
    for(int i = 0; i < df_len; i++) {
        data_filt_taps[i] /= _kai_scale;
    }

    mark_filt = firfilt_rrrf_create(data_filt_taps, df_len); 
    space_filt = firfilt_rrrf_create(data_filt_taps, df_len); 

    memset(mark_buff, 0, ASIZE(mark_buff));
    memset(space_buff, 0, ASIZE(space_buff));

    // Generate filter taps
    for(int i = 0; i < win_sz; i++) {
        win[i] = sin(1.0f * M_PI * i / (win_sz - 1));
        cos_mark[i] = cosf(1.0f * 2 * M_PI * mark * i / output_rate) * win[i];
        sin_mark[i] = sinf(1.0f * 2 * M_PI * mark * i / output_rate) * win[i];
        cos_space[i] = cosf(1.0f * 2 * M_PI * space * i / output_rate) * win[i];
        sin_space[i] = sinf(1.0f * 2 * M_PI * space * i / output_rate) * win[i];
    }

    if(argc != 3) {
        fprintf(stderr, "Usage: %s in.f32 out.f32\n", argv[0]);
        return 1;
    }

    fp = fopen(argv[1], "r");
    if(fp == NULL) {
        fprintf(stderr, "Can't open file %s for reading\n", argv[1]);
        goto fail;
    }

    fout = fopen(argv[2], "w");
    if(fout == NULL) {
        fprintf(stderr, "Can't open file %s for writing\n", argv[2]);
        goto fail;
    }

    rs = resamp_rrrf_create(r, h_len, bw, slsl, npfb);
    if(rs == NULL) goto fail;

    mark_cs = firfilt_rrrf_create(cos_mark, win_sz);
    mark_sn = firfilt_rrrf_create(sin_mark, win_sz);

    space_cs = firfilt_rrrf_create(cos_space, win_sz);
    space_sn = firfilt_rrrf_create(sin_space, win_sz);

    if(mark_cs == NULL || mark_sn == NULL || space_cs == NULL || space_sn == NULL) goto fail;

    idx = 0;

    float mark_scale, space_scale;
    while(1) {
        read = fread(&samps, sizeof(float), ASIZE(samps), fp);
        if(read != ASIZE(samps)) {
            if(ferror(fp)) {
                fprintf(stderr, "Read error: %d\n", ferror(fp));
                goto fail;
            }
        }

        for(int i = 0; i < read; i++) {
            resamp_rrrf_execute(rs, samps[i], resamp, &num_written);
            for(int j = 0; j < num_written; j++) {
                float re, im;
                float mark_mag, space_mag;
                float scale;

                firfilt_rrrf_push(mark_cs, resamp[j]);
                firfilt_rrrf_push(mark_sn, resamp[j]);
                firfilt_rrrf_execute(mark_cs, &re);
                firfilt_rrrf_execute(mark_sn, &im);
                mark_mag = sqrtf(re * re + im * im);
                firfilt_rrrf_push(mark_filt, mark_mag);
                firfilt_rrrf_execute(mark_filt, &mark_mag);
                mark_buff[buff_idx] = mark_mag;

                firfilt_rrrf_push(space_cs, resamp[j]);
                firfilt_rrrf_push(space_sn, resamp[j]);
                firfilt_rrrf_execute(space_cs, &re);
                firfilt_rrrf_execute(space_sn, &im);
                space_mag = sqrtf(re * re + im * im);
                firfilt_rrrf_push(space_filt, space_mag);
                firfilt_rrrf_execute(space_filt, &space_mag);
                space_buff[buff_idx] = space_mag;

                buff_idx += 1;
                buff_idx %= ASIZE(mark_buff);

                if(buff_idx == 0) {
                    minmax(mark_buff, ASIZE(mark_buff), &mark_min, &mark_max);
                    minmax(space_buff, ASIZE(space_buff), &space_min, &space_max);
                }

                scale = 1 / (mark_max - mark_min);
                scale = (scale > 10 ? 10 : scale);
                mark_mag -= (mark_max + mark_min) / 2;
                mark_mag *= scale;
                fwrite(&mark_mag, sizeof(float), 1, fout);

                scale = 1 / (space_max - space_min);
                scale = (scale > 10 ? 10 : scale);
                space_mag -= (space_max + space_min) / 2;
                space_mag *= scale;
                fwrite(&space_mag, sizeof(float), 1, fout);

                space_mag -= mark_mag;
                fwrite(&space_mag, sizeof(float), 1, fout);

                idx += 1;
            }
        }

        if(read != ASIZE(samps)) {
            printf("Done\n");
            break;
        }
    }

    rc = 0;

fail:
    if(fp) {
        fclose(fp);
        fp = NULL;
    }

    if(fout) {
        fclose(fout);
        fout = NULL;
    }

    if(rs) {
        resamp_rrrf_destroy(rs);
        rs = NULL;
    }

    return rc;
}

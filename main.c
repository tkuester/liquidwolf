#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <liquid/liquid.h>

#define ASIZE(x) (sizeof(x) / sizeof(x[0]))

void minmax(float *buff, size_t len, float *min, float *max);
float median(float *buff, float *scratch, size_t len);

void minmax(float *buff, size_t len, float *min, float *max) {
    *max = -INFINITY;
    *min = INFINITY;

    for(size_t i = 0; i < len; i++) {
        if(buff[i] > *max) *max = buff[i];
        if(buff[i] < *min) *min = buff[i];
    }
}

float median(float *buff, float *scratch, size_t len) {
    memcpy(scratch, buff, sizeof(float) * len);

    size_t i, j;
    float key;

    for(i = 1; i < len; i++) {
        key = scratch[i];
        j = i - 1;

        while(j >= 0 && scratch[j] > key) {
            scratch[j+1] = scratch[j];
            j -= 1;
        }
        scratch[j + 1] = key;
    }

    return scratch[len / 2];
}

int main(int argc, char **argv) {
    int rc = 1;
    struct timeval tv_start, tv_done;

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

    float mark_buff[win_sz * 8], space_buff[win_sz * 8], data_buff[win_sz * 8];
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

    int sps = output_rate / baud_rate;
    symsync_rrrf sync = symsync_rrrf_create_rnyquist(LIQUID_FIRFILT_RRC, sps, 21, 0.3f, 32); 

    float _kai_scale = 0;
    for(int i = 0; i < df_len; i++) {
        _kai_scale += data_filt_taps[i];
    }
    for(int i = 0; i < df_len; i++) {
        data_filt_taps[i] /= _kai_scale;
    }

    mark_filt = firfilt_rrrf_create(data_filt_taps, df_len); 
    space_filt = firfilt_rrrf_create(data_filt_taps, df_len); 

    float mark_del_buff[5], space_del_buff[5], scratch_del_buff[5];
    int del_buff_idx = 0;

    memset(mark_buff, 0, ASIZE(mark_buff));
    memset(space_buff, 0, ASIZE(space_buff));
    memset(mark_del_buff, 0, ASIZE(mark_del_buff));
    memset(space_del_buff, 0, ASIZE(space_del_buff));

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
    gettimeofday(&tv_start, NULL);
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

                // Update re/im correlators @ mark freq
                // calculate magnitude of re+im
                // lpf magnitude, store into buffer
                firfilt_rrrf_push(mark_cs, resamp[j]);
                firfilt_rrrf_push(mark_sn, resamp[j]);
                firfilt_rrrf_execute(mark_cs, &re);
                firfilt_rrrf_execute(mark_sn, &im);
                mark_mag = sqrtf(re * re + im * im);
                firfilt_rrrf_push(mark_filt, mark_mag);
                firfilt_rrrf_execute(mark_filt, &mark_mag);
                mark_buff[buff_idx] = mark_mag;

                // Update re/im correlators @ space freq
                // calculate magnitude of re+im
                // lpf magnitude, store into buffer
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

                // Scale waveforms
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

                // Data = space - mark, differential waveforms
                space_mag -= mark_mag;
                data_buff[buff_idx] = space_mag;
                fwrite(&space_mag, sizeof(float), 1, fout);

                mark_mag = mark_buff[buff_idx] - mark_buff[buff_idx == 0 ? ASIZE(mark_buff) : buff_idx - 1];
                space_mag = space_buff[buff_idx] - space_buff[buff_idx == 0 ? ASIZE(space_buff) : buff_idx - 1];
                mark_del_buff[del_buff_idx] = fabsf(mark_mag);
                space_del_buff[del_buff_idx] = fabsf(space_mag);
                del_buff_idx = (del_buff_idx + 1) % ASIZE(mark_del_buff);
                mark_mag = median(mark_del_buff, scratch_del_buff, ASIZE(mark_del_buff));
                space_mag = median(space_del_buff, scratch_del_buff, ASIZE(space_del_buff));
                fwrite(&mark_mag, sizeof(float), 1, fout);
                fwrite(&space_mag, sizeof(float), 1, fout);

                // Clock recovery
                /*
                unsigned int tmp;
                symsync_rrrf_execute(sync, &space_mag, 1, &mark_mag, &tmp);
                if(tmp == 0) {
                    mark_mag = 0;
                }
                fwrite(&mark_mag, sizeof(float), 1, fout);
                */

                idx += 1;
            }
        }

        if(read != ASIZE(samps)) {
            printf("Done\n");
            break;
        }
    }

    gettimeofday(&tv_done, NULL);

    float samp_per_sec = idx / ((tv_done.tv_sec + tv_done.tv_usec / 1e6) - (tv_start.tv_sec + tv_start.tv_usec / 1e6));
    printf("%d samp / sec\n", (int)samp_per_sec);
    printf("%.1fx speed\n", samp_per_sec / input_rate);

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

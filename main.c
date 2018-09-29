#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <liquid/liquid.h>
#include <sndfile.h>

#include "hdlc.h"
#include "util.h"

int main(int argc, char **argv) {
    int rc = 1;
    struct timeval tv_start, tv_done;

    char *wavfile = NULL;
    SNDFILE *sf;
    FILE *fout;

    const int baud_rate = 1200;
    const int mark = 1200;
    const int space = 2200;

    int input_rate;
    const int output_rate = 13200; // (1200: 11, 2200: 6)
    const size_t SAMPS_SIZE = 1024;
    float *samps;
    size_t read;
    size_t idx;

    unsigned int h_len = 13;
    float r;
    float bw = 1200.0f / output_rate;
    float slsl = 60.0f;
    unsigned int npfb = 32;
    resamp_rrrf rs = NULL;

    const int sps = 4;
    resamp_rrrf sps2 = NULL;
    float r2 = (float)sps * baud_rate / output_rate;
    float bw2 = 2400.0f / output_rate;

    unsigned int n;
    float resamp[100];
    unsigned int num_written;

    const int win_sz = (int)ceilf(1.0f * output_rate / baud_rate);
    float win[win_sz];
    float cos_mark[win_sz];
    float sin_mark[win_sz];
    float cos_space[win_sz];
    float sin_space[win_sz];

    firfilt_rrrf mark_cs, mark_sn;
    firfilt_rrrf space_cs, space_sn;

    float mark_buff[win_sz * 9], space_buff[win_sz * 9];
    int buff_idx = 0;
    float mark_max = 1, mark_min = -1, space_max = 1, space_min = -1;

    firfilt_rrrf mark_filt, space_filt;
    float fc = 1200.0f / output_rate;
    float ft = 1200.0f / output_rate;
    float As = 60.0f;
    float mu = 0.0f;
    const unsigned int df_len = estimate_req_filter_len(ft, As);
    float data_filt_taps[df_len];

    symsync_rrrf sync;
    hdlc_state_t hdlc;

    float _kai_scale = 0;

    float bit = 0;
    float last_bit = 0;

    size_t num_packets = 0;
    size_t num_one_flip_packets = 0;

    // Calc'd at 4 sps
    float sync_search[] = {-0.327787071466, -0.863485574722, -0.846625030041, -0.237066477537,
                           0.509203135967, 0.92675024271, 0.967760741711, 0.90081679821,
                           0.890751242638, 0.906954228878, 0.90988856554, 0.908331274986,
                           0.909126937389, 0.910285055637, 0.910628736019, 0.910407721996,
                           0.910471260548, 0.911236822605, 0.911468148232, 0.911164462566,
                           0.91139292717, 0.912554085255, 0.912695467472, 0.911213994026,
                           0.910359799862, 0.912545442581, 0.908676445484, 0.896155297756,
                           0.916578114033, 0.974584817886, 0.887850761414, 0.431282520294,
                           -0.297088474035, -0.840644657612, -0.802261173725, -0.211893334985};
    float sync_buff[ASIZE(sync_search)] = {0};
    size_t sync_buff_idx = 0;
    float sync_buff_sum;
    bool sync_lock = false;

    if(argc != 3) {
        fprintf(stderr, "Usage: %s in.f32 out.f32\n", argv[0]);
        return 1;
    }

    wavfile = argv[1];

    SF_INFO sfinfo;
    sf = sf_open(wavfile, SFM_READ, &sfinfo);
    if(sf == NULL) {
        fprintf(stderr, "Can't open file %s for reading\n", wavfile);
        goto fail;
    }

    if(sfinfo.channels > 2) {
        fprintf(stderr, "Sorry, can only work with 1 or 2 channel wav files\n");
        goto fail;
    }

    printf("Opened %s: %d Hz, %d chan\n", wavfile, sfinfo.samplerate, sfinfo.channels);

    fout = fopen(argv[2], "w");
    if(fout == NULL) {
        fprintf(stderr, "Can't open file %s for writing\n", argv[2]);
        goto fail;
    }

    input_rate = sfinfo.samplerate;
    samps = malloc(sizeof(float) * SAMPS_SIZE * sfinfo.channels);
    if(samps == NULL) {
        goto fail;
    }

    r = 1.0f * output_rate / input_rate;
    n = (unsigned int)ceilf(r);
    if(n > ASIZE(resamp)) {
        goto fail;
    }

    liquid_firdes_kaiser(df_len, fc, As, mu, data_filt_taps);
    sync = symsync_rrrf_create_rnyquist(LIQUID_FIRFILT_RRC, sps, 7, (4800.0f / output_rate), 32);

    _kai_scale = 0;
    for(int i = 0; i < df_len; i++) {
        _kai_scale += data_filt_taps[i];
    }
    for(int i = 0; i < df_len; i++) {
        data_filt_taps[i] /= _kai_scale;
    }

    mark_filt = firfilt_rrrf_create(data_filt_taps, df_len); 
    space_filt = firfilt_rrrf_create(data_filt_taps, df_len); 

    hdlc_init(&hdlc);

    memset(mark_buff, 0, sizeof(mark_buff));
    memset(space_buff, 0, sizeof(space_buff));

    // Generate filter taps
    for(int i = 0; i < win_sz; i++) {
        win[i] = sin(1.0f * M_PI * i / (win_sz - 1));
        cos_mark[i] = cosf(2 * M_PI * mark * i / output_rate) * win[i];
        sin_mark[i] = sinf(2 * M_PI * mark * i / output_rate) * win[i];
        cos_space[i] = cosf(2 * M_PI * space * i / output_rate) * win[i];
        sin_space[i] = sinf(2 * M_PI * space * i / output_rate) * win[i];
    }

    rs = resamp_rrrf_create(r, h_len, bw, slsl, npfb);
    sps2 = resamp_rrrf_create(r2, h_len, bw2, slsl, npfb);
    if(rs == NULL) goto fail;

    mark_cs = firfilt_rrrf_create(cos_mark, win_sz);
    mark_sn = firfilt_rrrf_create(sin_mark, win_sz);

    space_cs = firfilt_rrrf_create(cos_space, win_sz);
    space_sn = firfilt_rrrf_create(sin_space, win_sz);

    if(mark_cs == NULL || mark_sn == NULL || space_cs == NULL || space_sn == NULL) goto fail;

    idx = 0;

    gettimeofday(&tv_start, NULL);
    while(1) {
        //read = fread(&samps, sizeof(float), ASIZE(samps), fp);
        read = sf_read_float(sf, samps, SAMPS_SIZE);
        if(read != SAMPS_SIZE) {
            if(sf_error(sf)) {
                fprintf(stderr, "Read error: %d\n", sf_error(sf));
                goto fail;
            }
        }

        for(int i = 0; i < read; i += sfinfo.channels) {
            resamp_rrrf_execute(rs, samps[i], resamp, &num_written);

            for(int j = 0; j < num_written; j++) {
                float re, im;
                float mark_mag, space_mag, data_mag;
                float scale;

                fwrite(&resamp[j], sizeof(float), 1, fout);

                // Update re/im correlators @ mark freq
                // calculate magnitude of re+im
                // lpf magnitude, store into buffer
                firfilt_rrrf_push(mark_cs, resamp[j]);
                firfilt_rrrf_execute(mark_cs, &re);
                firfilt_rrrf_push(mark_sn, resamp[j]);
                firfilt_rrrf_execute(mark_sn, &im);
                mark_mag = sqrtf(re * re + im * im);

                firfilt_rrrf_push(mark_filt, mark_mag);
                firfilt_rrrf_execute(mark_filt, &mark_mag);
                mark_buff[buff_idx] = mark_mag;

                // Update re/im correlators @ space freq
                // calculate magnitude of re+im
                // lpf magnitude, store into buffer
                firfilt_rrrf_push(space_cs, resamp[j]);
                firfilt_rrrf_execute(space_cs, &re);
                firfilt_rrrf_push(space_sn, resamp[j]);
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
                data_mag = space_mag - mark_mag;
                //fwrite(&data_mag, sizeof(float), 1, fout);

                unsigned int tmp;
                resamp_rrrf_execute(sps2, data_mag, &data_mag, &tmp);
                if(tmp > 1) {
                    printf("Uhoh \n");
                }
                else if(tmp == 0) {
                    data_mag = 0;
                }
                fwrite(&data_mag, sizeof(float), 1, fout);

                if(tmp == 1) {
                    sync_buff[sync_buff_idx] = data_mag;
                    sync_buff_idx = (sync_buff_idx + 1) % ASIZE(sync_buff);
                    sync_buff_sum = 0;
                    for(size_t _sb = 0; _sb < ASIZE(sync_buff); _sb++) {
                        sync_buff_sum += sync_search[_sb] * sync_buff[(sync_buff_idx + _sb) % ASIZE(sync_buff)];
                    }
                    sync_buff_sum /= 26.0f;
                    sync_buff_sum = fabs(sync_buff_sum);
                    fwrite(&sync_buff_sum, sizeof(float), 1, fout);
                } else {
                    fwrite(&data_mag, sizeof(float), 1, fout);
                }

                bool got_pkt = false;
                bit = 0;
                if(tmp == 1) {
                    symsync_rrrf_execute(sync, &data_mag, 1, &bit, &tmp);
                    if(tmp == 1) {
                        float nrzi;
                        if((bit < 0 && last_bit >= 0) || (bit >= 0 && last_bit < 0)) {
                            nrzi = fabs(bit) * -1.0f;
                        } else {
                            nrzi = fabs(bit);
                        }
                        last_bit = bit;
                        bit = nrzi;
                        size_t len;
                        //printf("%d ", (nrzi >= 0 ? 1 : 0));
                        if(hdlc_execute(&hdlc, nrzi, &len)) {
                            //printf("hdlc_exec returned true with %zu len\n", len);
                            if((len % 8) == 0) {
								if(crc16_ccitt((float *)&hdlc.samps, len)) {
                                    num_packets += 1;
                                    got_pkt = true;
                                } else {
                                    flip_smallest((float *)&hdlc.samps, len);
                                    if(crc16_ccitt((float *)&hdlc.samps, len)) {
                                        num_one_flip_packets += 1;
                                        num_packets += 1;
                                        got_pkt = true;
                                    }
                                }
                            }
                        }
                        /*
                         * Doesn't quite help
                        if(hdlc.in_packet && hdlc.buff_idx >= 64 && !sync_lock) {
                            symsync_rrrf_lock(sync);
                            sync_lock = true;
                        } else {
                            if(sync_lock) {
                                symsync_rrrf_unlock(sync);
                                sync_lock = false;
                            }
                        }
                        */
                        //hdlc_debug(&hdlc);
                    }
                }
                fwrite(&bit, sizeof(float), 1, fout);

                float tau = symsync_rrrf_get_tau(sync);
                fwrite(&tau, sizeof(float), 1, fout);

                tau = got_pkt ? 0.8 : 0;
                fwrite(&tau, sizeof(float), 1, fout);

                idx += 1;
            }
        }

        if(read != SAMPS_SIZE) {
            printf("Done\n");
            break;
        }
    }

    gettimeofday(&tv_done, NULL);

    float samp_per_sec = idx / ((tv_done.tv_sec + tv_done.tv_usec / 1e6) - (tv_start.tv_sec + tv_start.tv_usec / 1e6));
    printf("Processed %zu samples\n", idx);
    printf("%d samp / sec\n", (int)samp_per_sec);
    printf("%.1fx speed\n", samp_per_sec / input_rate);
    printf("%zu packets\n", num_packets);
    printf("%zu one flip packets\n", num_one_flip_packets);

    rc = 0;

fail:
    if(samps != NULL) {
        free(samps);
        samps = NULL;
    }

    if(sf) {
        sf_close(sf);
        sf = NULL;
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

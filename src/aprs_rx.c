#include "aprs_rx.h"

aprs_rx_t *aprs_rx_init(void) {
    aprs_rx_t *ret = NULL;
    ret = malloc(sizeof(aprs_rx_t) * 1);
    if(!ret) goto fail;

    memset(ret, 0, sizeof(aprs_rx_t));

    ret->samps_len = SAMPS_SIZE;
    ret->samps = malloc(sizeof(float) * ret->samps_len);
    if(!ret->samps) {
        fprintf(stderr, "malloc\n");
        goto fail;
    }

    hdlc_init(&ret->hdlc);

    return ret;

fail:
    aprs_rx_destroy(ret);
    return NULL;
}

bool aprs_rx_wav_open(aprs_rx_t *rx, const char *wavfile) {
    if(!rx) return false;

    // TODO: Check if the structure is already init'd
    if(wav_open(wavfile, &rx->src.wav) != 0) goto fail;

    if(!bell202_init(&rx->modem, rx->src.wav.samplerate)) {
        fprintf(stderr, "Unable to init DSP structures\n");
        goto fail;
    }

    rx->src_type = SOURCE_WAV;
    rx->read_func = &wav_read;

    return true;

fail:
    return false;
}

bool aprs_rx_stdin_open(aprs_rx_t *rx) {
    if(!rx) return false;

    if(!bell202_init(&rx->modem, 24000)) {
        fprintf(stderr, "Unable to init DSP structures\n");
        return false;
    }

    rx->src_type = SOURCE_STDIN;
    rx->read_func = &stdin_read;
    return true;
}

void aprs_rx_process(aprs_rx_t *rx) {
    gettimeofday(&rx->tv_start, NULL);
    while(1) {
        // TODO: Use function pointers, call rx->read_func()
        ssize_t read;
        read = rx->read_func(&rx->src, rx->samps, rx->samps_len);

        for(ssize_t i = 0; i < read; i += 1) {
            rx->num_samps += 1;

            float out_bit;
            if(!bell202_process(&rx->modem, rx->samps[i], &out_bit)) continue;

            size_t frame_len;
            bool got_frame = hdlc_execute(&rx->hdlc, out_bit, &frame_len);
            if(!got_frame) continue;
            if((frame_len % 8) != 0) continue; // Discard packets with non multiple of 8 len

            bool got_pkt = false;
            ax25_pkt_t pkt;
            got_pkt = do_you_wanna_build_a_packet(&pkt, (float *)&rx->hdlc.samps, frame_len);

            if(got_pkt) rx->num_packets += 1;
        }

        if(read != rx->samps_len) {
            printf("Done\n");
            break;
        }
    }
    gettimeofday(&rx->tv_done, NULL);
}

void aprs_rx_destroy(aprs_rx_t *rx) {
    if(!rx) return;

    if(rx->num_samps > 0) {
        float samp_per_sec = rx->num_samps / ((rx->tv_done.tv_sec + rx->tv_done.tv_usec / 1e6) - (rx->tv_start.tv_sec + rx->tv_start.tv_usec / 1e6));
        fprintf(stderr, "Processed %zu samples\n", rx->num_samps);
        fprintf(stderr, "%d samp / sec\n", (int)samp_per_sec);
        fprintf(stderr, "%.1fx speed\n", samp_per_sec / rx->src.wav.info.samplerate);
        fprintf(stderr, "%zu packets\n", rx->num_packets);
        fprintf(stderr, "%zu one flip packets\n", rx->num_one_flip_packets);
    }

    if(rx->samps) free(rx->samps);
    switch(rx->src_type) {
        case SOURCE_WAV:
            wav_close(&rx->src.wav);
            break;

        default:
            break;
    }
    bell202_destroy(&rx->modem);
    free(rx);
    rx = NULL;
}

void flip_smallest(float *data, size_t len) {
    float min = INFINITY;
    size_t min_idx = 0;

    for(size_t j = 0; j < len; j++) {
        if(fabs(data[j]) < min) {
            min_idx = j;
            min = fabs(data[j]);
        }
    }

    data[min_idx] *= -1;
    data[min_idx] += (data[min_idx] >= 0 ? 1 : -1);
}

bool do_you_wanna_build_a_packet(ax25_pkt_t *pkt, float *buff, size_t len) {
    if(!pkt) return false;
    if(!buff) return false;

    // TODO: Min frame length: 136 bits?
    if(len < 32) return false;
    if((len % 8) != 0) return false;
    if(len > (4096 * 8)) return false;

    uint8_t data[4096];
    float qual;

    size_t pktlen = bit_buff_to_bytes(buff, len, data, 4096, &qual);

    uint16_t ret = hdlc_crc(data, pktlen - 2);
    uint16_t crc = data[pktlen - 1] << 8 | data[pktlen - 2];

    if(ret != crc) {
        // Toggle 1 bit for the packet
        for(size_t i = 0; i < pktlen; i++) {
            for(size_t j = 0; j < 8; j++) {
                data[i] ^= (1 << j);
                ret = hdlc_crc(data, pktlen - 2);
                crc = data[pktlen - 1] << 8 | data[pktlen - 2];
                if(ret == crc) goto done;
                // If the flip didn't get us a packet, reset things
                data[i] ^= (1 << j);
            }
        }

        // Toggle 2 bits for the packet
        for(size_t i = 0; i < pktlen; i++) {
            for(size_t j = 0; j < 8; j++) {
                // If we're not on the last byte
                if(j == 7 && i != (pktlen - 1)) {
                    // Toggle the bit between bytes
                    data[i] ^= 0x80;
                    data[i + 1] ^= 0x01;
                } else {
                    // xor-ing the last byte with 0x180 is the same
                    // as xor-ing with 0x80
                    data[i] ^= (3 << j);
                }
                ret = hdlc_crc(data, pktlen - 2);
                crc = data[pktlen - 1] << 8 | data[pktlen - 2];
                if(ret == crc) goto done;
                if(j == 7 && i != (pktlen - 1)) {
                    data[i] ^= 0x80;
                    data[i + 1] ^= 0x01;
                } else {
                    data[i] ^= (3 << j);
                }
            }
        }
    }

done:
    if(ret == crc) {
        int unpacked_ok = ax25_pkt_unpack(pkt, data, pktlen - 2);
        if(unpacked_ok == 0) {
            printf("Quality: %.2f\n", qual);
            hexdump(stdout, data, pktlen);
            ax25_pkt_dump(stdout, pkt);
            printf("================================\n");
        }
    }
    return ret == crc;
}


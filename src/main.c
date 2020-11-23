#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <liquid/liquid.h>

#include "bell202.h"
#include "hdlc.h"
#include "ax25.h"
#include "util.h"
#include "wav_src.h"

#ifndef _LIQUIDWOLF_VERSION
#define _LIQUIDWOLF_VERSION "(unversioned)"
#endif

#define SAMPS_SIZE (1024)

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

int main(int argc, char **argv) {
    int rc = 1;
    struct timeval tv_start, tv_done;

    char *wavfile = NULL;
    wav_t wav;

    float *samps = NULL;
    bell202_t modem;
    hdlc_state_t hdlc;

    size_t num_samps = 0;
    size_t num_packets = 0;
    size_t num_one_flip_packets = 0;

    printf("LiquidWolf %s\n", _LIQUIDWOLF_VERSION);

    if(argc != 2) {
        fprintf(stderr, "Usage: %s in.wav\n", argv[0]);
        return 1;
    }

    samps = malloc(sizeof(float) * SAMPS_SIZE * 1024);
    if(!samps) {
        fprintf(stderr, "malloc\n");
        goto fail;
    }

    wavfile = argv[1];
    if(wav_open(wavfile, &wav) != 0) goto fail;

    if(!bell202_init(&modem, wav.samplerate)) {
        fprintf(stderr, "Unable to init DSP structures\n");
        goto fail;
    }

    hdlc_init(&hdlc);

    gettimeofday(&tv_start, NULL);
    while(1) {
        ssize_t read = wav_read(&wav, samps, SAMPS_SIZE);

        for(ssize_t i = 0; i < read; i += 1) {
            num_samps += 1;

            float out_bit;
            if(!bell202_process(&modem, samps[i], &out_bit)) continue;

            size_t frame_len;
            bool got_frame = hdlc_execute(&hdlc, out_bit, &frame_len);
            if(!got_frame) continue;
            if((frame_len % 8) != 0) continue; // Discard packets with non multiple of 8 len

            bool got_pkt = false;
            ax25_pkt_t pkt;
            got_pkt = do_you_wanna_build_a_packet(&pkt, (float *)&hdlc.samps, frame_len);

            if(got_pkt) num_packets += 1;
        }

        if(read != SAMPS_SIZE) {
            printf("Done\n");
            break;
        }
    }

    gettimeofday(&tv_done, NULL);

    float samp_per_sec = num_samps / ((tv_done.tv_sec + tv_done.tv_usec / 1e6) - (tv_start.tv_sec + tv_start.tv_usec / 1e6));
    printf("Processed %zu samples\n", num_samps);
    printf("%d samp / sec\n", (int)samp_per_sec);
    printf("%.1fx speed\n", samp_per_sec / wav.info.samplerate);
    printf("%zu packets\n", num_packets);
    printf("%zu one flip packets\n", num_one_flip_packets);

    rc = 0;

fail:
    if(samps) free(samps);
    wav_close(&wav);
    bell202_destroy(&modem);
    return rc;
}

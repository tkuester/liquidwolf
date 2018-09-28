#include <stdio.h>
#include <string.h>
#include <math.h>

#include "util.h"
#include "hdlc.h"
#include "ax25.h"

void hdlc_debug(hdlc_state_t *state) {
    printf("hdlc_state(in_packet=%5s, one_count=%3zu, buff_idx=%3zu)\n",
            state->in_packet ? "true" : "false",
            state->one_count,
            state->buff_idx);
}

void hdlc_init(hdlc_state_t *state) {
    state->in_packet = false;
    state->one_count = 0;
    state->buff_idx = 0;
}

bool hdlc_execute(hdlc_state_t *state, float samp, size_t *len) {
    bool bit = samp >= 0;

    if(bit) {
        state->one_count += 1;

        // Illegal state
        if(state->one_count > 6) {
            state->in_packet = false;
            return false;
        } else if(state->one_count == 6) {
            if(state->in_packet) {
                state->in_packet = false;

                if(state->buff_idx > 6) {
                    *len = state->buff_idx - 6;
                    state->buff_idx = 0;
                    return true;
                }
                state->buff_idx = 0;
                return false;
            }
        }
    } else {
        size_t _one_count = state->one_count;

        // Reset the # of consecutive 1's
        state->one_count = 0;
        
        // Receiving a frame deliminter
        if(_one_count == 6) {
            state->in_packet = true;
            state->buff_idx = 0;
            return false;
        }

        // Receiving a stuffed bit
        else if(_one_count == 5) {
            return false;
        }
    }

    if(state->in_packet) {
        if(state->buff_idx < HDLC_SAMP_BUFF_LEN) {
            state->samps[state->buff_idx] = samp;
            state->buff_idx += 1;
        } else {
            state->buff_idx = 0;
            state->in_packet = false;
            fprintf(stderr, "Uhoh, squelch got left open.\n");
        }
    }

    return false;
}

bool crc16_ccitt(const float *buff, size_t len) {
    // TODO: Min frame length: 136 bits?
    if(len < 32) return false;
    if((len % 8) != 0) return false;
    if(len > (4096 * 8)) return false;

    uint8_t data[4096];
    float qual;
    ax25_pkt_t pkt;

    size_t pktlen = bit_buff_to_bytes(buff, len, data, 4096, &qual);

    uint16_t ret = calc_crc(data, pktlen - 2);
    uint16_t crc = data[pktlen - 1] << 8 | data[pktlen - 2];

    if(ret == crc) {
        bool unpacked_ok = unpack_ax25(&pkt, data, pktlen);
        hexdump(stdout, data, pktlen);
        printf("Unpacked: %s\n", unpacked_ok ? "ok" : "err");
        dump_pkt(&pkt);
        printf("Got packet with %zu samps\n", len);
        printf("Quality: %.2f\n", (1.0f * qual / len));
        printf("calc'd crc = 0x%04x\n", ret);
        printf("pkt crc    = 0x%04x\n", crc);
        printf("================================\n");
    }
    return ret == crc;
}

uint16_t calc_crc(uint8_t *data, size_t len) {
    unsigned int POLY=0x8408; //reflected 0x1021
    unsigned short crc=0xFFFF;
    for(size_t i=0; i<len; i++) {
        crc ^= data[i];
        for(size_t j=0; j<8; j++) {
            if(crc&0x01) crc = (crc >> 1) ^ POLY;
            else         crc = (crc >> 1);
        }
    }
    return crc ^ 0xFFFF;
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

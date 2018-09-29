#include <stdio.h>
#include <string.h>
#include <math.h>

#include "util.h"

void hexdump(FILE *fp, const uint8_t *buff, size_t len) {
    size_t i;
    for(i = 0; i < len; i++) {
        if((i % 16) == 0) {
            fprintf(fp, "%04x - ", (unsigned int)i);
        }
        fprintf(fp, "%02x ", buff[i]);
        if(i > 0 && (i % 16) == 15) {
            fprintf(fp, "\n");
        }
    }

    if((i % 16) != 0) fprintf(fp, "\n");
}

ssize_t bit_buff_to_bytes(const float *samps, size_t samp_len,
                          uint8_t *out, size_t out_len,
                          float *quality) {
    if((samp_len % 8) != 0) return -1;
    if((samp_len / 8) > out_len) return -1;

    uint8_t byte = 0;
    size_t out_idx = 0;
    size_t qual = 0;

    for(size_t i = 0; i < samp_len; i++) {
        byte >>= 1;
        byte |= (samps[i] >= 0 ? 0x80 : 0);
        if((i % 8) == 7) {
            out[out_idx] = byte;
            out_idx += 1;
        }

        if(samps[i] >= 0.5 || samps[i] <= -0.5) {
            qual += 1;
        }
    }

    if(quality) *quality = (1.0f * qual / samp_len);
    return out_idx;
}

/**
 * finds the min/max value of an array
 */
void minmax(float *buff, size_t len, float *min, float *max) {
    *max = -INFINITY;
    *min = INFINITY;

    for(size_t i = 0; i < len; i++) {
        if(buff[i] > *max) *max = buff[i];
        if(buff[i] < *min) *min = buff[i];
    }
}

/**
 * Simple median filter, useful for knocking out glitches
 * and spurious noise
 */
float median(float *buff, float *scratch, size_t len) {
    memcpy(scratch, buff, sizeof(float) * len);

    size_t i, j;
    float key;

    // Insertion sort
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

#include <stdio.h>
#include <stdint.h>

#include "stdin_src.h"

ssize_t stdin_read(const source_t *src, float *samps, size_t len) {
    ssize_t rdlen = 0;

    uint8_t buff[2];
    int16_t val;
    for(size_t i = 0; i < len; i++) {
        ssize_t rb = fread(buff, sizeof(uint8_t), 2, stdin);
        if(rb != 2) return -1;

        val = (buff[1] << 8) | buff[0];
        samps[i] = (float)val / 32768.0;
        rdlen += 1;
    }

    return rdlen;
}

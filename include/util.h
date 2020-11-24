#ifndef _UTIL_H
#define _UTIL_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define ASIZE(x) (sizeof(x) / sizeof(x[0]))

void hexdump(FILE *fp, const uint8_t *buff, size_t len);

ssize_t bit_buff_to_bytes(const float *samps, size_t samp_len,
                          uint8_t *out, size_t out_len,
                          float *quality);

void minmax(const float *buff, size_t len, float *min, float *max);

float median(const float *buff, float *scratch, size_t len);

void normalize(float *buff, size_t len);

void flip_smallest(float *data, size_t len);

#endif

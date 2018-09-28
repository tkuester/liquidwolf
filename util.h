#ifndef _UTIL_H
#define _UTIL_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

void hexdump(FILE *fp, const uint8_t *buff, size_t len);

ssize_t bit_buff_to_bytes(const float *samps, size_t samp_len,
                          uint8_t *out, size_t out_len,
                          float *quality);

#endif

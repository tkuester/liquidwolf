#ifndef _HLDC_H
#define _HLDC_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define HLDC_SAMP_BUFF_LEN ((256 + 10) * 8)

typedef struct {
    bool in_packet;
    size_t one_count;
    float samps[HLDC_SAMP_BUFF_LEN];
    size_t buff_idx;
} hldc_state_t;

void hldc_init(hldc_state_t *state);
bool hldc_execute(hldc_state_t *state, float samp, size_t *len);


#endif

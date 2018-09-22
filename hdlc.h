#ifndef _HDLC_H
#define _HDLC_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define HDLC_SAMP_BUFF_LEN ((256 + 10) * 8)

typedef struct {
    bool in_packet;
    size_t one_count;
    float samps[HDLC_SAMP_BUFF_LEN];
    size_t buff_idx;
} hdlc_state_t;

void hdlc_init(hdlc_state_t *state);
bool hdlc_execute(hdlc_state_t *state, float samp, size_t *len);


#endif

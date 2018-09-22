#include <stdio.h>

#include "hdlc.h"

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
            state->buff_idx = 0;
            return false;
        } else if(state->one_count == 6) {
            if(state->in_packet) {
                state->in_packet = false;

                if(state->buff_idx > 7) {
                    *len = state->buff_idx - 7;
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
        
        // Receiving a stuffed bit
        if(_one_count == 6) {
            state->in_packet = true;
        }
        if(_one_count == 5) {
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

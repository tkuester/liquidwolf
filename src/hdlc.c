/**
 * @file hdlc.c
 * @brief Handles HDLC deframing
 */
#include <stdio.h>

#include "hdlc.h"

/**
 * Utility function to print the state of the HDLC stream
 *
 * @param fp    The stream to print the message
 * @param state The state to debug
 */
void hdlc_debug(FILE *fp, hdlc_state_t *state) {
    fprintf(fp, "hdlc_state(in_packet=%5s, one_count=%3zu, buff_idx=%3zu)\n",
                 state->in_packet ? "true" : "false",
                 state->one_count,
                 state->buff_idx);
}

/**
 * Initializes the HDLC state structure
 *
 * @param state The HDLC FSM to init
 */
void hdlc_init(hdlc_state_t *state) {
    if(state == NULL) return;

    state->in_packet = false;
    state->one_count = 0;
    state->buff_idx = 0;
}

/**
 * Processes a sample to determine if it completes an HDLC frame
 *
 * @param state The struct which maintains state between calls
 * @param samp  The sample to process
 * @param len   The number of samples in the frame (if EOF detected)
 *
 * @return true if a frame was detected
 */
bool hdlc_execute(hdlc_state_t *state, float samp, size_t *len) {
    if(state == NULL) return false;

    // Logic one
    if(samp >= 0) {
        state->one_count += 1;

        // Illegal state: should not receive more than 6 ones in a row
        if(state->one_count > 6) {
            state->in_packet = false;
            return false;
        }

        // Receiving our 6th consecutive one
        else if(state->one_count == 6) {
            // Hop out of the frame
            if(state->in_packet) {
                state->in_packet = false;

                // Do we have bits to return? Or only two flags back to back?
                if(state->buff_idx > 6) {
                    if(len != NULL) *len = state->buff_idx - 6;
                    state->buff_idx = 0;
                    return true;
                }
                state->buff_idx = 0;
                return false;
            }
        }
    }

    // Logic 0
    else {
        // Temporarily store the number of ones
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

    // Fell through to here. No start/end of frame/stuffed bit detected
    // so log the sample to the buffer
    if(state->in_packet) {
        if(state->buff_idx < HDLC_SAMP_BUFF_LEN) {
            state->samps[state->buff_idx] = samp;
            state->buff_idx += 1;
        } else {
            state->buff_idx = 0;
            state->in_packet = false;
        }
    }

    return false;
}

/**
 * Calculates the HDLC checksum for a block of bytes. If the buffer
 * includes the checksum, subtract that from the length parameter.
 *
 * Thanks gnuradio. *yoink*
 *
 * @param data The buffer of data
 * @param len  The number of bytes in the buffer
 *
 * @return The checksum
 */
uint16_t hdlc_crc(uint8_t *data, size_t len) {
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

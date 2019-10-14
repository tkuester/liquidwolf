/**
 * @file hdlc.h
 * @brief State machine definition
 */
#ifndef _HDLC_H
#define _HDLC_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

// TODO: Make this dynamic?
#define HDLC_SAMP_BUFF_LEN ((256 + 10) * 8)

/** @brief Holds the state for the HDLC FSM */
typedef struct {
    bool in_packet; /**< @brief Are we currently in a packet? */
    size_t one_count; /**< @brief The number of consecutive ones */
    float samps[HDLC_SAMP_BUFF_LEN]; /**< @brief Stores bit samples for the buffer */
    size_t buff_idx; /**< @brief The number of bit samples in the buffer */
} hdlc_state_t;

void hdlc_debug(FILE *fp, hdlc_state_t *state);
void hdlc_init(hdlc_state_t *state);
bool hdlc_execute(hdlc_state_t *state, float samp, size_t *len);
uint16_t hdlc_crc(uint8_t *data, size_t len);

#endif

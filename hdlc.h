#ifndef _HDLC_H
#define _HDLC_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#define HDLC_SAMP_BUFF_LEN ((256 + 10) * 8)

#define FCSINIT 0xFFFF
#define FCSGOOD 0xF0B8
#define FCSSIZE 2 /* 16 bit FCS */ 

typedef struct {
    bool in_packet;
    size_t one_count;
    float samps[HDLC_SAMP_BUFF_LEN];
    size_t buff_idx;
} hdlc_state_t;

void hdlc_debug(hdlc_state_t *state);
void hdlc_init(hdlc_state_t *state);
bool hdlc_execute(hdlc_state_t *state, float samp, size_t *len);

bool crc16_ccitt(const float *buff, size_t len);
uint16_t calc_crc(uint8_t *data, size_t len);
void flip_smallest(float *data, size_t len);
#endif

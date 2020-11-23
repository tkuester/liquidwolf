#ifndef _APRS_RX_H
#define _ARPS_RX_H

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <liquid/liquid.h>

#include "bell202.h"
#include "hdlc.h"
#include "ax25.h"
#include "util.h"
#include "wav_src.h"


#define SAMPS_SIZE (1024)

typedef union {
    wav_t wav;
} source_t;

typedef struct {
    source_t src;
    float *samps;
    size_t samps_len;
    bell202_t modem;
    hdlc_state_t hdlc;

    size_t num_samps;
    size_t num_packets;
    size_t num_one_flip_packets;

    struct timeval tv_start;
    struct timeval tv_done;
} aprs_rx_t;

aprs_rx_t *aprs_rx_init(char *wavfile);
void aprs_rx_process(aprs_rx_t *rx);
void aprs_rx_destroy(aprs_rx_t *rx);

void flip_smallest(float *data, size_t len);
bool do_you_wanna_build_a_packet(ax25_pkt_t *pkt, float *buff, size_t len);


#endif

/**
 * @file aprs_rx.h
 * @brief Code that receives samples and searching for packets
 */
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
#include "stdin_src.h"

#define SAMPS_SIZE (1024)

/**
 * As each receiver needs a source, this allows all the source types
 * to share a common memory footprint, reducing the size of the receiver
 * struct.
 */
typedef union source {
    wav_src_t wav;
} source_t;


/**
 * Indicates which type of receiver this is
 */
typedef enum {
    SOURCE_NONE = 0,
    SOURCE_WAV,
    SOURCE_STDIN,
} source_type_t;

/**
 * Callback function prototype, each receiver must implement
 */
typedef ssize_t (*source_read_t)(const union source *src, float *buff, size_t len);

/** @brief The aprs receive object */
typedef struct {
    union source src; /**< @brief The sample source */
    source_type_t src_type; /**< @brief Indicate which source type */
    source_read_t read_func; /**< @brief Callback to the read samples function */
    float *samps; /**< @brief The working buffer */
    size_t samps_len; /**< @brief Size of the working buffer */
    bell202_t modem; /**< @brief The bell202 signal processing object */
    hdlc_state_t hdlc; /**< @brief The HDLC state machine */

    size_t num_samps; /**< @brief The number of samples processed */
    size_t num_packets; /**< @brief The number of packets received */
    size_t num_one_flip_packets; /**< @brief The number of packets with one bitflip */

    struct timeval tv_start; /**< @brief When the modem was started */
    struct timeval tv_done; /**< @brief When the modem was stopped */
} aprs_rx_t;

/** Allocates an aprs_rx_t struct, returns NULL on failure */
aprs_rx_t *aprs_rx_init(void);

/** Factory method for wav_src types */
bool aprs_rx_wav_open(aprs_rx_t *rx, const char *wavfile);

/** Factory method for stdin type */
bool aprs_rx_stdin_open(aprs_rx_t *rx, int rate);

/** while(1) loop for processing packets */
void aprs_rx_process(aprs_rx_t *rx);

/** Teardown method */
void aprs_rx_destroy(aprs_rx_t *rx);

/** Utility function to try flipping bits in the packet */
void flip_smallest(float *data, size_t len);

/** 
 * Given a suspected packet, compute the CRC checksum, and unpack the structure.
 * If the CRC fails, attempt to flip a few bits and test to see if the packet
 * becomes valid.
 *
 * @param pkt The packet to build
 * @param buff The buffer to analyze
 * @param len The length of the buffer
 * @return True if a packet was detected
 */
bool do_you_wanna_build_a_packet(ax25_pkt_t *pkt, const float *buff, size_t len);

#endif

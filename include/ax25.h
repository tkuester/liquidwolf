/**
 * @file ax25.h
 * @brief AX25 packet structures
 */
#ifndef _AX25_H
#define _AX25_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#define MAX_NUM_ADDRS (5)

/** @brief AX25 protocols */
typedef enum {
    ISO8208        = 0x01,
    COMP_TCPIP     = 0x06,
    UNCOMP_TCPIP   = 0x07,
    SEG_FRAG       = 0x08,
    TEXNET_DGRAM   = 0xc3,
    LINK_QUAL      = 0xc4,
    APPLETALK      = 0xca,
    APPLETALK_ARP  = 0xcb,
    ARPA_IP        = 0xcc,
    ARPA_ARP       = 0xcd,
    FLEXNET        = 0xce,
    NETROM         = 0xcf,
    NO_LAYER3      = 0xf0,
    OTHER          = 0x1ff
} ax25_proto_t;

/** @brief Contains an AX25 callsign. */
typedef struct {
    char callsign[7]; /**< @brief The callsign (null terminated) */
    bool cr;          /**< @brief Command response bit, sec 6.1.2 */
    uint8_t ssid;     /**< @brief The callsign's SSID */
} ax25_addr_t;

/** @brief Contains an AX25 packet */
typedef struct {
    ax25_addr_t dst; /**< @brief The packet destination */
    ax25_addr_t src; /**< @brief The packet source */
    ax25_addr_t rpt[MAX_NUM_ADDRS]; /**< @brief A list of repeaters */
    uint8_t rpt_len; /**< @brief How many repeaters */
    uint8_t ctrl; /**< @brief The control bit */
    ax25_proto_t proto; /**< @brief The protocol */
    const uint8_t *data; /**< @brief An unprocessed data frame */
    size_t data_len; /**< @brief The length of said data */
} ax25_pkt_t;

void ax25_pkt_dump(FILE *fp, const ax25_pkt_t *pkt);
int ax25_pkt_unpack(ax25_pkt_t *pkt, const uint8_t *buff, size_t len);
bool ax25_addr_unpack(ax25_addr_t *addr, const uint8_t *buff);

#endif

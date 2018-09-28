#ifndef _AX25_H
#define _AX25_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

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

typedef struct {
    char callsign[7];
    bool cr;
    uint8_t ssid;
} ax25_addr_t;

typedef struct {
    ax25_addr_t dst;
    ax25_addr_t src;
    uint8_t num_rpt;
    ax25_addr_t rpt[2];
    uint8_t ctrl;
    ax25_proto_t proto;
    const uint8_t *data;
    size_t data_len;
} ax25_pkt_t;

void dump_pkt(const ax25_pkt_t *pkt);
bool unpack_ax25(ax25_pkt_t *pkt, const uint8_t *buff, size_t len);
bool unpack_addr(ax25_addr_t *addr, const uint8_t *buff);

#endif

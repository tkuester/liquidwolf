#include <stdio.h>
#include <string.h>

#include "ax25.h"

void dump_pkt(const ax25_pkt_t *pkt) {
    if(pkt == NULL) return;
    printf("%s-%d -> %s-%d:", pkt->src.callsign, pkt->src.ssid, pkt->dst.callsign, pkt->dst.ssid);
    if(pkt->num_rpt >= 1) {
        printf(" via %s-%d", pkt->rpt[0].callsign, pkt->rpt[0].ssid);
        if(pkt->num_rpt == 2) {
            printf(", %s-%d", pkt->rpt[1].callsign, pkt->rpt[1].ssid);
        }
    }
    printf("\n");
    printf("> %.*s\n", (int)pkt->data_len, pkt->data);
    return;
}

bool unpack_ax25(ax25_pkt_t *pkt, const uint8_t *buff, size_t len) {
    size_t pos = 0;
    bool more_addr;

    if(pkt == NULL) return false;
    if(buff == NULL) return false;
    if(len < (7 + 7 + 2)) return false;

    memset(pkt, 0, sizeof(ax25_pkt_t));

    // Unpack dst and src addresses
    more_addr = unpack_addr(&pkt->dst, &buff[pos]); pos += 7;
    if(!more_addr) return false;

    more_addr = unpack_addr(&pkt->src, &buff[pos]); pos += 7;

    // Unpack up to two repeaters
    if(more_addr) {
        if((pos + 7) > len) return false;
        more_addr = unpack_addr(&pkt->rpt[0], &buff[pos]); pos += 7;
        pkt->num_rpt += 1;

        if(more_addr) {
            if((pos + 7) > len) return false;
            more_addr = unpack_addr(&pkt->rpt[1], &buff[pos]); pos += 7;
            pkt->num_rpt += 1;

            // Only two repeaters allowed
            if(more_addr) return false;
        }
    }

    // TODO: Ctrl / Proto > 8 bits
    if((pos + 2) > len) return false;
    pkt->ctrl = buff[pos]; pos += 1;
    pkt->proto = buff[pos]; pos += 1;
    if(!(pkt->ctrl == 0x03 && pkt->proto == NO_LAYER3)) return false;

    if(pos < len) {
        pkt->data = &buff[pos];
        pkt->data_len = len - pos;
    }

    return true;
}

bool unpack_addr(ax25_addr_t *addr, const uint8_t *buff) {
    uint8_t byte;
    size_t i;

    for(i = 0; i < 6; i++) {
        byte = buff[i] >> 1;
        if(byte == ' ') break;
        addr->callsign[i] = byte;
    }
    addr->callsign[i] = '\0';

    addr->cr = (buff[6] & 0x80) > 0;
    addr->ssid = (buff[6] >> 1) & 0x0f;

    return (buff[6] & 0x01) == 0;
}

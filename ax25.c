#include <stdio.h>
#include <string.h>

#include "ax25.h"

/**
 * Prints a file to the given file pointer
 */
void ax25_pkt_dump(FILE *fp, const ax25_pkt_t *pkt) {
    if(pkt == NULL) return;

    fprintf(fp, "%s-%d (%s) -> %s-%d (%s):",
        pkt->src.callsign, pkt->src.ssid, (pkt->src.cr ? "cmd" : "rsp"),
        pkt->dst.callsign, pkt->dst.ssid, (pkt->dst.cr ? "cmd" : "rsp"));

    if(pkt->num_rpt >= 1) {
        fprintf(fp, " (via: ");
        for(size_t i = 0; i < pkt->num_rpt; i++) {
            fprintf(fp, "%s-%d %c", pkt->rpt[i].callsign, pkt->rpt[i].ssid, (pkt->rpt[i].cr ? 'r' : '-'));
            if(i != pkt->num_rpt - 1) fprintf(fp, ", ");
        }
        fprintf(fp, ")");
    }
    fprintf(fp, "\n");
    fprintf(fp, "> %.*s\n", (int)pkt->data_len, pkt->data);
    return;
}

/**
 * Unpacks an ax25 packet from a byte buffer.
 *
 * Params:
 * pkt - The packet to unpack into
 * buff - The buffer which contains the packet
 * len - The size of the buffer (does not include HDLC checksum)
 *
 * Returns:
 * 0 on success
 */
int ax25_pkt_unpack(ax25_pkt_t *pkt, const uint8_t *buff, size_t len) {
    int err = 0;
    size_t pos = 0;
    bool more_addr;

    if(pkt == NULL) { err = 1; goto fail; }
    if(buff == NULL) { err = 2; goto fail; }
    if(len < (7 + 7 + 2)) { err = 3; goto fail; }

    memset(pkt, 0, sizeof(ax25_pkt_t));

    // Unpack dst and src addresses
    more_addr = ax25_addr_unpack(&pkt->dst, &buff[pos]); pos += 7;
    if(!more_addr) { err = 4; goto fail; }

    more_addr = ax25_addr_unpack(&pkt->src, &buff[pos]); pos += 7;

    // Unpack up to two repeaters
    for(size_t i = 0; i < MAX_NUM_ADDRS; i++) {
        if(!more_addr) break;

        if((pos + 7) > len) { err = 5; goto fail; }
        more_addr = ax25_addr_unpack(&pkt->rpt[i], &buff[pos]); pos += 7;
        pkt->num_rpt += 1;
    }
    if(more_addr) { err = 6; goto fail; }

    // TODO: Ctrl / Proto > 8 bits
    if((pos + 2) > len) { err = 7; goto fail; }
    pkt->ctrl = buff[pos]; pos += 1;
    pkt->proto = buff[pos]; pos += 1;
    if(!(pkt->ctrl == 0x03 && pkt->proto == NO_LAYER3)) { err = 8; goto fail; }

    if(pos < len) {
        pkt->data = &buff[pos];
        pkt->data_len = len - pos;
    }

    return 0;

fail:
    return err;
}

/**
 * Unpacks an AX.25 formatted address from a byte buffer.
 * buff must have len >= 7. Since addresses are a fixed size,
 * this function does no boundary checking. You shouldn't call
 * it directly.
 *
 * Params:
 * addr - The addr to fill
 * buff - The buffer to receive from, must be len >= 7
 *
 * Returns:
 * true if there are more addresses to parse
 */
bool ax25_addr_unpack(ax25_addr_t *addr, const uint8_t *buff) {
    uint8_t byte;
    size_t i;

    if(addr == NULL) return false;
    if(buff == NULL) return false;

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

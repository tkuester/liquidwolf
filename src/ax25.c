/**
 * @file ax25.c
 * @brief Logic for serializing / deserializing ax25 packets
 */
#include <stdio.h>
#include <string.h>

#include "ax25.h"

/**
 * Prints out a packet to the given file pointer. Output is multi line.
 *
 *     WA6TK-0 (rsp) -> APS227-0 (rsp): (via: W6PVG-3 r, W6SCE-10 r)
 *     > @230139z3451.81N/11810.37W_151/000g001t060r000p000P000b10196h27
 * 
 * @param fp  The file to print to (ie: stdout)
 * @param pkt The packet to print
 */
void ax25_pkt_dump(FILE *fp, const ax25_pkt_t *pkt) {
    if(pkt == NULL) return;

    fprintf(fp, "%s-%d (%s) -> %s-%d (%s):",
        pkt->src.callsign, pkt->src.ssid, (pkt->src.cr ? "cmd" : "rsp"),
        pkt->dst.callsign, pkt->dst.ssid, (pkt->dst.cr ? "cmd" : "rsp"));

    if(pkt->rpt_len >= 1) {
        fprintf(fp, " (via: ");
        for(size_t i = 0; i < pkt->rpt_len; i++) {
            fprintf(fp, "%s-%d %c", pkt->rpt[i].callsign, pkt->rpt[i].ssid, (pkt->rpt[i].cr ? 'r' : '-'));
            if(i != pkt->rpt_len - 1) fprintf(fp, ", ");
        }
        fprintf(fp, ")");
    }
    fprintf(fp, "\n");
    fprintf(fp, "> %.*s\n", (int)pkt->data_len, pkt->data);
}

/**
 * Unpacks an ax25 packet from a byte buffer. This function assumes that
 * the buffer already contains a valid packet.
 *
 * @param pkt  The packet to unpack into
 * @param buff The buffer which contains the packet
 * @param len  The size of the buffer, excluding HDLC checksum
 *
 * @return Zero on success
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
        pkt->rpt_len += 1;
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
 * this directly.
 *
 * @param addr The addr to fill
 * @param buff The buffer to receive from, must be len >= 7
 *
 * @return true if there are more addresses to parse
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

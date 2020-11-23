#include <stdio.h>

#include "aprs_rx.h"

#ifndef _LIQUIDWOLF_VERSION
#define _LIQUIDWOLF_VERSION "(unversioned)"
#endif

int main(int argc, char **argv) {
    int rc = 1;
    aprs_rx_t *rx;

    printf("LiquidWolf %s\n", _LIQUIDWOLF_VERSION);

    if(argc != 2) {
        fprintf(stderr, "Usage: %s in.wav\n", argv[0]);
        return 1;
    }

    rx = aprs_rx_init();
    if(!rx) goto fail;
    if(strcmp(argv[1], "-") == 0) {
        if(!aprs_rx_stdin_open(rx)) goto fail;
    } else {
        if(!aprs_rx_wav_open(rx, argv[1])) goto fail;
    }

    aprs_rx_process(rx);

    rc = 0;

fail:
    aprs_rx_destroy(rx);
    return rc;
}

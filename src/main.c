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

    rx = aprs_rx_init(argv[1]);
    if(!rx) goto fail;

    aprs_rx_process(rx);

    rc = 0;

fail:
    aprs_rx_destroy(rx);
    return rc;
}

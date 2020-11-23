#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "aprs_rx.h"

#ifndef _LIQUIDWOLF_VERSION
#define _LIQUIDWOLF_VERSION "(unversioned)"
#endif

void version(char **argv) {
    printf("%s %s\n", argv[0], _LIQUIDWOLF_VERSION);
}

void usage(char **argv) {
    printf("%s [-r sample_rate] FILE|-\n", argv[0]);
    printf("\t-r <rate> : Sample rate (default: 24000)\n");
    printf("\t-v        : Print version\n");
    printf("\t-h        : Usage\n");
}

int main(int argc, char **argv) {
    int rc = 1;
    aprs_rx_t *rx = NULL;
    int rate = 24000;
    char *file = NULL;

    int c;
    while((c = getopt(argc, argv, "hvr:")) != -1) {
        switch(c) {
            case 'r':
                errno = 0;
                rate = strtol(optarg, NULL, 10);
                if(errno != 0) {
                    perror("Invalid sample rate");
                    goto done;
                } else if (rate < 8000 || rate > 96000) {
                    fprintf(stderr, "Invalid sample rate: must be between 8 kHz and 96 kHz\n");
                    goto done;
                }
                break;

            case 'v':
                rc = 0;
                version(argv);
                goto done;
                break;

            case 'h':
                rc = 0;
            case '?':
                printf("\n");
            default:
                usage(argv);
                goto done;
                break;
        }
    }

    if(argc - optind != 1) {
        usage(argv);
        goto done;
    } else {
        file = argv[optind];
    }

    rx = aprs_rx_init();
    if(!rx) goto done;

    if(strcmp(file, "-") == 0) {
        if(!aprs_rx_stdin_open(rx, rate)) goto done;
    } else {
        if(!aprs_rx_wav_open(rx, file)) goto done;
    }

    aprs_rx_process(rx);

    rc = 0;

done:
    aprs_rx_destroy(rx);
    return rc;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>

#include <liquid/liquid.h>
#include <sndfile.h>

#include "dsp.h"
#include "hdlc.h"
#include "util.h"

#define SAMPS_SIZE (1024)

int main(int argc, char **argv) {
    int rc = 1;
    struct timeval tv_start, tv_done;

    char *wavfile = NULL;
    SNDFILE *sf = NULL;
    SF_INFO sfinfo;

    float *samps = NULL;
    hdlc_state_t hdlc;
    hdlc_init(&hdlc);

    size_t num_packets = 0;
    size_t num_one_flip_packets = 0;

    if(argc != 2) {
        fprintf(stderr, "Usage: %s in.wav\n", argv[0]);
        return 1;
    }

    wavfile = argv[1];
    memset(&sfinfo, 0, sizeof(sfinfo));
    sf = sf_open(wavfile, SFM_READ, &sfinfo);
    if(sf == NULL) {
        fprintf(stderr, "Can't open file %s for reading\n", wavfile);
        goto fail;
    }

    if(sfinfo.channels > 2) {
        fprintf(stderr, "WARNING: %d channels detected, only reading from ch 0\n", sfinfo.channels);
        goto fail;
    }

    if(sfinfo.samplerate < 8000) {
        fprintf(stderr, "ERROR: Sample rate must be at >=8000, is %d\n", sfinfo.samplerate);
        goto fail;
    }
    printf("Opened %s: %d Hz, %d chan\n", wavfile, sfinfo.samplerate, sfinfo.channels);

    samps = malloc(sizeof(float) * SAMPS_SIZE * sfinfo.channels);
    if(!samps) {
        fprintf(stderr, "malloc\n");
        goto fail;
    }

    if(!dsp_init(sfinfo.samplerate)) {
        fprintf(stderr, "Unable to init DSP structures\n");
        goto fail;
    }

    size_t idx = 0;
    gettimeofday(&tv_start, NULL);
    while(1) {
        //read = fread(&samps, sizeof(float), ASIZE(samps), fp);
        size_t read = sf_read_float(sf, samps, SAMPS_SIZE);
        if(read != SAMPS_SIZE) {
            if(sf_error(sf)) {
                fprintf(stderr, "Read error: %d\n", sf_error(sf));
                goto fail;
            }
        }

        for(int i = 0; i < read; i += sfinfo.channels) {
            idx += 1;

            float out_bit;
            if(!dsp_process(samps[i], &out_bit)) continue;

            size_t frame_len;
            bool got_frame = hdlc_execute(&hdlc, out_bit, &frame_len);
            if(!got_frame) continue;
            if((frame_len % 8) != 0) continue; // Discard packets with non multiple of 8 len

            bool got_pkt = false;
            if(crc16_ccitt((float *)&hdlc.samps, frame_len)) {
                got_pkt = true;
            } else {
                flip_smallest((float *)&hdlc.samps, frame_len);
                if(crc16_ccitt((float *)&hdlc.samps, frame_len)) {
                    got_pkt = true;
                }
            }

            if(got_pkt) num_packets += 1;
        }

        if(read != SAMPS_SIZE) {
            printf("Done\n");
            break;
        }
    }

    gettimeofday(&tv_done, NULL);

    float samp_per_sec = idx / ((tv_done.tv_sec + tv_done.tv_usec / 1e6) - (tv_start.tv_sec + tv_start.tv_usec / 1e6));
    printf("Processed %zu samples\n", idx);
    printf("%d samp / sec\n", (int)samp_per_sec);
    printf("%.1fx speed\n", samp_per_sec / sfinfo.samplerate);
    printf("%zu packets\n", num_packets);
    printf("%zu one flip packets\n", num_one_flip_packets);

    rc = 0;

fail:
    if(samps) free(samps);
    if(sf) sf_close(sf);
    dsp_destroy();
    return rc;
}

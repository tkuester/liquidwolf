#include "wav_src.h"
#include "aprs_rx.h"

int wav_open(const char *path, wav_src_t *wav) {
    if(!path || !wav) return -1;

    wav->sf = NULL;
    memset(&wav->info, 0, sizeof(SF_INFO));
    wav->samplerate = 0;
    wav->tmp_buff = NULL;

    wav->sf = sf_open(path, SFM_READ, &wav->info);
    if(wav->sf == NULL) {
        fprintf(stderr, "Can't open file %s for reading\n", path);
        goto fail;
    }

    wav->samplerate = wav->info.samplerate;

    if(wav->info.channels > 1) {
        fprintf(stderr, "WARNING: %d channels detected, only reading from ch 0\n", wav->info.channels);
        // FIXME: 1024 is a magic number
        wav->tmp_buff = malloc(sizeof(float) * 1024 * wav->info.channels);
        if(!wav->tmp_buff) {
            fprintf(stderr, "malloc\n");
            goto fail;
        }
    }

    if(wav->info.samplerate < 8000) {
        fprintf(stderr, "ERROR: Sample rate must be at >=8000, is %d\n", wav->info.samplerate);
        goto fail;
    }

    printf("Opened %s: %d Hz, %d chan\n", path, wav->info.samplerate, wav->info.channels);

    return 0;

fail:
    return -1;
}

void wav_close(wav_src_t *wav) {
    if(!wav) return;

    if(wav->sf) {
        sf_close(wav->sf);
        wav->sf = NULL;
    }

    if(wav->tmp_buff) {
        free(wav->tmp_buff);
        wav->tmp_buff = NULL;
    }
}

ssize_t wav_read(const source_t *src, float *samps, size_t len) {
    ssize_t read;

    if(!src) return -1;
    const wav_src_t wav = src->wav;

    if(wav.info.channels > 1) {
        read = sf_readf_float(wav.sf, wav.tmp_buff, len);
        for(size_t i = 0; i < read; i++) {
            samps[i] = wav.tmp_buff[i * wav.info.channels];
        }
    } else {
        read = sf_read_float(wav.sf, samps, len);
    }

    if(read != len) {
        if(sf_error(wav.sf)) {
            fprintf(stderr, "Read error: %d\n", sf_error(wav.sf));
            goto fail;
        }
    }

    return read;

fail:
    return -1;
}

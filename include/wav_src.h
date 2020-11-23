#ifndef _WAV_SRC_H
#define _WAV_SRC_H

#include <string.h>
#include <stdlib.h>

#include <sndfile.h>

typedef struct {
    SF_INFO info;
    SNDFILE *sf;
    int samplerate;
    float *tmp_buff;
} wav_t;

int wav_open(const char *path, wav_t *wav);
void wav_close(wav_t *wav);
ssize_t wav_read(const wav_t *wav, float *samps, size_t len);

#endif

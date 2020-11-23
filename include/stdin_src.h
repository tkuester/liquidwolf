#ifndef _STDIN_SRC_H
#define _STDIN_SRC_H

union source;
typedef union source source_t;

ssize_t stdin_read(const source_t *wav, float *samps, size_t len);

#endif

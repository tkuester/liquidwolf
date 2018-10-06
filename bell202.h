#ifndef _BELL202_H
#define _BELL202_H

#include <stdbool.h>

#ifndef M_PI
#define M_PI (3.141592654f)
#endif

bool dsp_init(int _input_rate);
bool dsp_process(float samp, float *out_bit);
void dsp_destroy(void);

#endif

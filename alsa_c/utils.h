#ifndef UTILS_H
#define UTILS_H

#include "kiss_fft.h"

int findPeaks(float *signal, int length, int maxPositions, int *positions);
float mag(kiss_fft_cpx number);
float phase(kiss_fft_cpx number);
void sortPeaks(float *samples, int *newInicies, int length);
void hanning(float *samples, int length);
void blackman(float *window, int N);
void peakEstimate(float l, float m, float r, float peak, float *x, float *y);

#endif
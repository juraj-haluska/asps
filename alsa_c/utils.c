#include "utils.h"
#include <math.h>
#include <stdlib.h>

// used in sorting
float *array;
static int cmp(const void *a, const void *b);

/*  find peaks in signal 
    returns number of peaks found
*/
int findPeaks(float *signal, int length, int maxPositions, int *positions)
{
    int peakCount = 0;
    for (int i = 1; i < (length - 1); i++)
    {
        if (signal[i - 1] < signal[i] && signal[i + 1] < signal[i])
        {
            positions[peakCount++] = i;
            if (peakCount >= maxPositions)
            {
                return maxPositions;
            }
        }
    }
    return peakCount;
}

float mag(kiss_fft_cpx number)
{
    return sqrt(number.r * number.r + number.i * number.i);
}

float phase(kiss_fft_cpx number)
{
    return atan2(number.i, number.r);
}

void hanning(float *samples, int length)
{
    for (int i = 0; i < length; i++)
    {
        samples[i] = 0.5f * (1 - cos(2 * M_PI * i / (length - 1)));
    }
}

void sortPeaks(float *samples, int *newInicies, int length)
{
    array = samples;
    qsort(newInicies, length, sizeof(*newInicies), cmp);
}

static int cmp(const void *a, const void *b)
{
    int ia = *(int *)a;
    int ib = *(int *)b;
    return array[ia] > array[ib] ? -1 : array[ia] < array[ib];
}

void blackman(float *window, int N)
{
    for (int n = 0; n < (N / 2); n++)
    {
        window[n] = 0.42f - 0.5f * cos((2 * M_PI * n) / (N - 1)) + 0.08f * cos((4 * M_PI * n) / (N - 1));
    }

    for (int n = (N / 2); n < N; n++)
    {
        window[n] = window[N - n - 1];
    }
}

void peakEstimate(float l, float m, float r, float peak, float *x, float *y)
{
    double s = 2 * (l - 2 * m + r);

    double _x = (l - r + peak * s) / s;
    double _y = (-(l * l) + (8 * l * m) + (2 * l * r) - (16 * m * m) + (8 * m * r) - (r * r)) / (4 * s);

    *x = (float) _x;
    *y = (float) _y;
}
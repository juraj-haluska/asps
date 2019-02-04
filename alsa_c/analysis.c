/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API

#include <unistd.h>
#include "kiss_fft.h"
#include "utils.h"
#include "common.h"
#include <fenv.h>

// program options
#define LIVE

#ifdef LIVE

#include <alsa/asoundlib.h>
#include <pthread.h>
void catchKeyThread();
volatile int changeHarmony = 0; 

#else

#include "wavfile.h"
#define INPUT_FILE "input.wav"

#endif

int main()
{
    //feenableexcept(FE_ALL_EXCEPT);

    unsigned long fftSize = 2048;
    unsigned long winSize = 512;
    unsigned long hopSize = winSize / 2;
    int fs = 44100;
    int isEmpty = 1;

#ifdef LIVE

    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    int dir;

#else

    FILE *file = wavOpen(INPUT_FILE);
    if (file == NULL)
    {
        fprintf(stderr, "error opening file\r\n");
        exit(1);
    }

    int16_t *buffer;

#endif

    kiss_fft_cpx inData[fftSize];
    kiss_fft_cpx outData[fftSize];
    float *act;
    float *prev;
    float *win;
    float mags[fftSize / 2 + 1];

    // fill in and out data with zeros
    memset(inData, 0, sizeof(kiss_fft_cpx) * fftSize);
    memset(outData, 0, sizeof(kiss_fft_cpx) * fftSize);

    /* allocate configuration struct for kiss fft lib */
    kiss_fft_cfg cfg = kiss_fft_alloc(fftSize, 0, 0, 0);

#ifdef LIVE

    /* Open PCM device for recording (capture). */
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (rc < 0)
    {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* Allocate a hardware parameters object. */
    snd_pcm_hw_params_alloca(&params);

    /* Fill it in with default values. */
    snd_pcm_hw_params_any(handle, params);

    /* Set the desired hardware parameters. */

    /* Interleaved mode */
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_NONINTERLEAVED);

    /* Signed 16-bit little-endian format */
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_FLOAT);

    /* Two channels (stereo) */
    snd_pcm_hw_params_set_channels(handle, params, 1);

    /* 44100 bits/second sampling rate (CD quality) */
    snd_pcm_hw_params_set_rate_near(handle, params, &fs, &dir);

    /* Set period size to n hopSize. */
    snd_pcm_hw_params_set_period_size_near(handle, params, (unsigned long *)&hopSize, &dir);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0)
    {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, (unsigned long *)&hopSize, &dir);
    fprintf(stderr, "actual frame size: %ld\r\n", hopSize);

#else

    buffer = malloc(sizeof(int16_t) * hopSize);

#endif

    act = malloc(sizeof(float) * hopSize);
    prev = malloc(sizeof(float) * hopSize);
    win = malloc(sizeof(float) * winSize);

    // calculate windowing function
    blackman(win, winSize);

    while (1)
    {

#ifdef LIVE
        rc = snd_pcm_readi(handle, act, hopSize);
        if (rc == -EPIPE)
        {
            /* EPIPE means overrun */
            fprintf(stderr, "overrun occurred\n");
            snd_pcm_prepare(handle);
        }
        else if (rc < 0)
        {
            fprintf(stderr, "error from read: %s\n", snd_strerror(rc));
        }
        else if (rc != (int)hopSize)
        {
            fprintf(stderr, "short read, read %d hopSize\n", rc);
        }

#else

        // iteration != 0
        if (isEmpty == 0)
        {
            fseek(file, hopSize * sizeof(int16_t), SEEK_CUR);
        }

        int cnt = fread(buffer, sizeof(int16_t), hopSize, file);
        if (cnt != hopSize)
        {
            fprintf(stderr, "file has ended\r\n");
            break; // exit while loop
        } 

        for (int i = 0; i < hopSize; i++)
        {
            act[i] = (float)buffer[i];
        }

#endif
        // copy data from buffers
        for (int i = 0; i < hopSize; i++)
        {
            inData[i].r = prev[i];
            inData[i + hopSize].r = act[i];
        }

        // apply window
        for (int i = 0; i < winSize; i++)
        {
            inData[i].r *= win[i];
        }

        // swap buffers
        float *tmp = act;
        act = prev;
        prev = tmp;

        // first iteration - we have only half of data
        if (isEmpty)
        {
            isEmpty = 0;
            continue;
        }

        // perform fft
        kiss_fft(cfg, inData, outData);

        // calculate magnitudes
        for (int i = 0; i <= fftSize / 2; i++)
        {
            mags[i] = mag(outData[i]);
            //mags[i] *= mags[i];
        }

        // find peaks
        int indicies[fftSize / 2];
        int peaksCount = findPeaks(mags, fftSize / 2 + 1, fftSize / 2, indicies);

        // sort peaks
        sortPeaks(mags, indicies, peaksCount);

        peaks_t peaks;
        memset(&peaks, 0, sizeof(peaks));

        // fill struct
        for (int i = 0; i < PEAKS_COUNT; i++)
        {
            int hI = indicies[i];

            float m = mags[hI];
            float l = mags[hI - 1];
            float r = mags[hI + 1];
            float x, y;
            peakEstimate(l, m, r, hI, &x, &y);
            float freq = x * fs / fftSize;

            peaks.mags[i] = y / fftSize;
            peaks.freqs[i] = freq;
        }

        // output struct
        int erc = write(1, (const void *)&peaks, sizeof(peaks));
        if (erc != sizeof(peaks))
        {
            fprintf(stderr, "error outputing data struct\r\n");
        }
    }

    free(act);
    free(prev);
    free(win);

#ifdef LIVE

    snd_pcm_drain(handle);
    snd_pcm_close(handle);

#else

    wavClose(file);
    free(buffer);

#endif

    kiss_fft_free(cfg);

    return 0;
}

#ifdef LIVE

void catchKeyThread() 
{
    while (1) {
        usleep(100000);
        char ch = getchar();
        if (ch == 'n')
        {
            changeHarmony = -1;
        } else 
        {
            changeHarmony = 1;
        }
        printf("ok\r\n");
    }
}

#endif

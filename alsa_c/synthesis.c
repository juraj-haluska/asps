#include <unistd.h>
#include "common.h"
#include "synthesis.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <time.h>

#define TRACKS PEAKS_COUNT
#define RADIUS 100

#define ITERATIONS 300000

// program options
//#define DEBUG
#define LIVE
//#define LOGFILE

// some harmonies
#define HEND    255
int MAJ7[] = {-24, 7, 11, HEND};
int MOL7[] = {3, 7, 10, HEND};
int DUR7[] = {4, 7, 10, HEND};
int DIM[] = {3, 6, 9, HEND};
int MOL7b5[] = {3, 6, 10, HEND};
int MOL76[] = {-2, 3, 9, HEND};
int MOL79[] = {3, 7, 10, 14, HEND};
int GGG[] = {-2, 2, 5, 8, HEND};
int AAA[] = {-5, 5, 10, 14, HEND};
int CLEAN[] = {0, HEND};
int DEEP[] = {-24, -12, HEND};
int POWER[] = {-12, 0, 7, 12, HEND};

int *harmony[] = {CLEAN, MAJ7, MOL7, DUR7, MOL76, DIM, MOL79, POWER};
const int harmonyCount = sizeof(harmony) / sizeof(harmony[0]);
int selectedHarmony = 5;


#ifdef DEBUG

static struct timespec startTime;
static long times[5];

#endif


int main()
{
    // params
    // must be the half of winSize in analysis
    unsigned long winSize = 256;
    float MAGIC = 0.00014247586f;
    float thold = 0.0003;
    float *buffer = (float *)malloc(sizeof(float) * winSize);
    char strBuff[50]; 

#ifdef LOGFILE
    FILE *f = fopen("data.csv", "a");
#endif

#ifdef LIVE
    // alsa thingy
    int rc;
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    unsigned int val;
    int dir;

    /* Open PCM device for playback. */
    rc = snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
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
    int fs = 44100;
    snd_pcm_hw_params_set_rate_near(handle, params, &fs, &dir);

    /* Set period size to 32 frames. */
    snd_pcm_hw_params_set_period_size_near(handle, params, &winSize, &dir);
    printf("actual win size: %ld\r\n", winSize);

    /* Write the parameters to the driver */
    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0)
    {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(params, &winSize, &dir);
#endif

    // initialise tracks
    track_t *tracks = createTracks(TRACKS);

    float(*distances)[PEAKS_COUNT] = malloc(sizeof(float[PEAKS_COUNT][PEAKS_COUNT]));

    int iter = 0;
    while (++iter <= ITERATIONS)
    {

#ifdef DEBUG
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
        times[0] = startTime.tv_nsec;
        printf(" ------------------- iteration: %d ---------------------\r\n", iter);
#endif

        peaks_t peaks;
        int count = read(0, (void *)&peaks, sizeof(peaks));
        if (count != sizeof(peaks))
        {
            fprintf(stderr, "error reading data from pipe\r\n");
            exit(1);
        }

        // update track states
        updateTracks(tracks, TRACKS);

        // calculate distances peaks <-> tracks
        for (int peakI = 0; peakI < PEAKS_COUNT; peakI++)
        {
            for (int trackI = 0; trackI < PEAKS_COUNT; trackI++)
            {
                distances[peakI][trackI] = trackDistance(tracks[trackI], peaks.freqs[peakI], peaks.mags[peakI]);
#ifdef DEBUG
                // printf("%d:%d=%f\r\n", peakI, trackI, distances[peakI][trackI]);
#endif
            }
        }

        // holds indicies of not assigned peaks, -1 is not valid index :D
        int notAssignedPeaksI[PEAKS_COUNT];
        for (int i = 0; i < PEAKS_COUNT; i++)
        {
            notAssignedPeaksI[i] = i;
        }

        // try match track with peak
        for (int peakI = 0; peakI < PEAKS_COUNT; peakI++)
        {
            // first filter out track which were already assiged and which
            // distance is bigger than radius
            int possibleIndicies[TRACKS];
            int iI = 0;
            for (int trackI = 0; trackI < TRACKS; trackI++)
            {
                // skip track if it is assigned
                if (tracks[trackI].assigned == 1)
                {
                    continue;
                }
                if (distances[peakI][trackI] > RADIUS)
                {
                    continue;
                }
                possibleIndicies[iI++] = trackI;
            }

            // if there were some track which could be used with this sneaky peaky
            if (iI > 0)
            {
                // now select track with lowest distance of possible tracks
                // (count of them is now in iI)
                int minI = possibleIndicies[0]; // keeps index of track with lowest distance
                for (int pI = 1; pI < iI; pI++)
                {
                    int possibleI = possibleIndicies[pI];
                    if (distances[peakI][possibleI] < distances[peakI][minI])
                    {
                        minI = possibleI;
                    }
                }

                // assign your track man
                tracks[minI].assigned = 1;
                tracks[minI].state = ACTIVE;
                tracks[minI].freq.f_1 = tracks[minI].freq.f0;
                tracks[minI].freq.f0 = peaks.freqs[peakI];
                tracks[minI].mag.m_1 = tracks[minI].mag.m0;
                tracks[minI].mag.m0 = peaks.mags[peakI];
                // this situation here is not very probable, but in case...
                if (tracks[minI].state == OFF)
                {
                    // because we do not want to interpolate frequency from zero
                    tracks[minI].freq.f_1 = peaks.freqs[peakI];
                }

                // remove peak index from notAssignedPeaksI
                for (int i = 0; i < PEAKS_COUNT; i++)
                {
                    if (notAssignedPeaksI[i] == peakI)
                    {
                        notAssignedPeaksI[i] = -1;
                    }
                }
            }
        }

        // tell the active tracks which weren't assigned that they're gonna die
        for (int trackI = 0; trackI < TRACKS; trackI++)
        {
            if (tracks[trackI].assigned == 0 && tracks[trackI].state == ACTIVE)
            {
                tracks[trackI].state = DROPPING;
                tracks[trackI].freq.f_1 = tracks[trackI].freq.f0;
                tracks[trackI].mag.m_1 = tracks[trackI].mag.m0;
                tracks[trackI].mag.m0 = 0;
                tracks[trackI].assigned = 1;
            }
        }

        // spin up new tracks with peaks wich werent assigned
        for (int i = 0; i < PEAKS_COUNT; i++)
        {
            // if this peak werent assigned to track
            if (notAssignedPeaksI[i] != -1)
            {
                int peakI = notAssignedPeaksI[i];

                // skip this peak if it's insignificant
                if (peaks.mags[peakI] < thold)
                {
                    continue;
                }

                // pick first available track
                for (int trackI = 0; trackI < TRACKS; trackI++)
                {
                    if (tracks[trackI].assigned == 0 && tracks[trackI].state == OFF)
                    {
                        tracks[trackI].freq.f_1 = peaks.freqs[peakI];
                        tracks[trackI].freq.f0 = peaks.freqs[peakI];
                        tracks[trackI].mag.m_1 = 0;
                        tracks[trackI].mag.m0 = peaks.mags[peakI];
                        tracks[trackI].assigned = 1;
                        tracks[trackI].state = ACTIVE;
                        break;
                    }
                }
            }
        }
#ifdef DEBUG
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
        times[1] = startTime.tv_nsec;
        // now tracks should be assigned
        // debug
        for (int i = 0; i < TRACKS; i++)
        {
            track_t track = tracks[i];

            printf("state: %s, f_1: %f, f0: %f, m_1: %f, m0: %f, p: %f, ass: %d\r\n",
                   (track.state == ACTIVE) ? "ACTIVE" : (track.state == OFF ? "OFF" : "DROPPING"),
                   track.freq.f_1,
                   track.freq.f0,
                   track.mag.m_1,
                   track.mag.m0,
                   track.phase,
                   track.assigned);
        }

        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
        times[2] = startTime.tv_nsec;
#endif
        // ------------- synthesis ----------------

        memset(buffer, 0, winSize * sizeof(float));

        float nFactor = 1;

        for (int n = 0; n < winSize; n++)
        {
            for (int t = 0; t < TRACKS; t++)
            {
                // skip inactive tracks
                if (tracks[t].state == OFF)
                {
                    continue;
                }

                // get track pointer for convenience
                track_t *track = &tracks[t];

                float _f0 = (track->freq.f0);

                float mn = track->mag.m_1 + (n + 1) * (track->mag.m0 - track->mag.m_1) / winSize;
                float dn = track->freq.f_1 + (n + 1) * (_f0 - track->freq.f_1) / winSize;
                track->phase = track->phase + MAGIC * dn;

                // selected harmony array
                int *harArr = harmony[selectedHarmony];
                int harmonyLength = getHarmonyLength(harArr);
            
                for (int h = 0; h < harmonyLength; h++)
                {
                    float harmonyFactor = pow(2, harArr[h] / 12.0);
                    float gn = mn * cos(track->phase * harmonyFactor);
                    buffer[n] += 1.5*gn;
                }
            }

            if (buffer[n] > nFactor)
            {
                nFactor = buffer[n];
            }
        }

        // normalise
        nFactor = 1 / nFactor;
        nFactor = 0.9 * nFactor;
        for (int n = 0; n < winSize; n++)
        {
            buffer[n] = buffer[n] * nFactor;
            if (buffer[n] > 0.06f) {
                buffer[n] = 0.06f;
            }
            if (buffer[n] < -0.06f)
            {
                buffer[n] = -0.06f;
            }
        }

#ifdef DEBUG
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
        times[3] = startTime.tv_nsec;
#endif

#ifdef LOGFILE

        for (int i = 0; i < winSize; i++)
        {
            int n = i % winSize;
            sprintf(strBuff, "%f;\r\n", buffer[n]);
            fputs(strBuff, f);
        }

#endif

#ifdef LIVE
        rc = snd_pcm_writei(handle, buffer, winSize);
        if (rc == -EPIPE)
        {
            /* EPIPE means underrun */
            fprintf(stderr, "underrun occurred\n");
            snd_pcm_prepare(handle);
        }
        else if (rc < 0)
        {
            fprintf(stderr, "error from writei: %s\n", snd_strerror(rc));
        }
        else if (rc != (int)winSize)
        {
            fprintf(stderr, "short write, write %d frames\n", rc);
        }
#endif

#ifdef DEBUG
        clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &startTime);
        times[4] = startTime.tv_nsec;
        printf(" -- iteration %d time stat --\r\n", iter);
        printf("\titeration took: %d us\r\n", (int)((times[4] - times[0]) / 1000));
        printf("\ttracking took: %d us\r\n", (int)((times[1] - times[0]) / 1000));
        printf("\tsynthesis took: %d us\r\n", (int)((times[3] - times[2]) / 1000));
        printf(" -- iteration %d time stat --\r\n", iter);
#endif
    }
#ifdef LIVE
    snd_pcm_drain(handle);
    snd_pcm_close(handle);
#endif
    free(distances);
    free(buffer);
    destroyTracks(tracks);

#ifdef LOGFILE
    fclose(f);
#endif

    return 0;
}

track_t *createTracks(int tracksCount)
{
    track_t *tracks = (track_t *)malloc(sizeof(track_t) * tracksCount);
    memset(tracks, 0, sizeof(track_t) * tracksCount);

    for (int i = 0; i < tracksCount; i++)
    {
        tracks[i].state = OFF;
        tracks[i].assigned = 0;
    }

    return tracks;
};

float trackDistance(track_t track, float freq, float mag)
{
    return sqrt(pow((track.freq.f_1 - freq), 2) + pow((track.mag.m_1 - mag), 2));
}

void destroyTracks(track_t *tracks)
{
    free(tracks);
};

void updateTracks(track_t *tracks, int tracksCount)
{
    for (int i = 0; i < tracksCount; i++)
    {
        tracks[i].assigned = 0;
        tracks[i].freq.f_1 = tracks[i].freq.f0;
        tracks[i].mag.m_1 = tracks[i].mag.m0;

        if (tracks[i].state == DROPPING)
        {
            tracks[i].state = OFF;
            tracks[i].phase = 0;
            tracks[i].freq.f_1 = 0;
            tracks[i].mag.m_1 = 0;
            tracks[i].freq.f0 = 0;
            tracks[i].mag.m0 = 0;
        }
    }
}

int getHarmonyLength(int *harArr)
{
    for (int i = 0; i < 10; i++)
    {
        if (harArr[i] == HEND)
        {
            return i;
        }
    }
    return 0;
}
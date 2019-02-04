#ifndef _SYNTHESIS_H
#define _SYNTHESIS_H

typedef enum {
    ACTIVE,
    DROPPING,
    OFF
} state_t;

typedef struct {
    float f_1;
    float f0;
} freq_t;

typedef struct {
    float m_1;
    float m0;
} mag_t;

typedef struct {
    state_t state;
    freq_t freq;
    mag_t mag;
    float phase;
    int assigned;
} track_t;

track_t *createTracks(int tracksCount);
float trackDistance(track_t track, float freq, float mag);
void destroyTracks(track_t *tracks);
void updateTracks(track_t *tracks, int tracksCount);
int getHarmonyLength(int *harArr);

#endif
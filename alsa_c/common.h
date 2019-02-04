#define PEAKS_COUNT 20

typedef struct
{
    float mags[PEAKS_COUNT];
    float freqs[PEAKS_COUNT];
    int changeHarmony;
} peaks_t;

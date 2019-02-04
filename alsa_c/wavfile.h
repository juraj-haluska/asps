/*  Primitive library for writing mono, 16 bit signed PCM
 *  data to .wav file.
 */

#ifndef PCM_TO_WAV_WAVFILE_H_H
#define PCM_TO_WAV_WAVFILE_H_H

#include <inttypes.h>
#include <stdio.h>

struct t_header
{
    uint32_t chunkId;
    uint32_t chunkSize;
    uint32_t format;
    uint32_t subChunk1Id;
    uint32_t subChunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    uint32_t subChunk2Id;
    uint32_t subChunk2Size;
};

FILE *wavOpen(char *fname);
void wavClose(FILE *w);

#endif //PCM_TO_WAV_WAVFILE_H_H
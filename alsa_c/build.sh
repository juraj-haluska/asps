gcc analysis.c kiss_fft.c utils.c wavfile.c -o analysis -lasound -lm -lpthread
gcc synthesis.c -o synthesis -lasound -lm
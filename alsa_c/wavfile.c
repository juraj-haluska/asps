#include <stdio.h>
#include <stdlib.h>
#include "wavfile.h"

FILE *wavOpen(char *fname)
{
    FILE *file = fopen(fname, "r");

    if (file != NULL)
    {
        // skip header
        fseek(file, sizeof(struct t_header), SEEK_SET);
        return file;
    }
    else
    {
        return NULL;
    }
}

void wavClose(FILE *file)
{
    fclose(file);
}
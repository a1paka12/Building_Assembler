#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdint.h>
#include <stdio.h>

int write_word_le(FILE *output, uint32_t word, const char *filename);

#endif

#ifndef PSEUDO_H
#define PSEUDO_H

#include "parse.h"

enum { MAX_PSEUDO_WORDS = 2 };

int pseudo_size(const struct ParsedLine *line, const struct LabelTable *labels,
                int line_number, uint32_t *size);
int expand_pseudo(const struct ParsedLine *line, const struct LabelTable *labels,
                  uint32_t pc, int line_number,
                  struct ParsedInstruction out[MAX_PSEUDO_WORDS], size_t *count);

#endif

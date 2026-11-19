#ifndef VALUES_H
#define VALUES_H

#include "parse.h"

int valid_symbol_name(const char *name);
int read_character(const char **text, unsigned char *value);
int parse_value(const char *text, const struct LabelTable *labels, int64_t *value);

#endif

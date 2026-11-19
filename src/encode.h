#ifndef ENCODE_H
#define ENCODE_H

#include <stdint.h>

#include "parse.h"

int encode_instruction(const struct ParsedInstruction *instruction, uint32_t *word);

#endif

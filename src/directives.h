#ifndef DIRECTIVES_H
#define DIRECTIVES_H

#include <stdio.h>

#include "parse.h"

enum DirectiveKind {
    DIRECTIVE_NONE,
    DIRECTIVE_TEXT,
    DIRECTIVE_DATA,
    DIRECTIVE_BSS,
    DIRECTIVE_GLOBL,
    DIRECTIVE_EQU,
    DIRECTIVE_BYTE,
    DIRECTIVE_HALF,
    DIRECTIVE_WORD,
    DIRECTIVE_ASCII,
    DIRECTIVE_ASCIZ,
    DIRECTIVE_SPACE,
    DIRECTIVE_ALIGN
};

enum DirectiveKind directive_kind(const char *name);
int directive_size(enum DirectiveKind kind, const struct ParsedLine *line,
                   int line_number, const struct LabelTable *labels,
                   uint64_t address, uint32_t *size);
int write_directive(FILE *output, const char *filename, enum DirectiveKind kind,
                    const struct ParsedLine *line, int line_number,
                    const struct LabelTable *labels);

#endif

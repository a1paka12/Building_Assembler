#ifndef DIRECTIVE_TABLE_H
#define DIRECTIVE_TABLE_H

#include "directives.h"

static const struct {
    const char *name;
    enum DirectiveKind kind;
} directives[] = {
    {".text",   DIRECTIVE_TEXT},
    {".data",   DIRECTIVE_DATA},
    {".bss",    DIRECTIVE_BSS},
    {".globl",  DIRECTIVE_GLOBL},
    {".global", DIRECTIVE_GLOBL},
    {".equ",    DIRECTIVE_EQU},
    {".byte",   DIRECTIVE_BYTE},
    {".half",   DIRECTIVE_HALF},
    {".word",   DIRECTIVE_WORD},
    {".ascii",  DIRECTIVE_ASCII},
    {".asciz",  DIRECTIVE_ASCIZ},
    {".space",  DIRECTIVE_SPACE},
    {".align",  DIRECTIVE_ALIGN}
};

#endif

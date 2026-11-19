#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#include "format.h"
#include "operands.h"

struct ParsedLine {
    const char *mnemonic;
    const char *operands[3];
    size_t operand_count;
};

struct ParsedInstruction {
    enum InstructionFormat format;
    union {
        struct RformatOperands r;
        struct IformatOperands i;
        struct SBformatOperands sb;
        struct UJformatOperands uj;
    } operands;
};

enum LineResult {
    LINE_ERROR,
    LINE_SKIP,
    LINE_OK
};

// ParsedLine의 문자열은 수정 가능한 원본 line 버퍼를 가리킨다.
enum LineResult parse_line(char *line, int line_number, struct ParsedLine *out);
enum LineResult parse_instruction(const struct ParsedLine *line, int line_number,
                                  struct ParsedInstruction *out);
#endif

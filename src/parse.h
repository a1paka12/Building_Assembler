#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#include "format.h"
#include "operands.h"

enum { LINE_CAPACITY = 256, INSTRUCTION_OPERAND_LIMIT = 3 };

struct ParsedLine {
    const char *label;
    const char *mnemonic;
    const char *operands[LINE_CAPACITY / 2];
    size_t operand_count;
};

struct ParsedInstruction {
    enum InstructionFormat format;
    const struct InstructionInfo *info;
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

struct Label {
    char name[64];
    int64_t address;
    int is_constant;
};

struct LabelTable {
    struct Label items[128];
    size_t count;
};

// ParsedLine의 문자열은 수정 가능한 원본 line 버퍼를 가리킨다.
enum LineResult parse_line(char *line, int line_number, struct ParsedLine *out);
const struct Label *find_label(const struct LabelTable *table, const char *name);
enum LineResult parse_instruction(const struct ParsedLine *line, int line_number,
                                  const struct LabelTable *labels, uint32_t pc,
                                  struct ParsedInstruction *out);

#endif

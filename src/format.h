#ifndef FORMAT_H
#define FORMAT_H

#include <stdint.h>

enum InstructionFormat {
    FORMAT_UNKNOWN,
    FORMAT_R,
    FORMAT_I,
    FORMAT_S,
    FORMAT_B,
    FORMAT_U,
    FORMAT_J,
    FORMAT_PSEUDO
};

// 포맷에 해당 비트 필드가 없으면 FIELD_UNUSED다.
enum { FIELD_UNUSED = -1 };

struct InstructionInfo {
    const char *mnemonic;
    enum InstructionFormat format;
    int opcode;
    int funct3;
    int funct7;
    uint32_t fixed_word;
};

const struct InstructionInfo *instruction_info(const char *mnemonic);
enum InstructionFormat instruction_format(const char *mnemonic);

#endif

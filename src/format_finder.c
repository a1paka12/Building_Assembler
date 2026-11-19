#include "instruction_table.h"

#include <string.h>

// 명령 이름에 해당하는 테이블 항목의 주소를 반환한다. 없으면 NULL을 반환한다.
const struct InstructionInfo *instruction_info(const char *mnemonic)
{
    for (size_t i = 0; i < sizeof instructions / sizeof instructions[0]; ++i) {
        if (strcmp(mnemonic, instructions[i].mnemonic) == 0)
            return &instructions[i];
    }
    return NULL;
}

// 테이블 항목에서 포맷을 가져온다. 등록되지 않은 이름이면 FORMAT_UNKNOWN을 반환한다.
enum InstructionFormat instruction_format(const char *mnemonic)
{
    const struct InstructionInfo *info = instruction_info(mnemonic);
    return info ? info->format : FORMAT_UNKNOWN;
}

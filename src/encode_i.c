#include "encode_i.h"
#include "format.h"

// addi의 피연산자와 명령 테이블의 opcode, funct3을 합쳐 32비트 기계어를 반환한다.
uint32_t encode_addi(struct IformatOperands operands)
{
    const struct InstructionInfo *info = instruction_info("addi");

    /* 워드 안의 I형 필드 위치: imm은 31:20, rs1은 19:15, funct3은 14:12,
     * rd는 11:7, opcode는 6:0에 들어간다.
     * 0xFFF로 마스킹하면 -2도 하위 12비트인 0xFFE가 된다. */
    return (((uint32_t)operands.imm & 0xFFFu) << 20)
          | ((uint32_t)operands.rs1 << 15)
          | ((uint32_t)info->funct3 << 12)
          | ((uint32_t)operands.rd << 7)
          | (uint32_t)info->opcode;
}

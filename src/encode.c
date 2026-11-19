#include "encode.h"

static uint32_t encode_r(const struct ParsedInstruction *instruction)
{
    const struct InstructionInfo *info = instruction->info;
    struct RformatOperands op = instruction->operands.r;

    return ((uint32_t)info->funct7 << 25)
         | ((uint32_t)op.rs2 << 20)
         | ((uint32_t)op.rs1 << 15)
         | ((uint32_t)info->funct3 << 12)
         | ((uint32_t)op.rd << 7)
         | (uint32_t)info->opcode;
}

static uint32_t encode_i(const struct ParsedInstruction *instruction)
{
    const struct InstructionInfo *info = instruction->info;
    struct IformatOperands op = instruction->operands.i;
    uint32_t imm = (uint32_t)op.imm & 0xFFFu;

    // 시프트 명령은 shamt 위에 funct7을 배치한다.
    if (info->funct7 != FIELD_UNUSED)
        imm |= (uint32_t)info->funct7 << 5;

    return (imm << 20)
         | ((uint32_t)op.rs1 << 15)
         | ((uint32_t)info->funct3 << 12)
         | ((uint32_t)op.rd << 7)
         | (uint32_t)info->opcode;
}

static uint32_t encode_s(const struct ParsedInstruction *instruction)
{
    const struct InstructionInfo *info = instruction->info;
    struct SBformatOperands op = instruction->operands.sb;
    uint32_t imm = (uint32_t)op.imm & 0xFFFu;

    // imm[11:5]는 31:25, imm[4:0]은 11:7에 들어간다.
    return ((imm >> 5) << 25)
         | ((uint32_t)op.rs2 << 20)
         | ((uint32_t)op.rs1 << 15)
         | ((uint32_t)info->funct3 << 12)
         | ((imm & 0x1Fu) << 7)
         | (uint32_t)info->opcode;
}

static uint32_t encode_b(const struct ParsedInstruction *instruction)
{
    const struct InstructionInfo *info = instruction->info;
    struct SBformatOperands op = instruction->operands.sb;
    uint32_t offset = (uint32_t)op.imm & 0x1FFFu;

    // 오프셋 비트 12, 10:5, 4:1, 11을 순서대로 배치한다.
    return ((offset >> 12) << 31)
         | (((offset >> 5) & 0x3Fu) << 25)
         | ((uint32_t)op.rs2 << 20)
         | ((uint32_t)op.rs1 << 15)
         | ((uint32_t)info->funct3 << 12)
         | (((offset >> 1) & 0xFu) << 8)
         | (((offset >> 11) & 1u) << 7)
         | (uint32_t)info->opcode;
}

static uint32_t encode_u(const struct ParsedInstruction *instruction)
{
    struct UJformatOperands op = instruction->operands.uj;
    uint32_t imm = (uint32_t)op.imm & 0xFFFFFu;

    return (imm << 12)
         | ((uint32_t)op.rd << 7)
         | (uint32_t)instruction->info->opcode;
}

static uint32_t encode_j(const struct ParsedInstruction *instruction)
{
    struct UJformatOperands op = instruction->operands.uj;
    uint32_t offset = (uint32_t)op.imm & 0x1FFFFFu;

    // 오프셋 비트 20, 10:1, 11, 19:12를 순서대로 배치한다.
    return ((offset >> 20) << 31)
         | (((offset >> 1) & 0x3FFu) << 21)
         | (((offset >> 11) & 1u) << 20)
         | (((offset >> 12) & 0xFFu) << 12)
         | ((uint32_t)op.rd << 7)
         | (uint32_t)instruction->info->opcode;
}

// 파싱과 범위 검사가 끝난 명령을 해당 포맷의 32비트 기계어로 변환한다.
int encode_instruction(const struct ParsedInstruction *instruction, uint32_t *word)
{
    static uint32_t (*const encoders[])(const struct ParsedInstruction *) = {
        [FORMAT_R] = encode_r,
        [FORMAT_I] = encode_i,
        [FORMAT_S] = encode_s,
        [FORMAT_B] = encode_b,
        [FORMAT_U] = encode_u,
        [FORMAT_J] = encode_j
    };
    unsigned int format = (unsigned int)instruction->format;

    if (instruction->info == NULL || instruction->info->format != instruction->format ||
        format >= sizeof encoders / sizeof encoders[0] || encoders[format] == NULL)
        return 1;

    *word = encoders[format](instruction);
    return 0;
}

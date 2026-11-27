#ifndef PSEUDO_TABLE_H
#define PSEUDO_TABLE_H

enum {
    ZERO_REGISTER = -1, LINK_REGISTER = -2, ZERO_IMMEDIATE = -3,
    NEGATIVE_ONE = -4, LINK_ADDRESS = -5, REGISTER_ADDRESS = -6
};

struct PseudoRule {
    const char *name;
    const char *base;
    unsigned int input_count;
    unsigned int output_count;
    int operands[3];
};

// 0~2는 입력 피연산자 번호이고, 음수는 확장할 때 추가하는 피연산자다.
static const struct PseudoRule pseudo_rules[] = {
    {"mv",    "addi",   2, 3, {0, 1, ZERO_IMMEDIATE}},
    {"nop",   "addi",   0, 3, {ZERO_REGISTER, ZERO_REGISTER, ZERO_IMMEDIATE}},
    {"not",   "xori",   2, 3, {0, 1, NEGATIVE_ONE}},
    {"neg",   "sub",    2, 3, {0, ZERO_REGISTER, 1}},
    {"j",     "jal",    1, 2, {ZERO_REGISTER, 0}},
    {"jal",   "jal",    1, 2, {LINK_REGISTER, 0}},
    {"call",  "jal",    1, 2, {LINK_REGISTER, 0}},
    {"jr",    "jalr",   1, 2, {ZERO_REGISTER, REGISTER_ADDRESS}},
    {"ret",   "jalr",   0, 2, {ZERO_REGISTER, LINK_ADDRESS}},
    {"beqz",  "beq",    2, 3, {0, ZERO_REGISTER, 1}},
    {"bnez",  "bne",    2, 3, {0, ZERO_REGISTER, 1}},
    {"bgt",   "blt",    3, 3, {1, 0, 2}},
    {"ble",   "bge",    3, 3, {1, 0, 2}},
    {"bgtu",  "bltu",   3, 3, {1, 0, 2}},
    {"bleu",  "bgeu",   3, 3, {1, 0, 2}},
    {"bgtz",  "blt",    2, 3, {ZERO_REGISTER, 0, 1}},
    {"bltz",  "blt",    2, 3, {0, ZERO_REGISTER, 1}},
    {"csrr",  "csrrs",  2, 3, {0, 1, ZERO_REGISTER}},
    {"csrw",  "csrrw",  2, 3, {ZERO_REGISTER, 0, 1}},
    {"csrs",  "csrrs",  2, 3, {ZERO_REGISTER, 0, 1}},
    {"csrsi", "csrrsi", 2, 3, {ZERO_REGISTER, 0, 1}},
    {"csrci", "csrrci", 2, 3, {ZERO_REGISTER, 0, 1}}
};

#endif

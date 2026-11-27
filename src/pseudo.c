#include "pseudo.h"
#include "pseudo_table.h"
#include "register_helper.h"
#include "values.h"

#include <stdio.h>
#include <string.h>

static const struct PseudoRule *find_rule(const char *name)
{
    for (size_t i = 0; i < sizeof pseudo_rules / sizeof pseudo_rules[0]; ++i)
        if (strcmp(name, pseudo_rules[i].name) == 0)
            return &pseudo_rules[i];
    return NULL;
}

static int check_count(const struct ParsedLine *line, size_t expected, int line_number)
{
    if (line->operand_count == expected)
        return 0;
    fprintf(stderr, "%d행: '%s' 피연산자 개수가 맞지 않음\n", line_number, line->mnemonic);
    return 1;
}

static int fits_addi(int64_t value)
{
    return value >= -2048 && value <= 2047;
}

int pseudo_size(const struct ParsedLine *line, const struct LabelTable *labels,
                int line_number, uint32_t *size)
{
    if (strcmp(line->mnemonic, "li") == 0 || strcmp(line->mnemonic, "la") == 0) {
        if (check_count(line, 2, line_number) != 0)
            return 1;

        // 앞에서 정의한 상수는 이미 수집되어 있다. 전방 라벨 주소는 12비트 범위 밖이다.
        int64_t value;
        *size = strcmp(line->mnemonic, "li") == 0 &&
                parse_value(line->operands[1], labels, &value) == 0 && fits_addi(value) ? 4 : 8;
        return 0;
    }

    const struct PseudoRule *rule = find_rule(line->mnemonic);
    if (rule == NULL || check_count(line, rule->input_count, line_number) != 0)
        return 1;
    *size = 4;
    return 0;
}

static int expand_address(const struct ParsedLine *line, const struct LabelTable *labels,
                          uint32_t pc, int line_number,
                          struct ParsedInstruction out[MAX_PSEUDO_WORDS], size_t *count)
{
    int rd = reg_number(line->operands[0]);
    int64_t value;
    int is_li = strcmp(line->mnemonic, "li") == 0;
    if (rd < 0 || (!is_li && find_label(labels, line->operands[1]) == NULL) ||
        parse_value(line->operands[1], labels, &value) != 0 ||
        value < INT32_MIN || value > UINT32_MAX) {
        fprintf(stderr, "%d행: '%s'의 레지스터 또는 값이 잘못됨\n", line_number, line->mnemonic);
        return 1;
    }

    if (is_li && fits_addi(value)) {
        out[0] = (struct ParsedInstruction){.format = FORMAT_I, .info = instruction_info("addi"),
                 .operands.i = {.rd = rd, .rs1 = 0, .imm = (int)value}};
        *count = 1;
        return 0;
    }

    // 하위 12비트를 부호 확장한다. 그 값만큼 빼면 보정된 상위 20비트가 남는다.
    uint32_t bits = (uint32_t)value;
    if (!is_li)
        bits -= pc;
    int low = (int)(bits & 0x7ffu) - (int)(bits & 0x800u);
    int high = (int)((bits - (uint32_t)low) >> 12);
    out[0] = (struct ParsedInstruction){.format = FORMAT_U,
             .info = instruction_info(is_li ? "lui" : "auipc"),
             .operands.uj = {.rd = rd, .imm = high}};
    out[1] = (struct ParsedInstruction){.format = FORMAT_I, .info = instruction_info("addi"),
             .operands.i = {.rd = rd, .rs1 = rd, .imm = low}};
    *count = 2;
    return 0;
}

int expand_pseudo(const struct ParsedLine *line, const struct LabelTable *labels,
                  uint32_t pc, int line_number,
                  struct ParsedInstruction out[MAX_PSEUDO_WORDS], size_t *count)
{
    if (strcmp(line->mnemonic, "li") == 0 || strcmp(line->mnemonic, "la") == 0) {
        if (check_count(line, 2, line_number) != 0)
            return 1;
        return expand_address(line, labels, pc, line_number, out, count);
    }

    const struct PseudoRule *rule = find_rule(line->mnemonic);
    if (rule == NULL || check_count(line, rule->input_count, line_number) != 0)
        return 1;

    static const char *const literals[] = {"x0", "ra", "0", "-1", "0(ra)"};
    struct ParsedLine base = {.mnemonic = rule->base, .operand_count = rule->output_count};
    char register_address[16];
    for (size_t i = 0; i < base.operand_count; ++i) {
        int operand = rule->operands[i];
        if (operand == REGISTER_ADDRESS) {
            if (reg_number(line->operands[0]) < 0) {
                fprintf(stderr, "%d행: '%s' 레지스터가 잘못됨\n", line_number, line->mnemonic);
                return 1;
            }
            snprintf(register_address, sizeof register_address, "0(%s)", line->operands[0]);
            base.operands[i] = register_address;
        } else {
            base.operands[i] = operand >= 0 ? line->operands[operand] : literals[-operand - 1];
        }
    }

    if (parse_instruction(&base, line_number, labels, pc, &out[0]) != LINE_OK)
        return 1;
    *count = 1;
    return 0;
}

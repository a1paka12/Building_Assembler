#include "parse.h"
#include "register_helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const whitespace = " \t\r\n";

// 따옴표 밖에서 처음 만난 구분자의 위치를 반환한다. 없으면 NULL을 반환한다.
static char *find_separator(char *text, char separator)
{
    char quote = '\0';

    for (char *p = text; *p != '\0'; ++p) {
        if (quote != '\0') {
            if (*p == '\\' && p[1] != '\0')
                ++p;
            else if (*p == quote)
                quote = '\0';
        } else if (*p == '"' || *p == '\'') {
            quote = *p;
        } else if (*p == separator) {
            return p;
        }
    }
    return NULL;
}

// 첫 주석 표시 #를 '\0'으로 바꿔 원본 줄에서 주석 부분을 잘라낸다.
static void strip_comment(char *line)
{
    char *comment = find_separator(line, '#');
    if (comment != NULL)
        *comment = '\0';
}

// 피연산자 문자열을 쉼표로 나눠 out에 담는다. 빈 항목이나 배열 초과가 있으면 -1을 반환한다.
static int parse_operands(char *text, struct ParsedLine *out)
{
    if (*text == '\0')
        return 0;

    for (;;) {
        char *comma = find_separator(text, ',');
        char *end = comma ? comma : text + strlen(text);
        while (end > text && strchr(whitespace, end[-1]) != NULL)
            --end;

        if (end == text || out->operand_count == sizeof out->operands / sizeof out->operands[0])
            return -1;

        *end = '\0';
        out->operands[out->operand_count++] = text;
        if (comma == NULL)
            return 0;

        text = comma + 1;
        text += strspn(text, whitespace);
    }
}

// 한 줄에서 주석을 떼고 명령 이름과 피연산자를 분리한다. 빈 줄이나 주석만 있으면 LINE_SKIP을 반환한다.
enum LineResult parse_line(char *line, int line_number, struct ParsedLine *out)
{
    strip_comment(line);

    char *mnemonic = line + strspn(line, whitespace);
    *out = (struct ParsedLine){0};
    if (*mnemonic == '\0')
        return LINE_SKIP;

    char *text = mnemonic + strcspn(mnemonic, whitespace);
    if (*text != '\0')
        *text++ = '\0';
    text += strspn(text, whitespace);

    out->mnemonic = mnemonic;
    if (parse_operands(text, out) != 0) {
        fprintf(stderr, "%d행: 빈 피연산자 또는 너무 많은 피연산자\n", line_number);
        return LINE_ERROR;
    }
    return LINE_OK;
}

// 명령 테이블을 조회해 피연산자를 해당 포맷의 union 멤버에 저장한다.
enum LineResult parse_instruction(const struct ParsedLine *line, int line_number,
                                  struct ParsedInstruction *out)
{
    const struct InstructionInfo *info = instruction_info(line->mnemonic);
    if (info == NULL) {
        fprintf(stderr, "%d행: 알 수 없는 명령 '%s'\n", line_number, line->mnemonic);
        return LINE_ERROR;
    }

    struct OperandLayout {
        size_t count;
        size_t register_count;
        int *fields[3];
        long min_imm;
        long max_imm;
    };

    const struct OperandLayout layouts[FORMAT_PSEUDO + 1] = {
        [FORMAT_R] = {3, 3, {
            &out->operands.r.rd, &out->operands.r.rs1, &out->operands.r.rs2
        }, 0, 0},
        [FORMAT_I] = {3, 2, {
            &out->operands.i.rd, &out->operands.i.rs1, &out->operands.i.imm
        }, -2048, 2047},
        [FORMAT_S] = {3, 2, {
            &out->operands.sb.rs2, &out->operands.sb.rs1, &out->operands.sb.imm
        }, -2048, 2047},
        [FORMAT_B] = {3, 2, {
            &out->operands.sb.rs1, &out->operands.sb.rs2, &out->operands.sb.imm
        }, -4096, 4094},
        [FORMAT_U] = {2, 1, {
            &out->operands.uj.rd, &out->operands.uj.imm
        }, -524288, 1048575},
        [FORMAT_J] = {2, 1, {
            &out->operands.uj.rd, &out->operands.uj.imm
        }, -1048576, 1048574}
    };

    const struct OperandLayout *layout = &layouts[info->format];

    // I형은 opcode로 정수 연산과 메모리 피연산자 문법을 구분한다.
    int i_alu = info->format == FORMAT_I && info->opcode == 0x13;
    int i_memory = info->format == FORMAT_I &&
                   (info->opcode == 0x03 || info->opcode == 0x67);
    int memory = info->format == FORMAT_S || i_memory;
    int shift = i_alu && info->funct7 != FIELD_UNUSED;

    if (layout->count == 0 ||
        (info->format == FORMAT_I && !i_alu && !i_memory)) {
        fprintf(stderr, "%d행: 지원하지 않는 명령 '%s'\n",
                line_number, info->mnemonic);
        return LINE_ERROR;
    }

    size_t expected_count = memory ? 2 : layout->count;
    if (line->operand_count != expected_count) {
        fprintf(stderr, "%d행: 피연산자 개수가 맞지 않음\n", line_number);
        return LINE_ERROR;
    }

    const char *operands[3] = {line->operands[0], line->operands[1], NULL};
    if (expected_count == 3)
        operands[2] = line->operands[2];

    // 두 번째 토큰 imm(rs1)을 공통 루프에서 읽을 rs1과 imm으로 나눈다.
    char base_register[16];
    const char *open = NULL;
    if (memory) {
        const char *address = operands[1];
        open = strchr(address, '(');
        int consumed = 0;
        if (open == NULL ||
            sscanf(open + 1, " %15[^) \t\r\n] ) %n", base_register, &consumed) != 1 ||
            consumed == 0 || open[1 + consumed] != '\0') {
            fprintf(stderr, "%d행: %s, imm(rs1) 형식을 기대함\n",
                    line_number, i_memory ? "rd" : "rs2");
            return LINE_ERROR;
        }
        operands[1] = base_register;
        operands[2] = address;
    }

    int values[3];
    for (size_t i = 0; i < layout->count; ++i) {
        const char *text = operands[i];
        if (i < layout->register_count) {
            values[i] = reg_number(text);
            if (values[i] < 0) {
                fprintf(stderr, "%d행: 잘못된 레지스터 '%s'\n", line_number, text);
                return LINE_ERROR;
            }
        } else {
            char *end;
            const char *digits = text + (*text == '+' || *text == '-');
            int base = info->format == FORMAT_U && digits[0] == '0' &&
                       (digits[1] == 'x' || digits[1] == 'X') ? 16 : 10;
            long imm = strtol(text, &end, base);
            int valid = end != text && *end == '\0';
            if (open != NULL)
                valid = (text == open || end != text) && end + strspn(end, whitespace) == open;
            int needs_even = info->format == FORMAT_B || info->format == FORMAT_J;
            long min_imm = shift ? 0 : layout->min_imm;
            long max_imm = shift ? 31 : layout->max_imm;
            if (!valid || imm < min_imm || imm > max_imm ||
                (needs_even && imm % 2 != 0)) {
                fprintf(stderr, "%d행: 잘못된 즉시값 '%s'\n", line_number, text);
                return LINE_ERROR;
            }
            values[i] = (int)imm;
        }
    }

    for (size_t i = 0; i < layout->count; ++i)
        *layout->fields[i] = values[i];

    out->format = info->format;
    return LINE_OK;
}

#include "directives.h"
#include "directive_table.h"
#include "output.h"
#include "values.h"

#include <string.h>

// 지시어 이름을 찾아 쓸 번호로 바꿈
enum DirectiveKind directive_kind(const char *name)
{
    for (size_t i = 0; i < sizeof directives / sizeof directives[0]; ++i) {
        if (strcmp(name, directives[i].name) == 0)
            return directives[i].kind;
    }
    return DIRECTIVE_NONE;
}

static const unsigned char data_widths[DIRECTIVE_ALIGN + 1] = {
    [DIRECTIVE_BYTE] = 1,
    [DIRECTIVE_HALF] = 2,
    [DIRECTIVE_WORD] = 4
};

static int read_string(const char *text, unsigned char *bytes, size_t *length)
{
    if (*text++ != '"')
        return 1;

    *length = 0;
    while (*text != '"') {
        unsigned char character;
        if (*length == LINE_CAPACITY - 1 || read_character(&text, &character) != 0)
            return 1;
        if (bytes != NULL)
            bytes[*length] = character;
        ++*length;
    }
    return text[1] != '\0';
}

// 크기에 영향을 주는 인수는 배치 전에 값이 확정되어야 한다.
static int read_size(const struct ParsedLine *line, const struct LabelTable *labels,
                     int64_t maximum, uint32_t *value)
{
    int64_t number;
    if (line->operand_count != 1)
        return 1;
    const struct Label *symbol = find_label(labels, line->operands[0]);
    if ((symbol != NULL && !symbol->is_constant) ||
        parse_value(line->operands[0], labels, &number) != 0 ||
        number < 0 || number > maximum)
        return 1;

    *value = (uint32_t)number;
    return 0;
}

int directive_size(enum DirectiveKind kind, const struct ParsedLine *line,
                   int line_number, const struct LabelTable *labels,
                   uint64_t address, uint32_t *size)
{
    *size = 0;
    switch (kind) {
    case DIRECTIVE_TEXT:
    case DIRECTIVE_DATA:
    case DIRECTIVE_BSS:
        if (line->operand_count == 0)
            return 0;
        break;
    case DIRECTIVE_EQU:
        if (line->operand_count == 2 && valid_symbol_name(line->operands[0]))
            return 0;
        break;
    case DIRECTIVE_GLOBL:
        if (line->operand_count == 0)
            break;
        for (size_t i = 0; i < line->operand_count; ++i) {
            if (!valid_symbol_name(line->operands[i])) {
                fprintf(stderr, "%d행: 잘못된 심볼 이름 '%s'\n", line_number, line->operands[i]);
                return 1;
            }
        }
        return 0;
    case DIRECTIVE_BYTE:
    case DIRECTIVE_HALF:
    case DIRECTIVE_WORD:
        if (line->operand_count == 0)
            break;
        if (kind == DIRECTIVE_HALF && address % 2 != 0) {
            fprintf(stderr, "%d행: .half 주소가 2바이트 경계에 정렬되지 않음\n", line_number);
            return 1;
        }
        *size = (uint32_t)(data_widths[kind] * line->operand_count);
        return 0;
    case DIRECTIVE_ASCII:
    case DIRECTIVE_ASCIZ:
        if (line->operand_count == 0)
            break;
        for (size_t i = 0; i < line->operand_count; ++i) {
            size_t length;
            if (read_string(line->operands[i], NULL, &length) != 0) {
                fprintf(stderr, "%d행: 잘못된 문자열 %s\n", line_number, line->operands[i]);
                return 1;
            }
            *size += (uint32_t)(length + (kind == DIRECTIVE_ASCIZ));
        }
        return 0;
    case DIRECTIVE_SPACE:
        if (read_size(line, labels, UINT32_MAX, size) == 0)
            return 0;
        break;
    case DIRECTIVE_ALIGN: {
        uint32_t exponent;
        if (read_size(line, labels, 31, &exponent) == 0) {
            uint32_t alignment = UINT32_C(1) << exponent;
            *size = (uint32_t)((alignment - address % alignment) % alignment);
            return 0;
        }
        break;
    }
    default:
        break;
    }

    fprintf(stderr, "%d행: 잘못된 %s 지시어 피연산자\n", line_number, line->mnemonic);
    return 1;
}

// 데이터 지시어를 리틀엔디안으로 쓴다. .space와 .align은 파일 배치에서 0으로 채운다.
int write_directive(FILE *output, const char *filename, enum DirectiveKind kind,
                    const struct ParsedLine *line, int line_number,
                    const struct LabelTable *labels)
{
    unsigned int width = data_widths[kind];
    int is_string = kind == DIRECTIVE_ASCII || kind == DIRECTIVE_ASCIZ;
    if (!is_string && width == 0)
        return 1;

    for (size_t i = 0; i < line->operand_count; ++i) {
        const char *text = line->operands[i];
        unsigned char bytes[LINE_CAPACITY];
        size_t length = width;
        if (is_string) {
            if (read_string(text, bytes, &length) != 0)
                return 1;
            if (kind == DIRECTIVE_ASCIZ)
                bytes[length++] = 0;
        } else {
            int64_t value;
            int64_t limit = INT64_C(1) << (8 * width);
            if (parse_value(text, labels, &value) != 0 || value < -(limit / 2) || value >= limit) {
                fprintf(stderr, "%d행: 잘못된 %s 값 또는 정의되지 않은 라벨 '%s'\n",
                        line_number, line->mnemonic, text);
                return 1;
            }
            uint32_t word = (uint32_t)value;
            if (width == 4) {
                if (write_word_le(output, word, filename) != 0)
                    return 1;
                continue;
            }
            bytes[0] = (unsigned char)word;
            if (width == 2)
                bytes[1] = (unsigned char)(word >> 8);
        }
        if (fwrite(bytes, 1, length, output) != length) {
            perror(filename);
            return 1;
        }
    }
    return 0;
}

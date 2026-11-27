#include "layout.h"
#include "pseudo.h"
#include "values.h"

#include <string.h>

// 섹션 지시어는 바이트를 차지하지 않고 커서만 바꾼다.
static int select_section(enum DirectiveKind directive, enum Section *section)
{
    switch (directive) {
    case DIRECTIVE_TEXT: *section = SECTION_TEXT; return 1;
    case DIRECTIVE_DATA: *section = SECTION_DATA; return 1;
    case DIRECTIVE_BSS:  *section = SECTION_BSS; return 1;
    default: return 0;
    }
}

// 두 패스에서 같은 규칙으로 섹션 내 위치와 한 줄의 크기를 구한다.
int layout_line(struct SectionCursor *cursor, const struct ParsedLine *line,
                int line_number, const struct LabelTable *labels, struct LineLayout *out)
{
    enum DirectiveKind directive = directive_kind(line->mnemonic);
    uint32_t size;
    int pseudo = 0;
    if (directive != DIRECTIVE_NONE) {
        int section_change = select_section(directive, &cursor->section);
        if (cursor->section == SECTION_BSS && !section_change &&
            directive != DIRECTIVE_SPACE && directive != DIRECTIVE_ALIGN &&
            directive != DIRECTIVE_EQU && directive != DIRECTIVE_GLOBL) {
            fprintf(stderr, "%d행: .bss에는 초기값을 넣을 수 없음\n", line_number);
            return 1;
        }
        uint64_t address = (uint64_t)cursor->starts[cursor->section] + cursor->offsets[cursor->section];
        if (directive_size(directive, line, line_number, labels, address, &size) != 0)
            return 1;
    } else {
        const struct InstructionInfo *info = instruction_info(line->mnemonic);
        if (info == NULL) {
            fprintf(stderr, "%d행: 지원하지 않는 명령 또는 지시어 '%s'\n",
                    line_number, line->mnemonic);
            return 1;
        }
        if (cursor->section != SECTION_TEXT || cursor->offsets[SECTION_TEXT] % 4 != 0) {
            fprintf(stderr, "%d행: 명령어는 .text의 4바이트 정렬 주소에 있어야 함\n",
                    line_number);
            return 1;
        }
        size = 4;
        pseudo = info->format == FORMAT_PSEUDO ||
                 (strcmp(info->mnemonic, "jal") == 0 && line->operand_count == 1);
        if (pseudo && pseudo_size(line, labels, line_number, &size) != 0)
            return 1;
    }

    uint32_t *offset = &cursor->offsets[cursor->section];
    if ((uint64_t)cursor->starts[cursor->section] + *offset + size > UINT32_MAX) {
        fprintf(stderr, "%d행: 섹션 크기 초과\n", line_number);
        return 1;
    }
    *out = (struct LineLayout){
        .directive = directive, .offset = *offset, .size = size, .pseudo = pseudo
    };
    *offset += size;
    return 0;
}

static int add_symbol(struct LabelTable *labels, const char *name, int64_t value,
                      int is_constant, int line_number)
{
    if (find_label(labels, name) != NULL) {
        fprintf(stderr, "%d행: 중복 라벨 또는 상수 '%s'\n", line_number, name);
        return 1;
    }
    if (labels->count == sizeof labels->items / sizeof labels->items[0] ||
        strlen(name) >= sizeof labels->items[0].name) {
        fprintf(stderr, "%d행: 심볼 표 용량 또는 이름 길이 초과\n", line_number);
        return 1;
    }

    struct Label *label = &labels->items[labels->count++];
    strcpy(label->name, name);
    label->address = value;
    label->is_constant = is_constant;
    return 0;
}

static int rewind_source(FILE *input)
{
    if (ferror(input) || fseek(input, 0, SEEK_SET) != 0) {
        perror("입력 파일");
        return 1;
    }
    return 0;
}

// 섹션 크기를 결정할 때 쓸 .equ 값부터 등록한다.
static int collect_constants(FILE *input, struct LabelTable *labels)
{
    char line[LINE_CAPACITY];
    int line_number = 0;
    while (fgets(line, sizeof line, input) != NULL) {
        struct ParsedLine parsed;
        if (parse_line(line, ++line_number, &parsed) == LINE_ERROR)
            return 1;
        if (parsed.mnemonic == NULL || directive_kind(parsed.mnemonic) != DIRECTIVE_EQU)
            continue;

        uint32_t size;
        if (directive_size(DIRECTIVE_EQU, &parsed, line_number, labels, 0, &size) != 0)
            return 1;

        int64_t value;
        if (parse_value(parsed.operands[1], labels, &value) != 0 ||
            value < INT32_MIN || value > UINT32_MAX) {
            fprintf(stderr, "%d행: 잘못된 .equ 값 '%s'\n", line_number, parsed.operands[1]);
            return 1;
        }
        if (add_symbol(labels, parsed.operands[0], value, 1, line_number) != 0)
            return 1;
    }
    return rewind_source(input);
}

// 절대 주소에서 .align을 계산하기 위해 시작 주소가 정해진 섹션부터 배치한다.
static int collect_section(FILE *input, struct LabelTable *labels, enum Section target,
                           uint32_t start, uint32_t *size)
{
    struct SectionCursor cursor = {0};
    cursor.starts[target] = start;
    char line[LINE_CAPACITY];
    int line_number = 0;

    while (fgets(line, sizeof line, input) != NULL) {
        struct ParsedLine parsed;
        if (parse_line(line, ++line_number, &parsed) == LINE_ERROR)
            return 1;

        if (cursor.section == target && parsed.label != NULL &&
            add_symbol(labels, parsed.label, start + cursor.offsets[target], 0, line_number) != 0)
            return 1;
        if (parsed.mnemonic == NULL)
            continue;

        if (cursor.section == target) {
            struct LineLayout item;
            if (layout_line(&cursor, &parsed, line_number, labels, &item) != 0)
                return 1;
        } else {
            select_section(directive_kind(parsed.mnemonic), &cursor.section);
        }
    }
    *size = cursor.offsets[target];
    return rewind_source(input);
}

static uint64_t next_section(uint64_t address)
{
    return (address + DATA_ALIGNMENT - 1) & ~((uint64_t)DATA_ALIGNMENT - 1);
}

int collect_layout(FILE *input, struct AssemblyLayout *layout)
{
    *layout = (struct AssemblyLayout){0};
    struct LabelTable *labels = &layout->labels;
    if (add_symbol(labels, "TEXT_START", TEXT_START, 0, 0) != 0 ||
        add_symbol(labels, "_stack_top", STACK_TOP, 0, 0) != 0 ||
        collect_constants(input, labels) != 0 ||
        collect_section(input, labels, SECTION_TEXT, TEXT_START, &layout->text_size) != 0)
        return 1;

    uint64_t data_start = next_section((uint64_t)TEXT_START + layout->text_size);
    if (data_start > UINT32_MAX) {
        fprintf(stderr, ".data 시작 주소가 32비트 범위를 벗어남\n");
        return 1;
    }
    layout->data_offset = (uint32_t)data_start - TEXT_START;
    if (add_symbol(labels, "_data_start", (int64_t)data_start, 0, 0) != 0 ||
        collect_section(input, labels, SECTION_DATA, (uint32_t)data_start, &layout->data_size) != 0)
        return 1;

    uint64_t bss_start = next_section(data_start + layout->data_size);
    if (bss_start > UINT32_MAX) {
        fprintf(stderr, ".bss 시작 주소가 32비트 범위를 벗어남\n");
        return 1;
    }
    layout->bss_offset = (uint32_t)bss_start - TEXT_START;
    if (add_symbol(labels, "_bss_start", (int64_t)bss_start, 0, 0) != 0 ||
        collect_section(input, labels, SECTION_BSS, (uint32_t)bss_start, &layout->bss_size) != 0 ||
        add_symbol(labels, "_bss_end", (int64_t)bss_start + layout->bss_size, 0, 0) != 0)
        return 1;
    return 0;
}

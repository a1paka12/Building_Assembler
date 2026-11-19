#ifndef LAYOUT_H
#define LAYOUT_H

#include "directives.h"

enum { TEXT_START = 0x10000, STACK_TOP = 0x40000, DATA_ALIGNMENT = 16 };

enum Section {
    SECTION_TEXT,
    SECTION_DATA,
    SECTION_BSS,
    SECTION_COUNT
};

struct SectionCursor {
    enum Section section;
    uint32_t starts[SECTION_COUNT];
    uint32_t offsets[SECTION_COUNT];
};

struct LineLayout {
    enum DirectiveKind directive;
    uint32_t offset;
    uint32_t size;
};

struct AssemblyLayout {
    struct LabelTable labels;
    uint32_t text_size;
    uint32_t data_size;
    uint32_t data_offset;
    uint32_t bss_size;
    uint32_t bss_offset;
};

int layout_line(struct SectionCursor *cursor, const struct ParsedLine *line,
                int line_number, const struct LabelTable *labels, struct LineLayout *out);
int collect_layout(FILE *input, struct AssemblyLayout *layout);

#endif

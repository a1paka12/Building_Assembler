#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "encode.h"
#include "format.h"
#include "layout.h"
#include "output.h"
#include "parse.h"
#include "pseudo.h"
#include "register_helper.h"

static void test_registers(void)
{
    const char *aliases[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
        "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
        "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"
    };

    for (int i = 0; i < 32; ++i) {
        char name[8];
        snprintf(name, sizeof name, "x%d", i);
        assert(reg_number(name) == i);
        assert(reg_number(aliases[i]) == i);
    }
    assert(reg_number("fp") == 8);
    assert(reg_number("x32") == -1);
    assert(reg_number("x01") == -1);
}

static void test_instruction_lookup(void)
{
    const struct InstructionInfo *sub = instruction_info("sub");
    assert(sub != NULL);
    assert(sub->format == FORMAT_R);
    assert(sub->opcode == 0x33 && sub->funct3 == 0 && sub->funct7 == 0x20);
    assert(instruction_format("addi") == FORMAT_I);
    assert(instruction_format("sw") == FORMAT_S);
    assert(instruction_format("beq") == FORMAT_B);
    assert(instruction_format("lui") == FORMAT_U);
    assert(instruction_format("jal") == FORMAT_J);
    assert(instruction_format("la") == FORMAT_PSEUDO);

    const struct InstructionInfo *mret = instruction_info("mret");
    assert(mret != NULL && mret->fixed_word == 0x30200073u);
    assert(instruction_info("unknown") == NULL);
    assert(instruction_format("unknown") == FORMAT_UNKNOWN);
}

// 대괄호 하나에 피연산자 하나를 표시해 분리된 위치를 확인한다.
static void print_parsed_line(const struct ParsedLine *line)
{
    printf("명령 %s | 피연산자 %zu개", line->mnemonic, line->operand_count);
    for (size_t i = 0; i < line->operand_count; ++i)
        printf(" [%s]", line->operands[i]);
    putchar('\n');
}

static void test_parser(void)
{
    struct ParsedLine line;
    struct ParsedInstruction instruction;
    char valid[] = "\taddi fp, x31, -2048 # comment";
    assert(parse_line(valid, 1, &line) == LINE_OK);
    assert(strcmp(line.mnemonic, "addi") == 0 && line.operand_count == 3);
    assert(strcmp(line.operands[0], "fp") == 0);
    assert(strcmp(line.operands[1], "x31") == 0);
    assert(strcmp(line.operands[2], "-2048") == 0);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 1, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_I);
    assert(instruction.operands.i.rd == 8 && instruction.operands.i.rs1 == 31);
    assert(instruction.operands.i.imm == -2048);
    printf("  매핑 I형 | rd=%d rs1=%d imm=%d\n",
           instruction.operands.i.rd, instruction.operands.i.rs1,
           instruction.operands.i.imm);

    char upper_bound[] = "addi x1, x0, 2047";
    assert(parse_line(upper_bound, 2, &line) == LINE_OK);
    assert(parse_instruction(&line, 2, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.operands.i.imm == 2047);

    char blank[] = " \t\r\n";
    char comment[] = "  # comment";
    assert(parse_line(blank, 3, &line) == LINE_SKIP);
    assert(parse_line(comment, 4, &line) == LINE_SKIP);

    // 오류를 기대하는 입력이라 아래 호출은 진단 메시지도 출력한다.
    char too_large[] = "addi x1, x0, 2048";
    char missing_operand[] = "addi x1, x0";
    assert(parse_line(too_large, 5, &line) == LINE_OK);
    assert(parse_instruction(&line, 5, NULL, 0, &instruction) == LINE_ERROR);
    assert(parse_line(missing_operand, 6, &line) == LINE_OK);
    assert(parse_instruction(&line, 6, NULL, 0, &instruction) == LINE_ERROR);

    char memory[] = "lw t0 , 4(sp)";
    assert(parse_line(memory, 7, &line) == LINE_OK && line.operand_count == 2);
    assert(strcmp(line.operands[0], "t0") == 0);
    assert(strcmp(line.operands[1], "4(sp)") == 0);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 7, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_I && instruction.operands.i.imm == 4);
    assert(instruction.operands.i.rd == 5 && instruction.operands.i.rs1 == 2);
    printf("  매핑 I형 | rd=%d rs1=%d imm=%d\n",
           instruction.operands.i.rd, instruction.operands.i.rs1,
           instruction.operands.i.imm);

    char quoted[] = ".asciz \"a,#b\" # comment";
    assert(parse_line(quoted, 8, &line) == LINE_OK && line.operand_count == 1);
    assert(strcmp(line.operands[0], "\"a,#b\"") == 0);
    print_parsed_line(&line);

    char empty_operand[] = "addi t0,, -2";
    char trailing_comma[] = "addi t0, t1,";
    char too_many[] = "addi t0, t1, -2, t2";
    assert(parse_line(empty_operand, 9, &line) == LINE_ERROR);
    assert(parse_line(trailing_comma, 10, &line) == LINE_ERROR);
    assert(parse_line(too_many, 11, &line) == LINE_ERROR);

    char r[] = "add t0, t1, t2";
    assert(parse_line(r, 12, &line) == LINE_OK);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 12, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_R);
    assert(instruction.operands.r.rd == 5 && instruction.operands.r.rs1 == 6);
    assert(instruction.operands.r.rs2 == 7);
    printf("  매핑 R형 | rd=%d rs1=%d rs2=%d\n",
           instruction.operands.r.rd, instruction.operands.r.rs1,
           instruction.operands.r.rs2);

    char s[] = "sw t0, -4(sp)";
    assert(parse_line(s, 13, &line) == LINE_OK);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 13, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_S && instruction.operands.sb.imm == -4);
    assert(instruction.operands.sb.rs1 == 2 && instruction.operands.sb.rs2 == 5);
    printf("  매핑 S형 | rs2=%d rs1=%d imm=%d\n",
           instruction.operands.sb.rs2, instruction.operands.sb.rs1,
           instruction.operands.sb.imm);
    char s_no_offset[] = "sw t0, (sp)";
    assert(parse_line(s_no_offset, 14, &line) == LINE_OK);
    assert(parse_instruction(&line, 14, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.operands.sb.imm == 0);

    char b[] = "beq a0, a1, -4096";
    assert(parse_line(b, 15, &line) == LINE_OK);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 15, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_B && instruction.operands.sb.imm == -4096);
    assert(instruction.operands.sb.rs1 == 10 && instruction.operands.sb.rs2 == 11);
    printf("  매핑 B형 | rs1=%d rs2=%d offset=%d\n",
           instruction.operands.sb.rs1, instruction.operands.sb.rs2,
           instruction.operands.sb.imm);

    char u[] = "lui t0, 0xFF200";
    assert(parse_line(u, 16, &line) == LINE_OK);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 16, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_U && instruction.operands.uj.imm == 0xFF200);
    assert(instruction.operands.uj.rd == 5);
    printf("  매핑 U형 | rd=%d imm=0x%X\n", instruction.operands.uj.rd,
           (unsigned int)instruction.operands.uj.imm);

    char j[] = "jal ra, 1048574";
    assert(parse_line(j, 17, &line) == LINE_OK);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 17, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_J && instruction.operands.uj.imm == 1048574);
    assert(instruction.operands.uj.rd == 1);
    printf("  매핑 J형 | rd=%d offset=%d\n",
           instruction.operands.uj.rd, instruction.operands.uj.imm);

    char odd_jump[] = "jal ra, 3";
    assert(parse_line(odd_jump, 18, &line) == LINE_OK);
    assert(parse_instruction(&line, 18, NULL, 0, &instruction) == LINE_ERROR);
    char bad_address[] = "sw t0, 4(sp";
    assert(parse_line(bad_address, 19, &line) == LINE_OK);
    assert(parse_instruction(&line, 19, NULL, 0, &instruction) == LINE_ERROR);

    char alu[] = "andi t0, t1, -2";
    assert(parse_line(alu, 20, &line) == LINE_OK);
    assert(parse_instruction(&line, 20, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.operands.i.rd == 5 && instruction.operands.i.rs1 == 6);
    assert(instruction.operands.i.imm == -2);

    char jalr[] = "jalr ra, (sp)";
    assert(parse_line(jalr, 21, &line) == LINE_OK);
    assert(parse_instruction(&line, 21, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.operands.i.rd == 1 && instruction.operands.i.rs1 == 2);
    assert(instruction.operands.i.imm == 0);

    char shift_max[] = "srai t0, t1, 31";
    char shift_too_large[] = "srai t0, t1, 32";
    char shift_negative[] = "srai t0, t1, -1";
    assert(parse_line(shift_max, 22, &line) == LINE_OK);
    assert(parse_instruction(&line, 22, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.operands.i.imm == 31);
    assert(parse_line(shift_too_large, 23, &line) == LINE_OK);
    assert(parse_instruction(&line, 23, NULL, 0, &instruction) == LINE_ERROR);
    assert(parse_line(shift_negative, 24, &line) == LINE_OK);
    assert(parse_instruction(&line, 24, NULL, 0, &instruction) == LINE_ERROR);

    char system[] = "mret";
    assert(parse_line(system, 25, &line) == LINE_OK);
    assert(parse_instruction(&line, 25, NULL, 0, &instruction) == LINE_ERROR);

    // 이스케이프된 따옴표 뒤의 #과 쉼표도 문자열 내부에 남는다.
    char escaped[] = ".asciz \"a\\\"#,b\" # 밖의 주석";
    assert(parse_line(escaped, 26, &line) == LINE_OK);
    assert(strcmp(line.mnemonic, ".asciz") == 0 && line.operand_count == 1);
    assert(strcmp(line.operands[0], "\"a\\\"#,b\"") == 0);

    // S형의 홀수 즉시값과 B형의 가장 큰 짝수 오프셋은 유효하다.
    char odd_store[] = "sb t0, 1(sp)";
    assert(parse_line(odd_store, 27, &line) == LINE_OK);
    assert(parse_instruction(&line, 27, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_S && instruction.operands.sb.imm == 1);

    char branch_max[] = "beq a0, a1, 4094";
    assert(parse_line(branch_max, 28, &line) == LINE_OK);
    assert(parse_instruction(&line, 28, NULL, 0, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_B && instruction.operands.sb.imm == 4094);

    // 토큰 분리에는 성공해도 명령 또는 피연산자 검증에서는 거부해야 한다.
    char invalid[][40] = {
        "unknown t0",
        "add t0, x32, t2",
        "addi t0, t1, -2extra",
        "lw t0, 4(sp)extra",
        "sw t0, 2048(sp)",
        "beq a0, a1, 4096",
        "beq a0, a1, 3",
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        int line_number = 29 + (int)i;
        assert(parse_line(invalid[i], line_number, &line) == LINE_OK);
        assert(parse_instruction(&line, line_number, NULL, 0, &instruction) == LINE_ERROR);
    }
}

static void test_encoder(void)
{
    const struct {
        struct InstructionInfo info;
        struct ParsedInstruction instruction;
        uint32_t expected;
    } cases[] = {
        {
            .info = {.mnemonic = "sub", .format = FORMAT_R, .opcode = 0x33,
                     .funct3 = 0, .funct7 = 0x20},
            .instruction = {.operands.r = {.rd = 7, .rs1 = 5, .rs2 = 6}},
            .expected = 0x406283b3u,
        },
        {
            .info = {.mnemonic = "srai", .format = FORMAT_I, .opcode = 0x13,
                     .funct3 = 5, .funct7 = 0x20},
            .instruction = {.operands.i = {.rd = 5, .rs1 = 6, .imm = 31}},
            .expected = 0x41f35293u,
        },
        {
            .info = {.mnemonic = "sw", .format = FORMAT_S, .opcode = 0x23,
                     .funct3 = 2, .funct7 = FIELD_UNUSED},
            .instruction = {.operands.sb = {.rs1 = 2, .rs2 = 7, .imm = -4}},
            .expected = 0xfe712e23u,
        },
        {
            .info = {.mnemonic = "beq", .format = FORMAT_B, .opcode = 0x63,
                     .funct3 = 0, .funct7 = FIELD_UNUSED},
            .instruction = {.operands.sb = {.rs1 = 10, .rs2 = 11, .imm = 2050}},
            .expected = 0x00b501e3u,
        },
        {
            .info = {.mnemonic = "lui", .format = FORMAT_U, .opcode = 0x37,
                     .funct3 = FIELD_UNUSED, .funct7 = FIELD_UNUSED},
            .instruction = {.operands.uj = {.rd = 5, .imm = 0xff200}},
            .expected = 0xff2002b7u,
        },
        {
            .info = {.mnemonic = "jal", .format = FORMAT_J, .opcode = 0x6f,
                     .funct3 = FIELD_UNUSED, .funct7 = FIELD_UNUSED},
            .instruction = {.operands.uj = {.rd = 1, .imm = -12}},
            .expected = 0xff5ff0efu,
        },
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        struct ParsedInstruction instruction = cases[i].instruction;
        instruction.info = &cases[i].info;
        instruction.format = cases[i].info.format;
        uint32_t word;
        assert(encode_instruction(&instruction, &word) == 0);
        assert(word == cases[i].expected);
        printf("인코더 %s | 0x%08" PRIX32 "\n", instruction.info->mnemonic, word);
    }
}

static void test_parse_and_encode(void)
{
    struct {
        char text[36];
        uint32_t expected;
    } cases[] = {
        {"add t2, t0, t1",           0x006283b3u},
        {"sub t2, t0, t1",           0x406283b3u},
        {"mul t2, t0, t1",           0x026283b3u},
        {"addi t0, zero, 7",         0x00700293u},
        {"addi t1, x0, -2",          0xffe00313u},
        {"addi t1, t1, 0",           0x00030313u},
        {"andi t0, t1, -2",          0xffe37293u},
        {"srai t0, t1, 31",          0x41f35293u},
        {"lw a0, -4(sp)",            0xffc12503u},
        {"jalr ra, (sp)",            0x000100e7u},
        {"sw t0, 8(sp)",             0x00512423u},
        {"sw t2, -4(sp)",            0xfe712e23u},
        {"beq a0, a1, 8",            0x00b50463u},
        {"beq x0, x0, -4096",        0x80000063u},
        {"beq x0, x0, 4094",         0x7e000fe3u},
        {"lui t0, 0xFF200",          0xff2002b7u},
        {"auipc sp, 1",              0x00001117u},
        {"jal ra, -1048576",         0x800000efu},
        {"jal ra, 1048574",          0x7ffff0efu},
    };

    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        struct ParsedLine line;
        struct ParsedInstruction instruction;
        uint32_t word;
        int line_number = (int)i + 1;
        assert(parse_line(cases[i].text, line_number, &line) == LINE_OK);
        assert(parse_instruction(&line, line_number, NULL, 0, &instruction) == LINE_OK);
        assert(encode_instruction(&instruction, &word) == 0);
        assert(word == cases[i].expected);
    }
}

static void test_pseudos(void)
{
    const struct LabelTable labels = {
        .items = {{"target", 0x10800, 0}}, .count = 1
    };
    struct {
        char text[32];
        uint32_t words[MAX_PSEUDO_WORDS];
        size_t count;
    } cases[] = {
        {"li t0, -2048",     {0x80000293u}, 1},
        {"li t0, 2047",      {0x7ff00293u}, 1},
        {"li t0, 2048",      {0x000012b7u, 0x80028293u}, 2},
        {"li t0, 0x12345fff",{0x123462b7u, 0xfff28293u}, 2},
        {"li t0, 0xffffffff",{0x000002b7u, 0xfff28293u}, 2},
        {"la t0, target",    {0x00001297u, 0x80028293u}, 2},
        {"mv t0, t1",        {0x00030293u}, 1},
        {"nop",              {0x00000013u}, 1},
        {"not t0, t1",       {0xfff34293u}, 1},
        {"neg t0, t1",       {0x406002b3u}, 1},
        {"j 8",              {0x0080006fu}, 1},
        {"jal 8",            {0x008000efu}, 1},
        {"call 8",           {0x008000efu}, 1},
        {"jr t0",            {0x00028067u}, 1},
        {"ret",              {0x00008067u}, 1},
        {"beqz t0, 8",       {0x00028463u}, 1},
        {"bnez t0, 8",       {0x00029463u}, 1},
        {"bgt t0, t1, 8",    {0x00534463u}, 1},
        {"ble t0, t1, 8",    {0x00535463u}, 1},
        {"bgtu t0, t1, 8",   {0x00536463u}, 1},
        {"bleu t0, t1, 8",   {0x00537463u}, 1},
        {"bgtz t0, 8",       {0x00504463u}, 1},
        {"bltz t0, 8",       {0x0002c463u}, 1},
        {"csrr t0, mstatus", {0x300022f3u}, 1},
        {"csrw mtvec, t0",   {0x30529073u}, 1},
        {"csrs mie, t0",     {0x3042a073u}, 1},
        {"csrsi mstatus, 8", {0x30046073u}, 1},
        {"csrci mstatus, 8", {0x30047073u}, 1}
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        struct ParsedLine line;
        struct ParsedInstruction result[MAX_PSEUDO_WORDS];
        uint32_t size, word;
        size_t count;
        int number = (int)i + 1;
        assert(parse_line(cases[i].text, number, &line) == LINE_OK);
        assert(pseudo_size(&line, &labels, number, &size) == 0);
        assert(expand_pseudo(&line, &labels, 0x10000, number, result, &count) == 0);
        assert(count == cases[i].count && size == count * 4);
        for (size_t j = 0; j < count; ++j) {
            assert(encode_instruction(&result[j], &word) == 0);
            assert(word == cases[i].words[j]);
        }
    }

    char invalid[][32] = {"li x32, 0", "li t0, 0x100000000", "la t0, missing",
                          "beqz t0, 3", "csrsi mstatus, 32", "csrw 0x1000, t0"};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        struct ParsedLine line;
        struct ParsedInstruction result[MAX_PSEUDO_WORDS];
        size_t count;
        assert(parse_line(invalid[i], 1, &line) == LINE_OK);
        assert(expand_pseudo(&line, &labels, 0x10000, 1, result, &count) != 0);
    }
}

static void test_labels(void)
{
    const struct LabelTable labels = {
        .items = {{"start", 0x10000, 0}, {"done", 0x10008, 0}, {"far", 0x11000, 0}},
        .count = 3
    };
    struct ParsedLine line;
    struct ParsedInstruction instruction;
    uint32_t word;

    char branch[] = "start: beq t0, t1, done";
    assert(parse_line(branch, 1, &line) == LINE_OK);
    assert(strcmp(line.label, "start") == 0 && strcmp(line.mnemonic, "beq") == 0);
    assert(strcmp(line.operands[2], "done") == 0);
    assert(parse_instruction(&line, 1, &labels, 0x10000, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_B && instruction.operands.sb.imm == 8);
    assert(encode_instruction(&instruction, &word) == 0 && word == 0x00628463u);
    printf("  라벨 매핑 B형 | done offset=%d\n", instruction.operands.sb.imm);

    char label_only[] = "done: # label";
    assert(parse_line(label_only, 3, &line) == LINE_OK);
    assert(strcmp(line.label, "done") == 0 && line.mnemonic == NULL);
    assert(parse_instruction(&line, 3, &labels, 0x10008, &instruction) == LINE_SKIP);

    char jump[] = "jal ra, start";
    assert(parse_line(jump, 5, &line) == LINE_OK);
    assert(parse_instruction(&line, 5, &labels, 0x1000c, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_J && instruction.operands.uj.imm == -12);
    assert(encode_instruction(&instruction, &word) == 0 && word == 0xff5ff0efu);
    printf("  라벨 매핑 J형 | start offset=%d\n", instruction.operands.uj.imm);

    char missing[] = "jal ra, missing";
    assert(parse_line(missing, 6, &line) == LINE_OK);
    assert(parse_instruction(&line, 6, &labels, 0x10010, &instruction) == LINE_ERROR);

    char far[] = "beq t0, t1, far";
    assert(parse_line(far, 7, &line) == LINE_OK);
    assert(parse_instruction(&line, 7, &labels, 0x10000, &instruction) == LINE_ERROR);

    char invalid[] = "1start: addi x1, x0, 0";
    assert(parse_line(invalid, 8, &line) == LINE_ERROR);
}

static void test_directives_and_layout(void)
{
    assert(directive_kind(".bss") == DIRECTIVE_BSS);
    assert(directive_kind(".globl") == DIRECTIVE_GLOBL);
    assert(directive_kind(".global") == DIRECTIVE_GLOBL);
    assert(directive_kind(".equ") == DIRECTIVE_EQU);
    assert(directive_kind(".byte") == DIRECTIVE_BYTE);
    assert(directive_kind(".half") == DIRECTIVE_HALF);
    assert(directive_kind(".ascii") == DIRECTIVE_ASCII);
    assert(directive_kind(".asciz") == DIRECTIVE_ASCIZ);
    assert(directive_kind(".space") == DIRECTIVE_SPACE);
    assert(directive_kind(".align") == DIRECTIVE_ALIGN);
    assert(directive_kind(".section") == DIRECTIVE_NONE);

    FILE *input = tmpfile();
    assert(input != NULL);
    assert(fputs(".data\n"
                 "values: .word entry, values, again, -1, 0b1010\n"
                 ".text\n"
                 "entry: addi x1, x0, 7\n"
                 ".data\n"
                 "more: .word 0xFFFFFFFF\n"
                 ".text\n"
                 "again: jal x0, entry\n", input) >= 0);
    assert(fseek(input, 0, SEEK_SET) == 0);

    struct AssemblyLayout layout;
    assert(collect_layout(input, &layout) == 0);
    assert(layout.text_size == 8 && layout.data_offset == 16 && layout.data_size == 24);
    assert(layout.labels.count == 9);
    assert(find_label(&layout.labels, "entry")->address == 0x10000);
    assert(find_label(&layout.labels, "again")->address == 0x10004);
    assert(find_label(&layout.labels, "values")->address == 0x10010);
    assert(find_label(&layout.labels, "more")->address == 0x10024);

    struct ParsedLine parsed;
    char line[LINE_CAPACITY];
    assert(fgets(line, sizeof line, input) != NULL && strcmp(line, ".data\n") == 0);
    assert(fgets(line, sizeof line, input) != NULL);
    assert(parse_line(line, 2, &parsed) == LINE_OK && parsed.operand_count == 5);
    assert(fclose(input) == 0);

    FILE *output = tmpfile();
    assert(output != NULL);
    assert(write_directive(output, "unit test", DIRECTIVE_WORD,
                           &parsed, 2, &layout.labels) == 0);
    assert(fseek(output, 0, SEEK_SET) == 0);
    const unsigned char expected[] = {
        0x00, 0x00, 0x01, 0x00, 0x10, 0x00, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00,
        0xff, 0xff, 0xff, 0xff, 0x0a, 0x00, 0x00, 0x00
    };
    unsigned char actual[sizeof expected];
    assert(fread(actual, 1, sizeof actual, output) == sizeof actual);
    assert(memcmp(actual, expected, sizeof expected) == 0);
    assert(fgetc(output) == EOF && feof(output));
    assert(fclose(output) == 0);
}

static void test_data_directives(void)
{
    FILE *input = tmpfile();
    assert(input != NULL);
    assert(fputs(".equ COUNT, 3\n"
                 ".global entry\n"
                 ".globl entry\n"
                 "entry: addi x1, x0, COUNT\n"
                 ".data\n"
                 ".byte 'A'\n"
                 ".align 5\n"
                 ".half 0x1234\n"
                 ".ascii \"a,#\"\n"
                 ".asciz \"b\\n\"\n"
                 ".space COUNT\n"
                 ".bss\n"
                 "buffer: .space COUNT\n"
                 ".align 4\n"
                 "tail: .space 2\n", input) >= 0);
    assert(fseek(input, 0, SEEK_SET) == 0);

    struct AssemblyLayout layout;
    assert(collect_layout(input, &layout) == 0);
    assert(layout.text_size == 4 && layout.data_offset == 16 && layout.data_size == 27);
    assert(layout.bss_offset == 48 && layout.bss_size == 18);
    assert(find_label(&layout.labels, "COUNT")->is_constant);
    assert(find_label(&layout.labels, "_data_start")->address == 0x10010);
    assert(find_label(&layout.labels, "buffer")->address == 0x10030);
    assert(find_label(&layout.labels, "tail")->address == 0x10040);
    assert(find_label(&layout.labels, "_bss_end")->address == 0x10042);
    assert(fclose(input) == 0);

    struct ParsedLine line;
    struct ParsedInstruction instruction;
    char immediate[] = "addi x1, x0, COUNT";
    assert(parse_line(immediate, 1, &line) == LINE_OK);
    assert(parse_instruction(&line, 1, &layout.labels, TEXT_START, &instruction) == LINE_OK);
    assert(instruction.operands.i.imm == 3);

    char memory[] = "sb x1, COUNT(sp)";
    assert(parse_line(memory, 2, &line) == LINE_OK);
    assert(parse_instruction(&line, 2, &layout.labels, TEXT_START, &instruction) == LINE_OK);
    assert(instruction.operands.sb.imm == 3);
}

static void test_binary_output(void)
{
    FILE *output = tmpfile();
    assert(output != NULL);
    assert(write_word_le(output, 0x00700293u, "unit test") == 0);
    assert(write_word_le(output, 0xffe00313u, "unit test") == 0);
    assert(fseek(output, 0, SEEK_SET) == 0);

    const unsigned char expected[] = {0x93, 0x02, 0x70, 0x00, 0x13, 0x03, 0xe0, 0xff};
    unsigned char actual[sizeof expected];
    assert(fread(actual, 1, sizeof actual, output) == sizeof actual);
    assert(memcmp(actual, expected, sizeof expected) == 0);
    assert(fgetc(output) == EOF && feof(output));
    assert(fclose(output) == 0);
}

int main(void)
{
    test_registers();
    test_instruction_lookup();
    test_parser();
    test_labels();
    test_encoder();
    test_parse_and_encode();
    test_pseudos();
    test_directives_and_layout();
    test_data_directives();
    test_binary_output();
    return 0;
}

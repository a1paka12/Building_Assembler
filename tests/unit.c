#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "encode_i.h"
#include "format.h"
#include "output.h"
#include "parse.h"
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
    assert(parse_instruction(&line, 1, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_I);
    assert(instruction.operands.i.rd == 8 && instruction.operands.i.rs1 == 31);
    assert(instruction.operands.i.imm == -2048);
    printf("  매핑 I형 | rd=%d rs1=%d imm=%d\n",
           instruction.operands.i.rd, instruction.operands.i.rs1,
           instruction.operands.i.imm);

    char upper_bound[] = "addi x1, x0, 2047";
    assert(parse_line(upper_bound, 2, &line) == LINE_OK);
    assert(parse_instruction(&line, 2, &instruction) == LINE_OK);
    assert(instruction.operands.i.imm == 2047);

    char blank[] = " \t\r\n";
    char comment[] = "  # comment";
    assert(parse_line(blank, 3, &line) == LINE_SKIP);
    assert(parse_line(comment, 4, &line) == LINE_SKIP);

    // 오류를 기대하는 입력이라 아래 호출은 진단 메시지도 출력한다.
    char too_large[] = "addi x1, x0, 2048";
    char missing_operand[] = "addi x1, x0";
    assert(parse_line(too_large, 5, &line) == LINE_OK);
    assert(parse_instruction(&line, 5, &instruction) == LINE_ERROR);
    assert(parse_line(missing_operand, 6, &line) == LINE_OK);
    assert(parse_instruction(&line, 6, &instruction) == LINE_ERROR);

    char memory[] = "lw t0 , 4(sp)";
    assert(parse_line(memory, 7, &line) == LINE_OK && line.operand_count == 2);
    assert(strcmp(line.operands[0], "t0") == 0);
    assert(strcmp(line.operands[1], "4(sp)") == 0);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 7, &instruction) == LINE_OK);
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
    assert(parse_instruction(&line, 12, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_R);
    assert(instruction.operands.r.rd == 5 && instruction.operands.r.rs1 == 6);
    assert(instruction.operands.r.rs2 == 7);
    printf("  매핑 R형 | rd=%d rs1=%d rs2=%d\n",
           instruction.operands.r.rd, instruction.operands.r.rs1,
           instruction.operands.r.rs2);

    char s[] = "sw t0, -4(sp)";
    assert(parse_line(s, 13, &line) == LINE_OK);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 13, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_S && instruction.operands.sb.imm == -4);
    assert(instruction.operands.sb.rs1 == 2 && instruction.operands.sb.rs2 == 5);
    printf("  매핑 S형 | rs2=%d rs1=%d imm=%d\n",
           instruction.operands.sb.rs2, instruction.operands.sb.rs1,
           instruction.operands.sb.imm);
    char s_no_offset[] = "sw t0, (sp)";
    assert(parse_line(s_no_offset, 14, &line) == LINE_OK);
    assert(parse_instruction(&line, 14, &instruction) == LINE_OK);
    assert(instruction.operands.sb.imm == 0);

    char b[] = "beq a0, a1, -4096";
    assert(parse_line(b, 15, &line) == LINE_OK);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 15, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_B && instruction.operands.sb.imm == -4096);
    assert(instruction.operands.sb.rs1 == 10 && instruction.operands.sb.rs2 == 11);
    printf("  매핑 B형 | rs1=%d rs2=%d offset=%d\n",
           instruction.operands.sb.rs1, instruction.operands.sb.rs2,
           instruction.operands.sb.imm);

    char u[] = "lui t0, 0xFF200";
    assert(parse_line(u, 16, &line) == LINE_OK);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 16, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_U && instruction.operands.uj.imm == 0xFF200);
    assert(instruction.operands.uj.rd == 5);
    printf("  매핑 U형 | rd=%d imm=0x%X\n", instruction.operands.uj.rd,
           (unsigned int)instruction.operands.uj.imm);

    char j[] = "jal ra, 1048574";
    assert(parse_line(j, 17, &line) == LINE_OK);
    print_parsed_line(&line);
    assert(parse_instruction(&line, 17, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_J && instruction.operands.uj.imm == 1048574);
    assert(instruction.operands.uj.rd == 1);
    printf("  매핑 J형 | rd=%d offset=%d\n",
           instruction.operands.uj.rd, instruction.operands.uj.imm);

    char odd_jump[] = "jal ra, 3";
    assert(parse_line(odd_jump, 18, &line) == LINE_OK);
    assert(parse_instruction(&line, 18, &instruction) == LINE_ERROR);
    char bad_address[] = "sw t0, 4(sp";
    assert(parse_line(bad_address, 19, &line) == LINE_OK);
    assert(parse_instruction(&line, 19, &instruction) == LINE_ERROR);

    char alu[] = "andi t0, t1, -2";
    assert(parse_line(alu, 20, &line) == LINE_OK);
    assert(parse_instruction(&line, 20, &instruction) == LINE_OK);
    assert(instruction.operands.i.rd == 5 && instruction.operands.i.rs1 == 6);
    assert(instruction.operands.i.imm == -2);

    char jalr[] = "jalr ra, (sp)";
    assert(parse_line(jalr, 21, &line) == LINE_OK);
    assert(parse_instruction(&line, 21, &instruction) == LINE_OK);
    assert(instruction.operands.i.rd == 1 && instruction.operands.i.rs1 == 2);
    assert(instruction.operands.i.imm == 0);

    char shift_max[] = "srai t0, t1, 31";
    char shift_too_large[] = "srai t0, t1, 32";
    char shift_negative[] = "srai t0, t1, -1";
    assert(parse_line(shift_max, 22, &line) == LINE_OK);
    assert(parse_instruction(&line, 22, &instruction) == LINE_OK);
    assert(instruction.operands.i.imm == 31);
    assert(parse_line(shift_too_large, 23, &line) == LINE_OK);
    assert(parse_instruction(&line, 23, &instruction) == LINE_ERROR);
    assert(parse_line(shift_negative, 24, &line) == LINE_OK);
    assert(parse_instruction(&line, 24, &instruction) == LINE_ERROR);

    char system[] = "mret";
    assert(parse_line(system, 25, &line) == LINE_OK);
    assert(parse_instruction(&line, 25, &instruction) == LINE_ERROR);

    // 이스케이프된 따옴표 뒤의 #과 쉼표도 문자열 내부에 남는다.
    char escaped[] = ".asciz \"a\\\"#,b\" # 밖의 주석";
    assert(parse_line(escaped, 26, &line) == LINE_OK);
    assert(strcmp(line.mnemonic, ".asciz") == 0 && line.operand_count == 1);
    assert(strcmp(line.operands[0], "\"a\\\"#,b\"") == 0);

    // S형의 홀수 즉시값과 B형의 가장 큰 짝수 오프셋은 유효하다.
    char odd_store[] = "sb t0, 1(sp)";
    assert(parse_line(odd_store, 27, &line) == LINE_OK);
    assert(parse_instruction(&line, 27, &instruction) == LINE_OK);
    assert(instruction.format == FORMAT_S && instruction.operands.sb.imm == 1);

    char branch_max[] = "beq a0, a1, 4094";
    assert(parse_line(branch_max, 28, &line) == LINE_OK);
    assert(parse_instruction(&line, 28, &instruction) == LINE_OK);
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
        assert(parse_instruction(&line, line_number, &instruction) == LINE_ERROR);
    }
}

static void test_encode_addi(void)
{
    struct IformatOperands positive = {.rd = 5, .rs1 = 0, .imm = 7};
    struct IformatOperands negative = {.rd = 6, .rs1 = 0, .imm = -2};
    struct IformatOperands with_source = {.rd = 6, .rs1 = 6, .imm = 0};
    assert(encode_addi(positive) == 0x00700293u);
    assert(encode_addi(negative) == 0xffe00313u);
    assert(encode_addi(with_source) == 0x00030313u);
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
    test_encode_addi();
    test_binary_output();
    return 0;
}

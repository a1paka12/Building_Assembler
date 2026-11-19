// macOS에서 mkstemp와 fdopen의 POSIX 선언을 사용할 수 있게 한다.
#define _POSIX_C_SOURCE 200809L
#include "src/encode_i.h"
#include "src/output.h"
#include "src/parse.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// 입력을 한 줄씩 분리·매핑·인코딩해 출력에 쓴다. 성공하면 0, 오류면 1을 반환한다.
static int assemble(FILE *input, FILE *output, const char *filename)
{
    char line[256];
    int line_number = 0;

    // fgets는 파일에서 한 줄씩 읽는다. 현재 실습의 줄 버퍼는 256바이트다.
    while (fgets(line, sizeof line, input) != NULL) {
        ++line_number;

        struct ParsedLine parsed_line;
        enum LineResult result = parse_line(line, line_number, &parsed_line);
        if (result == LINE_ERROR)
            return 1;
        if (result == LINE_SKIP)
            continue;

        struct ParsedInstruction instruction;
        if (parse_instruction(&parsed_line, line_number, &instruction) != LINE_OK)
            return 1;

        if (strcmp(parsed_line.mnemonic, "addi") != 0) {
            fprintf(stderr, "%d행: '%s' 인코더는 아직 구현되지 않음\n",
                    line_number, parsed_line.mnemonic);
            return 1;
        }

        uint32_t word = encode_addi(instruction.operands.i);
        if (write_word_le(output, word, filename) != 0)
            return 1;
    }

    // fgets가 NULL을 돌려준 이유가 읽기 오류였는지 확인한다.
    if (ferror(input)) {
        perror("입력 읽기");
        return 1;
    }
    return 0;
}

// 입력 파일을 열고, 어셈블 결과를 임시 파일에 쓴 뒤 성공하면 .bin으로 저장한다.
int main(int argc, char *argv[])
{

    // ./read_asm first.s에서 argv[1]에 "first.s"가 들어온다.
    FILE *input = fopen(argv[1], "r");
    if (input == NULL) {
        perror(argv[1]);
        return 1;
    }
    // 경로를 제외한 파일 이름에서 마지막 확장자를 .bin으로 바꾼다.
    const char *slash = strrchr(argv[1], '/');
    const char *filename = slash ? slash + 1 : argv[1];
    const char *dot = strrchr(filename, '.');
    size_t stem_length = (dot && dot != filename)
                       ? (size_t)(dot - filename)
                       : strlen(filename);
                       
    char output_name[stem_length + sizeof ".bin"];
    memcpy(output_name, filename, stem_length);
    memcpy(output_name + stem_length, ".bin", sizeof ".bin");
    
    if (strcmp(filename, output_name) == 0) {
        fprintf(stderr, "입력과 출력의 파일명이 같습니다: %s\n", filename);
        fclose(input);
        return 1;
    }

    // 먼저 임시 파일에 쓴다. 입력 중간에 오류가 나도 기존 결과를 보존한다.
    char temporary_path[] = ".read_asm.XXXXXX";
    int fd = mkstemp(temporary_path);
    if (fd < 0) {
        perror(temporary_path);
        fclose(input);
        return 1;
    }

    // mkstemp로 연 파일을 fwrite에 전달할 수 있는 바이너리 스트림으로 바꾼다.
    FILE *output = fdopen(fd, "wb");
    if (output == NULL) {
        perror(temporary_path);
        close(fd);
        remove(temporary_path);
        fclose(input);
        return 1;
    }

    int failed = assemble(input, output, output_name);
    fclose(input);
    // 버퍼에 남은 데이터는 fclose 때 쓰일 수 있어서 닫기 결과도 확인한다.
    if (fclose(output) == EOF) {
        perror(output_name);
        failed = 1;
    }
    // 입력 검증과 쓰기가 모두 성공하면 최종 파일명으로 바꾼다.
    if (!failed && rename(temporary_path, output_name) != 0) {
        perror(output_name);
        failed = 1;
    }
    if (failed)
        remove(temporary_path);
    return failed;
}

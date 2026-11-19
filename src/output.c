#include "output.h"

// 기계어 한 워드를 하위 바이트부터 4바이트로 쓴다. 성공하면 0, 쓰기 오류면 1을 반환한다.
int write_word_le(FILE *output, uint32_t word, const char *filename)
{
    // 리틀엔디안은 하위 바이트부터 쓴다. 0x00700293 → 93 02 70 00.
    unsigned char bytes[4] = {
        (unsigned char)(word & 0xFFu),
        (unsigned char)((word >> 8) & 0xFFu),
        (unsigned char)((word >> 16) & 0xFFu),
        (unsigned char)((word >> 24) & 0xFFu)
    };
    if (fwrite(bytes, 1, sizeof bytes, output) != sizeof bytes) {
        perror(filename);
        return 1;
    }
    return 0;
}

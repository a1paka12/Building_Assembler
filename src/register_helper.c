#include "register_helper.h"
#include "register_table.h"

#include <string.h>

// x0~x31과 ABI 별칭을 레지스터 번호로 바꾼다. 유효하지 않은 이름이면 -1을 반환한다.
int reg_number(const char *name)
{
    if (name[0] == 'x' && name[1] >= '0' && name[1] <= '9') {
        int number = name[1] - '0';

        if (name[2] == '\0')
            return number;
        if (number != 0 && name[2] >= '0' && name[2] <= '9' && name[3] == '\0') {
            number = 10 * number + (name[2] - '0');
            if (number <= 31)
                return number;
        }
        return -1;
    }

    if (strcmp(name, "fp") == 0)
        return 8;

    for (size_t i = 0; i < sizeof abi_registers / sizeof abi_registers[0]; ++i) {
        if (strcmp(name, abi_registers[i]) == 0)
            return (int)i;
    }
    return -1;
}

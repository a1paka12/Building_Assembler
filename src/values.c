#include "values.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>

int valid_symbol_name(const char *name)
{
    if (!(isalpha((unsigned char)*name) || *name == '_' || *name == '.'))
        return 0;
    for (++name; *name != '\0'; ++name) {
        if (!(isalnum((unsigned char)*name) || *name == '_' || *name == '.'))
            return 0;
    }
    return 1;
}

// 문자 상수와 문자열 지시어에서 같은 이스케이프 규칙을 사용한다.
int read_character(const char **text, unsigned char *value)
{
    const char *p = *text;
    if (*p == '\0')
        return 1;

    if (*p == '\\') {
        ++p;
        switch (*p) {
        case 'n': *value = '\n'; break;
        case 'r': *value = '\r'; break;
        case 't': *value = '\t'; break;
        case 'b': *value = '\b'; break;
        case '0': *value = '\0'; break;
        case '\\': case '"': case '\'': *value = (unsigned char)*p; break;
        default: return 1;
        }
    } else {
        *value = (unsigned char)*p;
    }

    *text = p + 1;
    return 0;
}

// 라벨, 상수, 또는 숫자 하나를 읽는다.
int parse_value(const char *text, const struct LabelTable *labels, int64_t *value)
{
    const struct Label *label = find_label(labels, text);
    if (label != NULL) {
        *value = label->address;
        return 0;
    }

    int negative = *text == '-';
    const char *digits = text + (*text == '+' || *text == '-');
    if (*digits == '\'') {
        unsigned char character;
        ++digits;
        if (*digits == '\'' || read_character(&digits, &character) != 0 ||
            digits[0] != '\'' || digits[1] != '\0')
            return 1;
        *value = negative ? -(int64_t)character : character;
        return 0;
    }
    if (*digits < '0' || *digits > '9')
        return 1;

    int base = 10;
    if (digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
        base = 16;
    if (digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B')) {
        base = 2;
        digits += 2;
        if (*digits != '0' && *digits != '1')
            return 1;
    }

    char *end;
    errno = 0;
    uintmax_t magnitude = strtoumax(digits, &end, base);
    uintmax_t limit = (uintmax_t)INT64_MAX + (unsigned int)negative;
    if (errno == ERANGE || end == digits || *end != '\0' || magnitude > limit)
        return 1;

    if (negative && magnitude == limit)
        *value = INT64_MIN;
    else
        *value = negative ? -(int64_t)magnitude : (int64_t)magnitude;
    return 0;
}

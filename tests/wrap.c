#include "common/wrap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                            \
    do {                                                            \
        if (!(condition)) {                                         \
            fprintf(stderr, "Line %d: %s\n", __LINE__, #condition); \
            exit(1);                                                \
        }                                                           \
    } while (0)

static void checkSpan(const char* input, uint32_t width, const char* expected, const char* rest) {
    FFwrapSpan span = ffWrapNext(input, (uint32_t) strlen(input), width, true);
    if (span.length != strlen(expected)) {
        fprintf(stderr, "Input '%s', width %u: got '%.*s', expected '%s'\n", input, width, (int) span.length, input, expected);
    }
    CHECK(span.length == strlen(expected));
    CHECK(memcmp(input, expected, span.length) == 0);
    CHECK(strcmp(input + span.consumed, rest) == 0);
}

int main(void) {
    checkSpan("one two three", 7, "one two", "three");
    checkSpan("one  two", 3, "one", "two");
    checkSpan("android15-8-long", 12, "android15-8-", "long");
    checkSpan("abcdefghij", 4, "abcd", "efghij");
    checkSpan("exact", 5, "exact", "");
    checkSpan("\e[31mred\e[0m blue", 3, "\e[31mred\e[0m", "blue");
#if FF_ENABLE_WCWIDTH
    checkSpan("界界A", 4, "界界", "A");
    checkSpan("界A", 1, "界", "A");
    checkSpan("e\xcc\x81x", 1, "e\xcc\x81", "x");
    checkSpan("a-\xcc\x81xyz", 3, "a-\xcc\x81", "xyz");
#endif
    checkSpan("\e]8;;https://example.org\e\\link\e]8;;\e\\ rest", 4, "\e]8;;https://example.org\e\\link\e]8;;\e\\", "rest");
    const char* colored = "\e[38;2;10;20;30mred\e[m";
    CHECK(ffWrapTextWidth(colored, (uint32_t) strlen(colored)) == 3);
    const char* colonColor = "\e[1;38:2::1:2:3mX";
    CHECK(ffWrapIsSafe(colonColor, (uint32_t) strlen(colonColor)));
    CHECK(!ffWrapIsSafe("\e[20GX", 6));
    CHECK(!ffWrapIsSafe("\e[31", 4));
    CHECK(!ffWrapIsSafe("a\rb", 3));
    FFwrapSpan blocks = ffWrapNext("      ", 6, 4, false);
    CHECK(blocks.length == 4 && blocks.consumed == 4);
    puts("Wrapping tests passed");
    return 0;
}

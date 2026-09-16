#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct FFwrapSpan {
    uint32_t length;   // bytes to print
    uint32_t consumed; // includes whitespace discarded at a word boundary
    uint32_t width;    // terminal cells
} FFwrapSpan;

// SGR and OSC 8 are the only control sequences that can be safely reflowed.
uint32_t ffWrapEscapeLength(const char* text, uint32_t length);
bool ffWrapIsSafe(const char* text, uint32_t length);
uint32_t ffWrapTextWidth(const char* text, uint32_t length);
FFwrapSpan ffWrapNext(const char* text, uint32_t length, uint32_t width, bool wordWrap);

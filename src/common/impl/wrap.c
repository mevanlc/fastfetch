#include "common/wrap.h"
#include "common/strutil.h"

uint32_t ffWrapEscapeLength(const char* text, uint32_t length) {
    if (length < 3 || text[0] != '\e') {
        return 0;
    }
    if (text[1] == '[') {
        for (uint32_t i = 2; i < length; ++i) {
            if (text[i] == 'm') {
                return i + 1;
            }
            if (!(ffCharIsDigit(text[i]) || text[i] == ';' || text[i] == ':')) {
                return 0;
            }
        }
    } else if (length >= 5 && text[1] == ']' && text[2] == '8' && text[3] == ';') {
        for (uint32_t i = 4; i < length; ++i) {
            if (text[i] == '\a') {
                return i + 1;
            }
            if (text[i] == '\e' && i + 1 < length && text[i + 1] == '\\') {
                return i + 2;
            }
        }
    }
    return 0;
}

bool ffWrapIsSafe(const char* text, uint32_t length) {
    for (uint32_t pos = 0; pos < length;) {
        if ((unsigned char) text[pos] < 32 || text[pos] == 127) {
            uint32_t escape = ffWrapEscapeLength(text + pos, length - pos);
            if (!escape) {
                return false;
            }
            pos += escape;
        } else {
            ++pos;
        }
    }
    return true;
}

uint32_t ffWrapTextWidth(const char* text, uint32_t length) {
    uint32_t width = 0;
    for (uint32_t pos = 0; pos < length;) {
        uint32_t escape = ffWrapEscapeLength(text + pos, length - pos);
        if (escape) {
            pos += escape;
        } else {
            uint8_t cells;
            uint8_t bytes = ffUtf8CharLenWidth(text + pos, length - pos, &cells);
            pos += bytes ? bytes : 1;
            width += cells;
        }
    }
    return width;
}

FFwrapSpan ffWrapNext(const char* text, uint32_t length, uint32_t width, bool wordWrap) {
    FFwrapSpan fit = {}, boundary = {};
    bool inSpaces = false;
    while (fit.length < length) {
        uint32_t pos = fit.length;
        uint32_t escape = ffWrapEscapeLength(text + pos, length - pos);
        if (escape) {
            fit.length += escape;
            continue;
        }
        uint8_t cells;
        uint8_t bytes = ffUtf8CharLenWidth(text + pos, length - pos, &cells);
        if (!bytes) {
            bytes = 1;
        }
        if (wordWrap && text[pos] == ' ' && fit.width > 0 && !inSpaces) {
            boundary = (FFwrapSpan) { .length = pos, .consumed = pos, .width = fit.width };
        }
        if (cells > 0 && fit.width > 0 && (fit.width >= width || cells > width - fit.width)) {
            if (wordWrap && boundary.length > 0) {
                while (boundary.consumed < length && text[boundary.consumed] == ' ') {
                    ++boundary.consumed;
                }
                return boundary;
            }
            break;
        }
        // A character wider than the entire column is emitted intact to make progress.
        fit.length += bytes;
        fit.width += cells;
        if (cells == 0 && boundary.length == pos && boundary.consumed == pos) {
            // Keep a combining mark attached even when its base is a breakable hyphen.
            boundary.length = boundary.consumed = fit.length;
        }
        inSpaces = text[pos] == ' ';
        if (wordWrap && text[pos] == '-') {
            boundary = fit;
            boundary.consumed = boundary.length;
        }
    }
    fit.consumed = fit.length;
    return fit;
}

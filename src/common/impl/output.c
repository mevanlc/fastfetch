#include "common/printing.h"
#include "common/color.h"
#include "common/wrap.h"
#include "common/textModifier.h"
#include "logo/logo.h"

#include <unistd.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/ioctl.h>
    #ifdef __sun
        #include <sys/termios.h>
    #endif
#endif

static struct {
    FFstrbuf line;
    FFstrbuf style;
    FFstrbuf hyperlink;
    bool initialized;
    bool active;
    bool emitting;
    bool clip;
    uint32_t blockWidth;
    uint32_t blockStart;
} output;

static void initOutput(void) {
    if (!output.initialized) {
        ffStrbufInit(&output.line);
        ffStrbufInit(&output.style);
        ffStrbufInit(&output.hyperlink);
        output.initialized = true;
    }
}

static uint32_t informationWidth(void) {
    uint32_t reserved = instance.state.logoReservedWidth;
    return instance.state.wrapWidth > reserved ? instance.state.wrapWidth - reserved : 1;
}

static void rememberStyle(const char* text, uint32_t length) {
    for (uint32_t pos = 0; pos < length;) {
        uint32_t escape = ffWrapEscapeLength(text + pos, length - pos);
        if (!escape) {
            ++pos;
            continue;
        }
        const char* seq = text + pos;
        if (seq[1] == '[') {
            if (seq[2] == 'm' || (seq[2] == '0' && (seq[3] == ';' || seq[3] == 'm'))) {
                ffStrbufClear(&output.style);
            }
            ffStrbufAppendNS(&output.style, escape, seq);
        } else {
            const char* uri = memchr(seq + 4, ';', escape - 4);
            if (uri && (uri[1] == '\a' || uri[1] == '\e')) {
                ffStrbufClear(&output.hyperlink);
            } else {
                ffStrbufSetNS(&output.hyperlink, escape, seq);
            }
        }
        pos += escape;
    }
}

static void resetStyle(void) {
    if (output.hyperlink.length) {
        fputs("\e]8;;\e\\", stdout);
    }
    if (!instance.config.display.pipe || output.style.length) {
        fputs(FASTFETCH_TEXT_MODIFIER_RESET, stdout);
    }
}

static void emitLine(void) {
    output.emitting = true;
    uint32_t width = informationWidth();
    bool safe = ffWrapIsSafe(output.line.chars, output.line.length);
    uint32_t pos = 0;
    bool continuation = false;
    do {
        uint32_t indent = continuation && !output.blockWidth ? (width > 5 ? 4 : width - 1) : 0;
        uint32_t available = width - indent;
        if (output.blockWidth && output.blockWidth <= available) {
            uint32_t prefix = pos == 0 ? output.blockStart : 0;
            if (prefix < available) {
                available = prefix + (available - prefix) / output.blockWidth * output.blockWidth;
            }
        }
        FFwrapSpan span = safe
            ? ffWrapNext(output.line.chars + pos, output.line.length - pos, available, !output.clip && !output.blockWidth)
            : (FFwrapSpan) { .length = output.line.length - pos, .consumed = output.line.length - pos };

        resetStyle();
        ffLogoPrintLine();
        ffPrintCharTimes(' ', indent);
        ffStrbufWriteTo(&output.style, stdout);
        ffStrbufWriteTo(&output.hyperlink, stdout);
        fwrite(output.line.chars + pos, 1, span.length, stdout);
        rememberStyle(output.line.chars + pos, span.consumed);
        resetStyle();
        ffLogoPrintLineEnd(indent + (safe ? span.width : ffWrapTextWidth(output.line.chars + pos, span.length)));
        putchar('\n');
        pos += span.consumed;
        continuation = true;
        if (output.clip || !span.consumed) {
            break;
        }
    } while (pos < output.line.length);
    ffStrbufClear(&output.line);
    output.emitting = false;
}

void ffPrintInitFrame(void) {
    ffPrintEndModule();
    instance.state.wrapWidth = 0;
    const FFOptionsDisplay* options = &instance.config.display;
    if (options->wrap < 0 || (!options->wrapExplicit && options->disableLinewrap)) {
        return;
    }
    if (options->wrap > 0) {
        instance.state.wrapWidth = (uint32_t) options->wrap;
        return;
    }
    if (!isatty(STDOUT_FILENO)) {
        return;
    }
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        instance.state.wrapWidth = (uint32_t) (info.srWindow.Right - info.srWindow.Left + 1);
    }
#else
    struct winsize size = {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
        instance.state.wrapWidth = size.ws_col;
    }
#endif
}

void ffPrintBeginLine(void) {
    if (!instance.state.wrapWidth) {
        ffLogoPrintLine();
        return;
    }
    initOutput();
    ffPrintEndModule();
    output.active = true;
    output.clip = false;
    output.blockWidth = 0;
    output.blockStart = 0;
}

void ffPrintEndModule(void) {
    if (!output.initialized) {
        return;
    }
    if (output.active && output.line.length) {
        emitLine();
    }
    output.active = false;
    ffStrbufClear(&output.style);
    ffStrbufClear(&output.hyperlink);
}

void ffPrintDestroy(void) {
    ffPrintEndModule();
    if (output.initialized) {
        ffStrbufDestroy(&output.line);
        ffStrbufDestroy(&output.style);
        ffStrbufDestroy(&output.hyperlink);
        output.initialized = false;
    }
}

void ffPrintWrite(const char* text, uint32_t length) {
    if (!output.active || output.emitting) {
        fwrite(text, 1, length, stdout);
        return;
    }
    for (uint32_t pos = 0; pos < length;) {
        uint32_t start = pos;
        while (pos < length && text[pos] != '\n' && text[pos] != '\t') {
            ++pos;
        }
        ffStrbufAppendNS(&output.line, pos - start, text + start);
        if (pos < length) {
            if (text[pos++] == '\n') {
                emitLine();
            } else {
                uint32_t column = instance.state.logoWidth + ffWrapTextWidth(output.line.chars, output.line.length);
                ffStrbufAppendNC(&output.line, 8 - column % 8, ' ');
            }
        }
    }
}

void ffPrintS(const char* text) {
    ffPrintWrite(text, (uint32_t) strlen(text));
}

void ffPrintC(char c) {
    ffPrintWrite(&c, 1);
}

void ffPrintLine(const char* text) {
    ffPrintS(text);
    ffPrintC('\n');
}

void ffPrintBuffer(const FFstrbuf* buffer) {
    ffPrintWrite(buffer->chars, buffer->length);
}

void ffPrintBufferLine(const FFstrbuf* buffer) {
    ffPrintBuffer(buffer);
    ffPrintC('\n');
}

void ffPrintVF(const char* format, va_list args) {
    if (!output.active || output.emitting) {
        vprintf(format, args);
        return;
    }
    FF_STRBUF_AUTO_DESTROY buffer = ffStrbufCreate();
    ffStrbufAppendVF(&buffer, format, args);
    ffPrintBuffer(&buffer);
}

void ffPrintF(const char* format, ...) {
    va_list args;
    va_start(args, format);
    ffPrintVF(format, args);
    va_end(args);
}

void ffPrintKeyWidth(uint32_t width) {
    if (!output.active) {
        printf("\e[%uG", (unsigned) (width + instance.state.logoWidth));
        return;
    }
    uint32_t current = ffWrapTextWidth(output.line.chars, output.line.length);
    if (width > current + 1) {
        ffStrbufAppendNC(&output.line, width - current - 1, ' ');
    }
}

void ffPrintSetClipping(void) {
    output.clip = true;
}

void ffPrintSetBlockWidth(uint32_t width) {
    output.blockWidth = width;
    output.blockStart = output.active ? ffWrapTextWidth(output.line.chars, output.line.length) : 0;
}

void ffPrintStat(double ms, int32_t threshold) {
    char str[64];
    int len = snprintf(str, sizeof(str), "%.3fms", ms);
    if (threshold > 0) {
        snprintf(str, sizeof(str), "\e[%sm%.3fms\e[m", ms <= threshold ? FF_COLOR_FG_GREEN : ms <= 2 * threshold ? FF_COLOR_FG_YELLOW
                                                                                                                 : FF_COLOR_FG_RED,
            ms);
    }
    if (!instance.state.wrapWidth) {
        printf("\e7\e[1A\e[9999999C\e[%dD%s\e8", len - 1, str);
        return;
    }
    // Timing gets its own row so it cannot overwrite a wrapped value or a right logo.
    ffPrintBeginLine();
    uint32_t width = informationWidth();
    if (width > (uint32_t) len) {
        ffPrintCharTimes(' ', width - (uint32_t) len);
    }
    ffPrintLine(str);
    ffPrintEndModule();
}

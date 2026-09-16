#pragma once

#include "fastfetch.h"
#include "common/format.h"

typedef enum FFPrintType: uint8_t {
    FF_PRINT_TYPE_DEFAULT = 0,
    FF_PRINT_TYPE_NO_CUSTOM_KEY = 1 << 0, // key has been formatted outside
    FF_PRINT_TYPE_NO_CUSTOM_KEY_COLOR = 1 << 1,
    FF_PRINT_TYPE_NO_CUSTOM_KEY_WIDTH = 1 << 2,
    FF_PRINT_TYPE_NO_CUSTOM_OUTPUT_FORMAT = 1 << 3, // reserved
} FFPrintType;

void ffPrintLogoAndKey(const char* moduleName, uint8_t moduleIndex, const FFModuleArgs* moduleArgs, FFPrintType printType);
bool ffPrintFormat(const char* moduleName, uint8_t moduleIndex, const FFModuleArgs* moduleArgs, FFPrintType printType, uint32_t numArgs, const FFformatarg* arguments);
#define FF_PRINT_FORMAT_CHECKED(moduleName, moduleIndex, moduleArgs, printType, arguments) \
    ffPrintFormat((moduleName), (moduleIndex), (moduleArgs), (printType), (sizeof(arguments) / sizeof(*arguments)), (arguments));
[[gnu::format(printf, 5, 6)]] void ffPrintError(const char* moduleName, uint8_t moduleIndex, const FFModuleArgs* moduleArgs, FFPrintType printType, const char* message, ...);
void ffPrintColor(const FFstrbuf* colorValue);
void ffPrintCharTimes(char c, uint32_t times);

// Module output passes through these helpers; logo/protocol output stays on stdout.
void ffPrintBeginLine(void);
void ffPrintEndModule(void);
void ffPrintInitFrame(void);
void ffPrintDestroy(void);
void ffPrintWrite(const char* text, uint32_t length);
void ffPrintS(const char* text);
void ffPrintC(char c);
void ffPrintLine(const char* text);
void ffPrintBuffer(const FFstrbuf* buffer);
void ffPrintBufferLine(const FFstrbuf* buffer);
void ffPrintVF(const char* format, va_list args);
[[gnu::format(printf, 1, 2)]] void ffPrintF(const char* format, ...);
void ffPrintKeyWidth(uint32_t width);
void ffPrintSetClipping(void);
void ffPrintSetBlockWidth(uint32_t width);
void ffPrintStat(double ms, int32_t threshold);

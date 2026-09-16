#include "fastfetch.h"
#include "common/printing.h"
#include "common/textModifier.h"
#include "logo/logo.h"

void ffPrintLogoAndKey(const char* moduleName, uint8_t moduleIndex, const FFModuleArgs* moduleArgs, FFPrintType printType) {
    ffPrintBeginLine();

    // This is used by --set-keyless, in this case we want neither the module name nor the separator
    if (moduleName == nullptr) {
        return;
    }

    // This is used as a magic value for hiding keys
    if (!(moduleArgs && ffStrbufEqualS(&moduleArgs->key, " ")) && instance.config.display.keyType != FF_MODULE_KEY_TYPE_NONE) {
        ffPrintCharTimes(' ', instance.config.display.keyPaddingLeft);

        if (!instance.config.display.pipe) {
            ffPrintS(FASTFETCH_TEXT_MODIFIER_RESET);
            if (instance.config.display.brightColor) {
                ffPrintS(FASTFETCH_TEXT_MODIFIER_BOLT);
            }

            if (moduleArgs && !(printType & FF_PRINT_TYPE_NO_CUSTOM_KEY_COLOR) && moduleArgs->keyColor.length > 0) {
                ffPrintColor(&moduleArgs->keyColor);
            } else {
                ffPrintColor(&instance.config.display.colorKeys);
            }
        }

        if (instance.config.display.keyType & FF_MODULE_KEY_TYPE_ICON && moduleArgs && moduleArgs->keyIcon.length > 0) {
            ffPrintBuffer(&moduleArgs->keyIcon);
        }

        if (instance.config.display.keyType & FF_MODULE_KEY_TYPE_STRING) {
            ffPrintCharTimes(' ', instance.config.display.keyType >> FF_MODULE_KEY_TYPE_SPACE_SHIFT);

            // nullptr check is required for modules with custom keys, e.g. disk with the folder path
            if ((printType & FF_PRINT_TYPE_NO_CUSTOM_KEY) || !moduleArgs || moduleArgs->key.length == 0) {
                ffPrintS(moduleName);

                if (moduleIndex > 0) {
                    ffPrintF(" %hhu", moduleIndex);
                }
            } else {
                FF_STRBUF_AUTO_DESTROY key = ffStrbufCreate();
                FF_PARSE_FORMAT_STRING_CHECKED(&key, &moduleArgs->key, ((FFformatarg[]) {
                                                                           FF_ARG(moduleIndex, "index"),
                                                                           FF_ARG(moduleArgs->keyIcon, "icon"),
                                                                           FF_ARG(moduleName, "module-name"),
                                                                       }));
                ffPrintBuffer(&key);
            }
        }

        if (!instance.config.display.pipe) {
            ffPrintS(FASTFETCH_TEXT_MODIFIER_RESET);
            ffPrintColor(&instance.config.display.colorSeparator);
        }

        ffPrintBuffer(&instance.config.display.keyValueSeparator);

        if (!instance.config.display.pipe && instance.config.display.colorSeparator.length) {
            ffPrintS(FASTFETCH_TEXT_MODIFIER_RESET);
        }

        if (!(printType & FF_PRINT_TYPE_NO_CUSTOM_KEY_WIDTH)) {
            uint32_t keyWidth = moduleArgs && moduleArgs->keyWidth > 0 ? moduleArgs->keyWidth : instance.config.display.keyWidth;
            if (keyWidth > 0) {
                ffPrintKeyWidth(keyWidth);
            }
        }
    }

    if (!instance.config.display.pipe) {
        ffPrintS(FASTFETCH_TEXT_MODIFIER_RESET);
        if (moduleArgs && moduleArgs->outputColor.length) {
            ffPrintColor(&moduleArgs->outputColor);
        } else if (instance.config.display.colorOutput.length) {
            ffPrintColor(&instance.config.display.colorOutput);
        }
    }
}

bool ffPrintFormat(const char* moduleName, uint8_t moduleIndex, const FFModuleArgs* moduleArgs, FFPrintType printType, uint32_t numArgs, const FFformatarg* arguments) {
    FF_STRBUF_AUTO_DESTROY buffer = ffStrbufCreate();
    bool success;
    if (__builtin_expect(moduleArgs != nullptr, 1)) {
        success = ffParseFormatString(&buffer, &moduleArgs->outputFormat, numArgs, arguments);
    } else {
        ffStrbufSetStatic(&buffer, "undefined format");
        success = false;
    }

    if (success) {
        ffPrintLogoAndKey(moduleName, moduleIndex, moduleArgs, printType);
        ffPrintBufferLine(&buffer);
    } else {
        ffPrintError(moduleName, moduleIndex, moduleArgs, printType, "%s", buffer.chars);
    }

    return success;
}

void ffPrintError(const char* moduleName, uint8_t moduleIndex, const FFModuleArgs* moduleArgs, FFPrintType printType, const char* message, ...) {
    if (!instance.config.display.showErrors) {
        return;
    }

    ffPrintLogoAndKey(moduleName, moduleIndex, moduleArgs, printType);

    if (!instance.config.display.pipe) {
        ffPrintS(FASTFETCH_TEXT_MODIFIER_ERROR);
    }

    va_list arguments;
    va_start(arguments, message);
    ffPrintVF(message, arguments);
    va_end(arguments);

    if (!instance.config.display.pipe) {
        ffPrintS(FASTFETCH_TEXT_MODIFIER_RESET);
    }

    ffPrintC('\n');
}

void ffPrintColor(const FFstrbuf* colorValue) {
    // If the color is not set, this would reset in \033[m, which resets everything.
    // So we only print it, if the main color is at least one char.
    if (colorValue->length == 0) {
        return;
    }

    ffPrintF("\e[%sm", colorValue->chars);
}

void ffPrintCharTimes(char c, uint32_t times) {
    if (times == 0) {
        return;
    }

    if (times == 1) {
        ffPrintC(c);
        return;
    }

    char str[32];
    memset(str, c, sizeof(str)); // 2 instructions when compiling with AVX2 enabled
    for (uint32_t i = sizeof(str); i <= times; i += (uint32_t) sizeof(str)) {
        ffPrintWrite(str, sizeof(str));
    }
    uint32_t remaining = times % sizeof(str);
    if (remaining > 0) {
        ffPrintWrite(str, remaining);
    }
}

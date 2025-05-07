#ifndef STRINGS_H
#define STRINGS_H

#include <opencl-c-base.h>
#include "Utils.h"

// Assume findLibrary and libBase are defined in Utils.h
extern uintptr_t libBase;

uintptr_t getAbsoluteAddress(uintptr_t relativeAddr, const char *libraryName = "libil2cpp.so") {
    if (libBase == 0)
        libBase = findLibrary(libraryName);
    if (libBase != 0)
        return (libBase + relativeAddr);
    else
        return 0;
}

typedef struct _monoString {
    void *klass;
    void *monitor;
    int length;
    char chars[1];

    int getLength() {
        return length;
    }

    char *getChars() {
        return chars;
    }
} monoString;

monoString *CreateMonoString(const char *str) {
#ifdef __aarch64__
    monoString *(*String_CreateString)(void *instance, const char *str) = (monoString *(*)(void *, const char *))getAbsoluteAddress(string2Offset("0xE62818"), "libil2cpp.so");
#else
    monoString *(*String_CreateString)(void *instance, const char *str) = (monoString *(*)(void *, const char *))getAbsoluteAddress(string2Offset("0xC8A518"), "libil2cpp.so");
#endif
    return String_CreateString(NULL, str);
}

#endif // STRINGS_H
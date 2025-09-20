#include "utils.hpp"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>

#include "../Obfuscation/Obfuscate.h"
#include "Logging.h"

uintptr_t libBase = 0;
bool libLoaded = false;

uintptr_t findLibrary(const char *libraryName) {
    char filename[0xFF] = {0}, buffer[1024] = {0};
    FILE *fp = nullptr;
    uintptr_t address = 0;

    sprintf(filename, ("/proc/self/maps"));

    fp = fopen(filename, OBFUSCATE("rt"));
    if (fp == nullptr) {
        LOGE(OBFUSCATE("Failed to open /proc/self/maps: %s"), strerror(errno));
        return 0;
    }

    size_t page_size = sysconf(_SC_PAGESIZE);
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, libraryName) && (strstr(buffer, "r-xp") || strstr(buffer, "r--p"))) {
            char *addr_str = strtok(buffer, "-");
            if (addr_str) {
                address = strtoull(addr_str, nullptr, 16);
                if (address != 0 && msync((void*)address, page_size, MS_ASYNC) == 0) {
                    LOGI(OBFUSCATE("Found %s at 0x%lx (%s)"), libraryName, address, strstr(buffer, "r-xp") ? "executable" : "readable");
                    break;
                }
            }
        }
    }

    if (address == 0) {
        LOGE(OBFUSCATE("Could not find valid segment for %s"), libraryName);
    }

    fclose(fp);
    return address;
}

uintptr_t getAbsoluteAddress(const char *libraryName, uintptr_t relativeAddr) {
    if (libBase == 0) {
        libBase = findLibrary(libraryName);
        if (libBase == 0) {
            LOGE(OBFUSCATE("Cannot get absolute address: %s not found"), libraryName);
            return 0;
        }
    }
    uintptr_t absolute = libBase + relativeAddr;
    LOGI(OBFUSCATE("Calculated absolute address: 0x%lx + 0x%lx = 0x%lx"), libBase, relativeAddr, absolute);
    return absolute;
}

DWORD getRealOffset(DWORD offset) {
    if (libBase == 0)
        libBase = findLibrary(OBFUSCATE("libil2cpp.so"));
    if (libBase != 0)
        return (reinterpret_cast<DWORD>(libBase + offset));
    else
        return 0;
}

bool isLibraryLoaded(const char *libraryName) {
    if (libLoaded && libBase != 0) {
        return true;
    }

    libBase = findLibrary(libraryName);
    if (libBase != 0) {
        libLoaded = true;
        LOGI(OBFUSCATE("%s is loaded at 0x%lx"), libraryName, libBase);
        return true;
    }
    LOGW(OBFUSCATE("%s not loaded yet"), libraryName);
    return false;
}

uintptr_t string2Offset(const char *c) {
    int base = 16;
    static_assert(sizeof(uintptr_t) == sizeof(unsigned long) || sizeof(uintptr_t) == sizeof(unsigned long long),
                  "Unsupported architecture for string2Offset");
    if (sizeof(uintptr_t) == sizeof(unsigned long)) {
        return strtoul(c, nullptr, base);
    }
    return strtoull(c, nullptr, base);
}

void hook(void *orig_fcn, void* new_fcn, void **orig_fcn_ptr) {
    DobbyHook(orig_fcn, new_fcn, orig_fcn_ptr);
}

std::string readTextFile(std::string path) {
    FILE *textfile;
    char *text;
    long numbytes;
    textfile = fopen(path.c_str(), "r");
    if (textfile == NULL)
        return "ERROR";

    fseek(textfile, 0L, SEEK_END);
    numbytes = ftell(textfile);
    fseek(textfile, 0L, SEEK_SET);

    text = (char*)calloc(numbytes + 1, sizeof(char)); // +1 for null-terminator
    if (text == NULL)
        return "ERROR";

    fread(text, sizeof(char), numbytes, textfile);
    fclose(textfile);
    std::string result(text);
    free(text);
    return result;
}
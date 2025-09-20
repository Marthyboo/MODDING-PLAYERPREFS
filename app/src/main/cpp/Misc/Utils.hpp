#ifndef UTILS_H
#define UTILS_H


#include <sys/mman.h>
#include "../Hooking/dobby.h"
#include <string>
#include <cstdint>
#include <unistd.h> // For sysconf

typedef unsigned long DWORD;

extern uintptr_t libBase;
extern bool libLoaded;

uintptr_t findLibrary(const char *libraryName);
uintptr_t getAbsoluteAddress(const char *libraryName, uintptr_t relativeAddr);
bool isLibraryLoaded(const char *libraryName);
DWORD getRealOffset(DWORD offset);
void hook(void *orig_fcn, void* new_fcn, void **orig_fcn_ptr);
uintptr_t string2Offset(const char *c);
std::string readTextFile(std::string path);

#endif
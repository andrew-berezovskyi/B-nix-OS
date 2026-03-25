#ifndef KSTRING_H
#define KSTRING_H

#include <stddef.h>

void* memset(void* dest, int ch, size_t count);
void* memcpy(void* dest, const void* src, size_t count);
size_t strlen(const char* str);
void* __memset_chk(void* dest, int ch, size_t count, size_t destlen);
void* __memcpy_chk(void* dest, const void* src, size_t len, size_t destlen);

#endif

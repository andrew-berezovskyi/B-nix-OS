#ifndef LIBC_H
#define LIBC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Системні виклики (Syscalls)
void sys_print(const char* str);
void sys_readline(char* buf);
void sys_exit(void);

// Допоміжні функції (Рядки та Математика)
void itoa(int num, char* str);
int atoi(const char* str);
int strcmp(const char* s1, const char* s2);

// Функція зупинки програми, поки користувач не введе щось не порожнє
void get_input(const char* prompt, char* buf);

#endif
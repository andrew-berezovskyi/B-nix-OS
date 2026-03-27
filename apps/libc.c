#include "libc.h"

// ==========================================
// СИСТЕМНІ ВИКЛИКИ
// ==========================================
void sys_print(const char* str) {
    asm volatile("mov $6, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(str) : "eax", "ebx");
}

void sys_readline(char* buf) {
    asm volatile("mov $8, %%eax \n mov %0, %%ebx \n int $0x80" : : "r"(buf) : "eax", "ebx", "memory");
}

void sys_exit(void) {
    asm volatile("mov $7, %%eax \n int $0x80" : : : "eax");
}

// ==========================================
// ДОПОМІЖНІ ФУНКЦІЇ (УТІЛІТИ)
// ==========================================
void itoa(int num, char* str) {
    int i = 0;
    if (num == 0) { str[i++] = '0'; str[i] = '\0'; return; }
    if (num < 0) { str[i++] = '-'; num = -num; }
    int temp = num; int length = 0;
    while (temp != 0) { length++; temp /= 10; }
    for (int j = length - 1; j >= 0; j--) {
        str[i + j] = (num % 10) + '0';
        num /= 10;
    }
    str[i + length] = '\0';
}

int atoi(const char* str) {
    int res = 0; int sign = 1; int i = 0;
    if (str[0] == '-') { sign = -1; i++; }
    for (; str[i] != '\0'; ++i) {
        if (str[i] >= '0' && str[i] <= '9') res = res * 10 + str[i] - '0';
        else break;
    }
    return sign * res;
}

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void get_input(const char* prompt, char* buf) {
    buf[0] = '\0';
    while (buf[0] == '\0') {
        sys_print(prompt);
        sys_readline(buf);
    }
}
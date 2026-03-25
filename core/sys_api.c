#include "sys_api.h"
#include "syscall.h" // Звідси беремо номери команд (SYS_OPEN, SYS_READ...)

// Універсальна макрос-функція для виклику int 0x80 з 3 параметрами
static inline int syscall3(int num, int p1, int p2, int p3) {
    int ret;
    asm volatile (
        "int $0x80"
        : "=a" (ret)                    // Результат повертається в EAX
        : "a" (num), "b" (p1), "c" (p2), "d" (p3) // EAX=num, EBX=p1, ECX=p2, EDX=p3
        : "memory"
    );
    return ret;
}

// Виклик int 0x80 з 1 параметром
static inline int syscall1(int num, int p1) {
    int ret;
    asm volatile (
        "int $0x80" 
        : "=a" (ret) 
        : "a" (num), "b" (p1) 
        : "memory"
    );
    return ret;
}

// ==============================================================
// ПУБЛІЧНЕ API ДЛЯ ПРОГРАМ (USER SPACE)
// ==============================================================

int sys_open(const char* path, int flags) {
    return syscall3(SYS_OPEN, (int)path, flags, 0);
}

int sys_read(int fd, void* buf, int size) {
    return syscall3(SYS_READ, fd, (int)buf, size);
}

int sys_write(int fd, const void* buf, int size) {
    return syscall3(SYS_WRITE, fd, (int)buf, size);
}

void sys_close(int fd) {
    syscall1(SYS_CLOSE, fd);
}

int sys_unlink(const char* path) { return syscall1(SYS_UNLINK, (int)path); }
#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

// Номери системних викликів (API нашої ОС)
#define SYS_OPEN  1
#define SYS_READ  2
#define SYS_WRITE 3
#define SYS_CLOSE 4
#define SYS_UNLINK 5

// 🔥 НОВІ ВИКЛИКИ ДЛЯ ПРОГРАМ
#define SYS_PRINT 6  // Вивід тексту в Термінал
#define SYS_EXIT  7  // Завершення роботи програми
#define SYS_READLINE 8

#endif
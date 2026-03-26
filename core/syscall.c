#include "syscall.h"
#include "fs.h"
#include "timer.h"
#include <stdint.h>

extern void print(const char* str); 
extern void shell_readline(char* out_buf); // Зв'язок з Терміналом

uint32_t syscall_handler_main(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    switch (eax) {
        case SYS_OPEN:   return fs_open((const char*)ebx, (int)ecx);
        case SYS_READ:   return fs_read((int)ebx, (void*)ecx, (int)edx);
        case SYS_WRITE:  return fs_write((int)ebx, (const void*)ecx, (int)edx);
        case SYS_CLOSE:  fs_close((int)ebx); return 0;
        case SYS_UNLINK: return fs_unlink((const char*)ebx);
        case SYS_PRINT:  print((const char*)ebx); return 0;
        case SYS_EXIT:   exit_current_task(); return 0;
            
        // 🔥 НОВЕ: Передаємо керування Терміналу
        case SYS_READLINE:
            shell_readline((char*)ebx);
            return 0;
            
        default: return -1;
    }
}
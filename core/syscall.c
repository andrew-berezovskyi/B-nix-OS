#include "syscall.h"
#include "fs.h"
#include <stdint.h>

// Це серце стіни. Воно приймає номер виклику з регістра EAX
uint32_t syscall_handler_main(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    switch (eax) {
        case SYS_OPEN:
            return fs_open((const char*)ebx, (int)ecx);
            
        case SYS_READ:
            return fs_read((int)ebx, (void*)ecx, (int)edx);
            
        case SYS_WRITE:
            return fs_write((int)ebx, (const void*)ecx, (int)edx);
            
        case SYS_CLOSE:
            fs_close((int)ebx);
            return 0;
            
        case SYS_UNLINK: // <--- ОСЬ ВІН, НАШ ГЕРОЙ
            return fs_unlink((const char*)ebx);
            
        default:
            return -1; // Невідомий системний виклик
    }
}
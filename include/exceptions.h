#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdint.h>

typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t vector, error_code;
    uint32_t eip, cs, eflags, user_esp, user_ss;
} __attribute__((packed)) exception_frame_t;

void exceptions_install(void);
void exception_handler_main(exception_frame_t* frame);

#endif

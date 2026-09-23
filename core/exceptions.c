#include "exceptions.h"
#include "idt.h"
#include "serial.h"

extern void* isr_stub_table[];

static const char* const exception_names[32] = {
    "Divide Error", "Debug", "NMI", "Breakpoint",
    "Overflow", "BOUND Range", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment", "Invalid TSS", "Segment Not Present",
    "Stack Fault", "General Protection", "Page Fault", "Reserved",
    "x87 Floating Point", "Alignment Check", "Machine Check", "SIMD Floating Point",
    "Virtualization", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security", "Reserved"
};

void exceptions_install(void) {
    for (uint32_t i = 0; i < 32; ++i) {
        idt_set_gate((uint8_t)i, (uint32_t)isr_stub_table[i], 0x08, 0x8E);
    }
}

void exception_handler_main(exception_frame_t* frame) {
    __asm__ volatile("cli");
    serial_write("\n[BNIX:PANIC] CPU exception ");
    serial_write_hex(frame->vector);
    serial_write(" ");
    if (frame->vector < 32) serial_write(exception_names[frame->vector]);
    serial_write("\n  error=");
    serial_write_hex(frame->error_code);
    serial_write(" eip=");
    serial_write_hex(frame->eip);
    serial_write(" cs=");
    serial_write_hex(frame->cs);
    serial_write(" eflags=");
    serial_write_hex(frame->eflags);
    if (frame->vector == 14) {
        uint32_t fault_addr;
        __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
        serial_write(" cr2=");
        serial_write_hex(fault_addr);
    }
    serial_write("\nSystem halted.\n");
    for (;;) __asm__ volatile("hlt");
}

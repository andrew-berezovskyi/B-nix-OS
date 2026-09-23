#include "shell.h"
#include "fs.h"
#include "sys_api.h"
#include "kheap.h"   
#include "timer.h"   
#include "elf.h"      
#include "vmm.h"

#define MAX_ELF_PROGRAM_HEADERS 64U
#define MAX_ELF_SEGMENT_BYTES (64U * 1024U * 1024U)
#include <stdint.h>

extern void print(const char* str);
extern void terminal_putchar(char c);
extern void terminal_clear(void);
extern uint32_t get_uptime_seconds(void); 
extern void sleep(uint32_t seconds); 
extern uint32_t pmm_get_free_block_count(void);
extern uint32_t pmm_get_max_blocks(void);

char command_buffer[SHELL_BUFFER_SIZE];
int buffer_index = 0;

volatile bool app_waiting_for_input = false;
char* app_input_buffer = 0;
bool suppress_prompt = false; 

int running_app_task_id = -1;

int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}
int strncmp(const char* s1, const char* s2, int n) {
    while (n && *s1 && (*s1 == *s2)) { ++s1; ++s2; --n; }
    if (n == 0) return 0;
    return *(const unsigned char*)s1 - *(const unsigned char*)s2;
}

void print_prompt() { print("B-nix> "); }

// ========================================================
// ELF32 loader with page-offset aware PT_LOAD mapping
// ========================================================
static int load_elf_and_run(const char* filename) {
    int fd = sys_open(filename, O_RDONLY);
    if (fd == -1) {
        print("ELF Error: Cannot open file.\n");
        return -1;
    }

    elf32_ehdr_t ehdr;
    if (sys_read(fd, &ehdr, sizeof(elf32_ehdr_t)) != (int)sizeof(elf32_ehdr_t)) {
        print("ELF Error: File too small.\n");
        sys_close(fd);
        return -1;
    }

    if (ehdr.e_ident[0] != ELFMAG0 || ehdr.e_ident[1] != ELFMAG1 ||
        ehdr.e_ident[2] != ELFMAG2 || ehdr.e_ident[3] != ELFMAG3) {
        print("ELF Error: Not an ELF file.\n");
        sys_close(fd);
        return -1;
    }

    if (ehdr.e_ident[4] != ELFCLASS32 || ehdr.e_ident[5] != ELFDATA2LSB ||
        ehdr.e_type != ET_EXEC || ehdr.e_machine != EM_386 ||
        ehdr.e_version != EV_CURRENT || ehdr.e_ehsize < sizeof(elf32_ehdr_t)) {
        print("ELF Error: Unsupported executable format.\n");
        sys_close(fd);
        return -1;
    }

    if (ehdr.e_phnum == 0 || ehdr.e_phnum > MAX_ELF_PROGRAM_HEADERS ||
        ehdr.e_phentsize < sizeof(elf32_phdr_t) ||
        ehdr.e_phoff < sizeof(elf32_ehdr_t)) {
        print("ELF Error: Invalid program headers.\n");
        sys_close(fd);
        return -1;
    }

    uint32_t* page_directory = vmm_create_address_space();
    if (page_directory == (uint32_t*)0) {
        print("ELF Error: No memory for page directory.\n");
        sys_close(fd);
        return -1;
    }

    elf32_phdr_t* phdrs = (elf32_phdr_t*)kmalloc((uint32_t)ehdr.e_phnum * sizeof(elf32_phdr_t));
    if (phdrs == (elf32_phdr_t*)0) {
        print("ELF Error: No memory for PHDR table.\n");
        sys_close(fd);
        return -1;
    }

    bool loaded_anything = false;
    bool entry_covered = false;
    char scratch[64];

    uint32_t ph_skip = ehdr.e_phoff;
    while (ph_skip > 0) {
        int chunk = (ph_skip > sizeof(scratch)) ? (int)sizeof(scratch) : (int)ph_skip;
        int got = sys_read(fd, scratch, chunk);
        if (got <= 0) {
            print("ELF Error: PHDR seek failed.\n");
            sys_close(fd);
            return -1;
        }
        ph_skip -= (uint32_t)got;
    }

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        if (ehdr.e_phentsize > sizeof(elf32_phdr_t)) {
            if (sys_read(fd, &phdrs[i], sizeof(elf32_phdr_t)) != (int)sizeof(elf32_phdr_t)) {
                print("ELF Error: PHDR read failed.\n");
                sys_close(fd);
                return -1;
            }

            uint32_t trailing = (uint32_t)ehdr.e_phentsize - sizeof(elf32_phdr_t);
            while (trailing > 0) {
                int chunk = (trailing > sizeof(scratch)) ? (int)sizeof(scratch) : (int)trailing;
                int got = sys_read(fd, scratch, chunk);
                if (got <= 0) {
                    print("ELF Error: PHDR read failed.\n");
                    sys_close(fd);
                    return -1;
                }
                trailing -= (uint32_t)got;
            }
        } else {
            if (sys_read(fd, &phdrs[i], sizeof(elf32_phdr_t)) != (int)sizeof(elf32_phdr_t)) {
                print("ELF Error: PHDR read failed.\n");
                sys_close(fd);
                return -1;
            }
        }
    }

    sys_close(fd);

    for (uint16_t i = 0; i < ehdr.e_phnum; i++) {
        elf32_phdr_t phdr = phdrs[i];
        if (phdr.p_type != PT_LOAD || phdr.p_memsz == 0) {
            continue;
        }
        uint32_t offset_in_page = phdr.p_vaddr & 0xFFFU;
        if (phdr.p_filesz > phdr.p_memsz ||
            phdr.p_memsz > 0xFFFFFFFFU - offset_in_page ||
            phdr.p_vaddr > 0xFFFFFFFFU - phdr.p_memsz) {
            print("ELF Error: Corrupted segment sizes.\n");
            return -1;
        }

        uint32_t total_mem = phdr.p_memsz + offset_in_page;
        uint32_t pages_needed = (total_mem + 4095U) / 4096U;
        if (total_mem > MAX_ELF_SEGMENT_BYTES || total_mem > 0xFFFFFFFFU - 4095U ||
            pages_needed == 0 || pages_needed > MAX_ELF_SEGMENT_BYTES / 4096U) {
            print("ELF Error: Segment exceeds loader limits.\n");
            return -1;
        }
        uint32_t alloc_size = pages_needed * 4096U;
        loaded_anything = true;
        if (ehdr.e_entry >= phdr.p_vaddr &&
            ehdr.e_entry < phdr.p_vaddr + phdr.p_memsz) entry_covered = true;

        uint32_t raw = (uint32_t)kmalloc(alloc_size + 4095U);
        if (raw == 0) {
            print("ELF Error: Out of memory.\n");
            return -1;
        }
        uint32_t segment_phys_base = (raw + 4095U) & ~0xFFFU;

        for (uint32_t z = 0; z < alloc_size; z++) {
            ((uint8_t*)segment_phys_base)[z] = 0;
        }

        int seg_fd = sys_open(filename, O_RDONLY);
        if (seg_fd == -1) {
            print("ELF Error: Cannot reopen segment.\n");
            return -1;
        }

        uint32_t seg_skip = phdr.p_offset;
        while (seg_skip > 0) {
            int chunk = (seg_skip > sizeof(scratch)) ? (int)sizeof(scratch) : (int)seg_skip;
            int got = sys_read(seg_fd, scratch, chunk);
            if (got <= 0) {
                sys_close(seg_fd);
                print("ELF Error: Segment seek failed.\n");
                return -1;
            }
            seg_skip -= (uint32_t)got;
        }

        uint8_t* segment_dst = (uint8_t*)(segment_phys_base + offset_in_page);
        if (phdr.p_filesz > 0) {
            uint32_t total_copied = 0;
            while (total_copied < phdr.p_filesz) {
                int got = sys_read(
                    seg_fd,
                    segment_dst + total_copied,
                    (int)(phdr.p_filesz - total_copied)
                );
                if (got <= 0) {
                    sys_close(seg_fd);
                    print("ELF Error: Segment read failed.\n");
                    return -1;
                }
                total_copied += (uint32_t)got;
            }
        }
        sys_close(seg_fd);

        if (phdr.p_memsz > phdr.p_filesz) {
            uint8_t* bss = segment_dst + phdr.p_filesz;
            for (uint32_t j = 0; j < (phdr.p_memsz - phdr.p_filesz); j++) {
                bss[j] = 0;
            }
        }

        uint32_t virt_base = phdr.p_vaddr & ~0xFFFU;
        for (uint32_t page = 0; page < pages_needed; page++) {
            vmm_map_page(
                page_directory,
                virt_base + (page * 4096U),
                segment_phys_base + (page * 4096U),
                PTE_USER | PTE_PRESENT | ((phdr.p_flags & PF_W) ? PTE_RW : 0)
            );
        }
    }

    if (!loaded_anything || !entry_covered) {
        print("ELF Error: Entry point is outside loadable segments.\n");
        return -1;
    }

    int task_id = create_task((void (*)(void))ehdr.e_entry, page_directory);
    if (task_id == -1) {
        print("ELF Error: Task limit reached (Max 4). Please reboot OS!\n");
    }
    return task_id;
}
// ========================================================

void execute_command() {
    command_buffer[buffer_index] = '\0';
    suppress_prompt = false; 

    if (buffer_index == 0) { } 
    else if (strcmp(command_buffer, "help") == 0) {
        print("Available commands:\n  ls, cat, rm, echo, clear, info, uptime, meminfo, sleep, run\n");
    } 
    else if (strcmp(command_buffer, "clear") == 0) { terminal_clear(); } 
    else if (strcmp(command_buffer, "ls") == 0) {
        int dir_fd = fs_opendir("/");
        if (dir_fd != -1) {
            fs_dirent_t entry; int count = 0;
            while (fs_readdir(dir_fd, &entry)) {
                if (entry.type == FS_TYPE_DIR) print("[DIR]  "); else print("[FILE] ");
                print(entry.name); print("\n"); count++;
            }
            fs_closedir(dir_fd);
            if (count == 0) print("Directory is empty.\n");
        }
    }
    else if (strncmp(command_buffer, "cat ", 4) == 0) {
        char* filename = command_buffer + 4; while (*filename == ' ') filename++; 
        if (*filename != '\0') {
            int fd = sys_open(filename, O_RDONLY);
            if (fd != -1) {
                char buf[128]; int bytes_read;
                while ((bytes_read = sys_read(fd, buf, 127)) > 0) { buf[bytes_read] = '\0'; print(buf); }
                print("\n"); sys_close(fd);
            } else print("No such file\n");
        }
    }
    else if (strncmp(command_buffer, "run ", 4) == 0) {
        char* filename = command_buffer + 4; while (*filename == ' ') filename++;
        int len = 0; while (filename[len]) len++;
        while (len > 0 && filename[len-1] == ' ') { filename[len-1] = '\0'; len--; }
        if (*filename != '\0') {
            running_app_task_id = load_elf_and_run(filename);
            if (running_app_task_id != -1) {
                suppress_prompt = true;
            }
        }
    }
    else if (strncmp(command_buffer, "rm ", 3) == 0) {
        char* filename = command_buffer + 3; while (*filename == ' ') filename++;
        int len = 0; while (filename[len]) len++;
        while (len > 0 && filename[len-1] == ' ') { filename[len-1] = '\0'; len--; }
        if (*filename != '\0') { if (sys_unlink(filename) == 0) print("File deleted.\n"); else print("Error\n"); }
    }
    else { print("Unknown command.\n"); }
    buffer_index = 0;
}

void shell_readline(char* out_buf) {
    app_input_buffer = out_buf;
    buffer_index = 0; 
    app_waiting_for_input = true;
    
    asm volatile("cli");
    tasks[current_task].state = TASK_WAITING_KBD;
    asm volatile("sti"); 
    
    while (tasks[current_task].state == TASK_WAITING_KBD) {
        asm volatile("hlt");
    }
}

void shell_handle_keypress(char c) {
    if (app_waiting_for_input) {
        if (c == '\n') {
            print("\n");
            command_buffer[buffer_index] = '\0';
            
            char* dest = app_input_buffer; char* src = command_buffer;
            while(*src) *dest++ = *src++;
            *dest = '\0';
            
            buffer_index = 0; 
            app_waiting_for_input = false; 
            
            if (running_app_task_id != -1) {
                wake_up_task(running_app_task_id);
            }
        } else if (c == '\b') {
            if (buffer_index > 0) { buffer_index--; terminal_putchar('\b'); }
        } else if (buffer_index < SHELL_BUFFER_SIZE - 1) { 
            command_buffer[buffer_index++] = c; terminal_putchar(c); 
        }
        return; 
    }

    if (c == '\n') { 
        print("\n"); execute_command(); 
        if (!suppress_prompt) print_prompt(); 
    } else if (c == '\b') {
        if (buffer_index > 0) { buffer_index--; terminal_putchar('\b'); }
    } else if (buffer_index < SHELL_BUFFER_SIZE - 1) { 
        command_buffer[buffer_index++] = c; terminal_putchar(c); 
    }
}

void init_shell() { buffer_index = 0; print("\n"); print_prompt(); }

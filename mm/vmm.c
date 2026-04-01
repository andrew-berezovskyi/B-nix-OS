#include "vmm.h"
#include "kheap.h"
#include <stdint.h>

#define PAGE_SIZE                4096U
#define PDE_PRESENT              0x01U
#define PDE_RW                   0x02U
#define PDE_USER                 0x04U
#define PDE_PAGE_SIZE_4MB        0x80U
#define CR4_PSE                  0x00000010U

/*
 * Keep low memory identity-mapped with 4MB PDEs so IRQ handlers, VGA text
 * memory, kernel code/data, and heap (starting at 16MB) remain accessible
 * in every address space.
 */
#define KERNEL_IDENTITY_PDE_COUNT 1024U

uint32_t* kernel_directory = (uint32_t*)0;

static void* kmalloc_aligned(uint32_t size) {
    uint32_t raw = (uint32_t)kmalloc(size + (PAGE_SIZE - 1));
    if (raw == 0) {
        return (void*)0;
    }
    raw = (raw + (PAGE_SIZE - 1)) & ~(PAGE_SIZE - 1);
    return (void*)raw;
}

static void zero_page(void* page) {
    uint32_t* p = (uint32_t*)page;
    for (int i = 0; i < 1024; i++) {
        p[i] = 0;
    }
}

void init_vmm(void) {
    kernel_directory = (uint32_t*)kmalloc_aligned(PAGE_SIZE);
    if (kernel_directory == (uint32_t*)0) {
        return;
    }

    zero_page(kernel_directory);

    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= CR4_PSE;
    asm volatile("mov %0, %%cr4" :: "r"(cr4));

    for (uint32_t i = 0; i < KERNEL_IDENTITY_PDE_COUNT; i++) {
        kernel_directory[i] = (i << 22) | PDE_PAGE_SIZE_4MB | PDE_RW | PDE_PRESENT;
    }

    vmm_switch_directory(kernel_directory);

    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

uint32_t* vmm_create_address_space(void) {
    uint32_t* dir = (uint32_t*)kmalloc_aligned(PAGE_SIZE);
    if (dir == (uint32_t*)0) {
        return (uint32_t*)0;
    }

    zero_page(dir);

    for (uint32_t i = 0; i < KERNEL_IDENTITY_PDE_COUNT; i++) {
        dir[i] = kernel_directory[i];
    }

    return dir;
}

void vmm_map_page(uint32_t* dir, uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    if (dir == (uint32_t*)0) {
        return;
    }

    uint32_t pd_index = (virtual_addr >> 22) & 0x3FFU;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FFU;

    if ((dir[pd_index] & PDE_PRESENT) == 0) {
        uint32_t* pt = (uint32_t*)kmalloc_aligned(PAGE_SIZE);
        if (pt == (uint32_t*)0) {
            return;
        }
        zero_page(pt);
        dir[pd_index] = ((uint32_t)pt & 0xFFFFF000U) | PDE_PRESENT | PDE_RW | PDE_USER;
    } else if (dir[pd_index] & PDE_PAGE_SIZE_4MB) {
        uint32_t* pt = (uint32_t*)kmalloc_aligned(PAGE_SIZE);
        if (pt == (uint32_t*)0) {
            return;
        }

        uint32_t pde_base_4mb = dir[pd_index] & 0xFFC00000U;
        for (uint32_t i = 0; i < 1024; i++) {
            uint32_t ident_phys = pde_base_4mb + (i * PAGE_SIZE);
            pt[i] = (ident_phys & 0xFFFFF000U) | PTE_PRESENT | PTE_RW;
        }

        dir[pd_index] = ((uint32_t)pt & 0xFFFFF000U) | PDE_PRESENT | PDE_RW | PDE_USER;
    }

    uint32_t* pt = (uint32_t*)(dir[pd_index] & 0xFFFFF000U);
    pt[pt_index] = (physical_addr & 0xFFFFF000U) | (flags & 0x0FFFU) | PTE_PRESENT;

    asm volatile("invlpg (%0)" :: "r"((void*)virtual_addr) : "memory");
}

void vmm_switch_directory(uint32_t* dir) {
    asm volatile("mov %0, %%cr3" :: "r"(dir));
}

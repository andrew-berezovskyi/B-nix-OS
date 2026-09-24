#include "pmm.h"

#define PMM_MAX_BLOCKS 1048576U
#define PMM_BITMAP_WORDS (PMM_MAX_BLOCKS / 32U)

static uint32_t memory_bitmap[PMM_BITMAP_WORDS];
static uint32_t max_blocks;
static uint32_t used_blocks;

static void bitmap_set(uint32_t bit) {
    if (bit < max_blocks) memory_bitmap[bit / 32U] |= 1U << (bit % 32U);
}

static void bitmap_unset(uint32_t bit) {
    if (bit < max_blocks) memory_bitmap[bit / 32U] &= ~(1U << (bit % 32U));
}

static int bitmap_test(uint32_t bit) {
    return bit >= max_blocks || (memory_bitmap[bit / 32U] & (1U << (bit % 32U))) != 0;
}

void pmm_reserve_region(uint32_t base, uint32_t size) {
    uint32_t first = base / PMM_BLOCK_SIZE;
    uint64_t end = (uint64_t)base + size;
    uint64_t rounded = end + PMM_BLOCK_SIZE - 1U;
    uint32_t last = rounded / PMM_BLOCK_SIZE > max_blocks
        ? max_blocks : (uint32_t)(rounded / PMM_BLOCK_SIZE);
    for (uint32_t frame = first; frame < last; ++frame) {
        if (!bitmap_test(frame)) {
            bitmap_set(frame);
            ++used_blocks;
        }
    }
}

void pmm_free_region(uint32_t base, uint32_t size) {
    uint64_t rounded_base = (uint64_t)base + PMM_BLOCK_SIZE - 1U;
    uint32_t first = rounded_base / PMM_BLOCK_SIZE > max_blocks
        ? max_blocks : (uint32_t)(rounded_base / PMM_BLOCK_SIZE);
    uint64_t end = (uint64_t)base + size;
    uint32_t last = end / PMM_BLOCK_SIZE > max_blocks
        ? max_blocks : (uint32_t)(end / PMM_BLOCK_SIZE);
    for (uint32_t frame = first; frame < last; ++frame) {
        if (bitmap_test(frame)) {
            bitmap_unset(frame);
            --used_blocks;
        }
    }
}

void init_pmm(uint32_t mem_size_kb) {
    uint64_t bytes = (uint64_t)mem_size_kb * 1024U;
    uint64_t blocks = bytes / PMM_BLOCK_SIZE;
    max_blocks = blocks > PMM_MAX_BLOCKS ? PMM_MAX_BLOCKS : (uint32_t)blocks;
    used_blocks = max_blocks;

    for (uint32_t i = 0; i < PMM_BITMAP_WORDS; ++i) {
        memory_bitmap[i] = 0xFFFFFFFFU;
    }

    if (max_blocks > 512U) {
        pmm_free_region(2U * 1024U * 1024U, (max_blocks - 512U) * PMM_BLOCK_SIZE);
    }

    /* Kernel, boot modules, static graphics buffers and bootstrap heap. */
    pmm_reserve_region(0, 48U * 1024U * 1024U);
}

uint32_t pmm_alloc_block(void) {
    for (uint32_t word = 0; word < (max_blocks + 31U) / 32U; ++word) {
        if (memory_bitmap[word] == 0xFFFFFFFFU) continue;
        for (uint32_t bit = 0; bit < 32U; ++bit) {
            uint32_t frame = word * 32U + bit;
            if (frame < max_blocks && !bitmap_test(frame)) {
                bitmap_set(frame);
                ++used_blocks;
                return frame * PMM_BLOCK_SIZE;
            }
        }
    }
    return 0;
}

void pmm_free_block(uint32_t address) {
    if ((address & (PMM_BLOCK_SIZE - 1U)) != 0) return;
    uint32_t frame = address / PMM_BLOCK_SIZE;
    if (frame < max_blocks && bitmap_test(frame)) {
        bitmap_unset(frame);
        --used_blocks;
    }
}

uint32_t pmm_get_free_block_count(void) {
    return max_blocks - used_blocks;
}

uint32_t pmm_get_max_blocks(void) {
    return max_blocks;
}

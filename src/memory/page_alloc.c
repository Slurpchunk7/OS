#include "page_alloc.h"
#include "../print.h"

static uint8_t bitmap[PAGE_COUNT / 8];

static void bitmap_set(int idx)   { bitmap[idx / 8] |=  (1 << (idx % 8)); }
static void bitmap_clear(int idx) { bitmap[idx / 8] &= ~(1 << (idx % 8)); }
static bool bitmap_get(int idx)   { return (bitmap[idx / 8] >> (idx % 8)) & 1; }

static int addr_to_idx(void* addr) {
    return ((uintptr_t)addr - RAM_BASE) / PAGE_SIZE;
}

static void* idx_to_addr(int idx) {
    return (void*)(RAM_BASE + idx * PAGE_SIZE);
}

void page_alloc_init() {
    for (int i = 0; i < PAGE_COUNT / 8; i++) bitmap[i] = 0xFF;

    uintptr_t free_start = 0x44040000UL;
    uintptr_t free_end   = RAM_BASE + RAM_SIZE;

    int start_idx = (free_start - RAM_BASE) / PAGE_SIZE;
    int end_idx   = (free_end   - RAM_BASE) / PAGE_SIZE;

    for (int i = start_idx; i < end_idx; i++) bitmap_clear(i);

    uart_puts("page_alloc: init, free pages=");
    print_int(page_free_count());
    uart_putc('\n');
}

void* page_alloc() {
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (!bitmap_get(i)) {
            bitmap_set(i);
            return idx_to_addr(i);
        }
    }
    uart_puts("page_alloc: OUT OF MEMORY\n");
    return 0;
}

void* page_alloc_contiguous(int n) {
    int run = 0;
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (!bitmap_get(i)) {
            run++;
            if (run == n) {
                int start = i - n + 1;
                for (int j = start; j <= i; j++) bitmap_set(j);
                return idx_to_addr(start);
            }
        } else {
            run = 0;
        }
    }
    uart_puts("page_alloc: no contiguous region\n");
    return 0;
}

void page_free(void* addr) {
    bitmap_clear(addr_to_idx(addr));
}

void page_free_contiguous(void* addr, int n) {
    int start = addr_to_idx(addr);
    for (int i = start; i < start + n; i++) bitmap_clear(i);
}

int page_free_count() {
    int count = 0;
    for (int i = 0; i < PAGE_COUNT; i++)
        if (!bitmap_get(i)) count++;
    return count;
}
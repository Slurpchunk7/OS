#include "heap.h"
#include "page_alloc.h"
#include "../print.h"

#define HEAP_PAGES      16          // 64KB initial heap
#define LARGE_THRESHOLD 2048        // allocations above this get whole pages

#define MAGIC_FREE 0xDEADBEEF
#define MAGIC_USED 0xCAFEBABE

typedef struct block_t {
    uint32_t       magic;
    uint32_t       size;       // usable bytes, not including header
    struct block_t *next;
    struct block_t *prev;
} block_t;

#define HEADER_SIZE sizeof(block_t)
#define MIN_SPLIT   (HEADER_SIZE + 16)  // don't split if remainder < this

static block_t *heap_head = 0;

static void coalesce(block_t *b) {
    // merge with next
    if (b->next && b->next->magic == MAGIC_FREE) {
        b->size += HEADER_SIZE + b->next->size;
        b->next  = b->next->next;
        if (b->next) b->next->prev = b;
    }
    // merge with prev
    if (b->prev && b->prev->magic == MAGIC_FREE) {
        b->prev->size += HEADER_SIZE + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void heap_init() {
    void *mem = page_alloc_contiguous(HEAP_PAGES);
    if (!mem) {
        uart_puts("heap_init: failed to allocate pages\n");
        return;
    }

    heap_head        = (block_t *)mem;
    heap_head->magic = MAGIC_FREE;
    heap_head->size  = HEAP_PAGES * PAGE_SIZE - HEADER_SIZE;
    heap_head->next  = 0;
    heap_head->prev  = 0;

    uart_puts("heap_init: ");
    print_int(HEAP_PAGES * PAGE_SIZE / 1024);
    uart_puts("KB at ");
    print_hex((uintptr_t)mem);
    uart_putc('\n');
}

#define LARGE_MAGIC 0xBEEFCAFE

typedef struct {
    uint32_t magic;
    uint32_t pages;
} large_header_t;

void* kmalloc(uint32_t size) {
    if (size == 0) return 0;
    size = (size + 7) & ~7u;

    if (size > LARGE_THRESHOLD) {
        int pages = (size + sizeof(large_header_t) + PAGE_SIZE - 1) / PAGE_SIZE;
        large_header_t* hdr = (large_header_t*)page_alloc_contiguous(pages);
        if (!hdr) { uart_puts("kmalloc: large alloc failed\n"); return 0; }
        hdr->magic = LARGE_MAGIC;
        hdr->pages = pages;
        return (void*)((uint8_t*)hdr + sizeof(large_header_t));
    }

    // walk free list
    block_t *b = heap_head;
    while (b) {
        if (b->magic == MAGIC_FREE && b->size >= size) {
            // split if there's enough room left over
            if (b->size >= size + MIN_SPLIT) {
                block_t *split  = (block_t *)((uint8_t *)b + HEADER_SIZE + size);
                split->magic    = MAGIC_FREE;
                split->size     = b->size - size - HEADER_SIZE;
                split->next     = b->next;
                split->prev     = b;
                if (b->next) b->next->prev = split;
                b->next         = split;
                b->size         = size;
            }
            b->magic = MAGIC_USED;
            return (void *)((uint8_t *)b + HEADER_SIZE);
        }
        b = b->next;
    }

    // heap exhausted, grab another page
    uart_puts("kmalloc: heap exhausted, expanding\n");
    void *mem = page_alloc_contiguous(4);
    if (!mem) {
        uart_puts("kmalloc: out of memory\n");
        return 0;
    }

    block_t *new_block  = (block_t *)mem;
    new_block->magic    = MAGIC_FREE;
    new_block->size     = 4 * PAGE_SIZE - HEADER_SIZE;
    new_block->next     = 0;

    // append to end of list
    block_t *tail = heap_head;
    while (tail->next) tail = tail->next;
    tail->next       = new_block;
    new_block->prev  = tail;

    // try again
    return kmalloc(size);
}

void kfree(void* ptr) {
    if (!ptr) return;

    // check if this is a large allocation
    large_header_t* lhdr = (large_header_t*)((uint8_t*)ptr - sizeof(large_header_t));
    if (lhdr->magic == LARGE_MAGIC) {
        lhdr->magic = 0;
        page_free_contiguous(lhdr, lhdr->pages);
        return;
    }

    // normal heap block
    block_t* b = (block_t*)((uint8_t*)ptr - HEADER_SIZE);
    if (b->magic != MAGIC_USED) {
        uart_puts("kfree: bad magic, double free or corruption at ");
        print_hex((uintptr_t)ptr);
        uart_putc('\n');
        return;
    }

    b->magic = MAGIC_FREE;
    coalesce(b);
}
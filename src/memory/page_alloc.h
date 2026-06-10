#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_SIZE       4096
#define RAM_BASE        0x40000000UL
#define RAM_SIZE        (128 * 1024 * 1024)
#define PAGE_COUNT      (RAM_SIZE / PAGE_SIZE)  // 32768

void   page_alloc_init();
void*  page_alloc();
void*  page_alloc_contiguous(int n);
void   page_free(void* addr);
void   page_free_contiguous(void* addr, int n);
int    page_free_count();

#ifdef __cplusplus
}
#endif
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void  heap_init();
void* kmalloc(uint32_t size);
void  kfree(void* ptr);

#ifdef __cplusplus
}
#endif
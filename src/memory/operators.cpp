#include "heap.h"

void* operator new(unsigned long size) {
    return kmalloc((uint32_t)size);
}

void* operator new[](unsigned long size) {
    return kmalloc((uint32_t)size);
}

void operator delete(void* ptr) {
    kfree(ptr);
}

void operator delete[](void* ptr) {
    kfree(ptr);
}

void operator delete(void* ptr, unsigned long) {
    kfree(ptr);
}

void operator delete[](void* ptr, unsigned long) {
    kfree(ptr);
}
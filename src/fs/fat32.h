#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     name[256];
    uint32_t size;
    bool     is_dir;
    uint32_t cluster;
} fat_entry_t;

bool fat32_init();
bool fat32_open(const char* path, fat_entry_t* out);
bool fat32_read(fat_entry_t* entry, void* buf);
bool fat32_list(const char* path, fat_entry_t* entries, int max, int* count);

#ifdef __cplusplus
}
#endif
#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool virtio_blk_init(uint8_t bus, uint8_t dev, uint8_t fn);
bool virtio_blk_read(uint64_t lba, uint32_t count, void* buf);
bool virtio_blk_write(uint64_t lba, uint32_t count, const void* buf);

#ifdef __cplusplus
}
#endif
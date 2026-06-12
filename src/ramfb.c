#include "ramfb.h"
#include "utils/mem.h"

#define bool char
#define true 1
#define false 0

#define fw_cfg_address ((volatile uint8_t*)0x09020000)
#define selector_register ((volatile uint16_t*)(fw_cfg_address + 8))
#define data_register ((volatile uint64_t*)(fw_cfg_address + 0))
#define dma_address ((volatile uint64_t*)(fw_cfg_address + 16))

struct QemuCfgDmaAccess {
    uint32_t control;
    uint32_t length;
    uint64_t address;
} __attribute__((packed));

struct FWCfgFile {
    uint32_t size;
    uint16_t select;
    uint16_t reserved;
    char name[56];
} __attribute__((packed));

struct FWCfgFiles {
    uint32_t count;
    struct FWCfgFile files[];
} __attribute__((packed));

bool memeq(const char* b1, const char* b2, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        if (*b1++ != *b2++) return false;
    }

    return true;
}

void fw_cfg_write_selector(uint16_t selector) {
    *selector_register = SWAP16(selector);
}

uint64_t fw_cfg_read_data() {
    return *data_register;
}

void fw_cfg_dma_transfer(volatile void* address, uint32_t length, uint32_t control) {
    volatile struct QemuCfgDmaAccess dma;
    dma.control = SWAP32(control);
    dma.address = SWAP64((uint64_t)address);
    dma.length = SWAP32(length);

    *dma_address = SWAP64((uint64_t)&dma);
    while (SWAP32(dma.control) & ~0x01);
}

void fw_cfg_dma_read(volatile void* buf, int e, int length) {
    uint32_t control = (e << 16) | 0x08 | 0x02;
    fw_cfg_dma_transfer(buf, length, control);
}

void fw_cfg_dma_write(void* buf, int e, int length) {
    uint32_t control = (e << 16) | 0x08 | 0x10;
    fw_cfg_dma_transfer(buf, length, control);
}

bool fw_cfg_find_file(struct FWCfgFile* out, const char* name) {
    uint64_t name_size = strlen(name);
    volatile uint32_t files_count = 0;
    fw_cfg_dma_read(&files_count, 0x19, sizeof(files_count));
    files_count = SWAP32(files_count);

    uint64_t directory_size = sizeof(struct FWCfgFiles) + (sizeof(struct FWCfgFile) * files_count);
    struct FWCfgFiles* directory = __builtin_alloca(directory_size);
    fw_cfg_dma_read(directory, 0x19, directory_size);

    for (int i = 0; i < files_count; i++) {
        struct FWCfgFile* file = &directory->files[i];
        if (memeq(name, file->name, name_size) == true) {
            file->size = SWAP32(file->size);
            file->select = SWAP16(file->select);
            *out = *file;

            return true;
        }
    }

    return false;
}

extern void qemu_ramfb_configure(struct QemuRamFBCfg* cfg) {
    struct FWCfgFile ramfb_file;
    fw_cfg_find_file(&ramfb_file, "etc/ramfb");

    fw_cfg_dma_write(cfg, ramfb_file.select, sizeof(struct QemuRamFBCfg));
}

extern void qemu_ramfb_make_cfg(struct QemuRamFBCfg* cfg, void* fb_address, uint32_t fb_width, uint32_t fb_height) {
    cfg->address = SWAP64((uint64_t)fb_address);
    cfg->fourcc = SWAP32(PIXEL_FORMAT_XRGB8888);
    cfg->width = SWAP32(fb_width);
    cfg->height = SWAP32(fb_height);
    cfg->flags = SWAP32(0);
    cfg->stride = SWAP32(fb_width * sizeof(uint32_t));
}

extern void set_pixel(uint32_t* fb, uint32_t x, uint32_t y, uint32_t width, uint32_t color) {
    fb[y * width + x] = color;
}
#include "ramfb.h"

#define bool char
#define true 1
#define false 0

#define QEMU_FWCFG_BASE        ((volatile uint8_t*)0x09020000)
#define QEMU_FWCFG_DATA        ((volatile uint64_t*)(QEMU_FWCFG_BASE + 0))
#define QEMU_FWCFG_SELECTOR    ((volatile uint16_t*)(QEMU_FWCFG_BASE + 8))
#define QEMU_FWCFG_DMA         ((volatile uint64_t*)(QEMU_FWCFG_BASE + 16))

#define FWCFG_DMA_FLAG_ERROR   0x01
#define FWCFG_DMA_FLAG_READ    0x02
#define FWCFG_DMA_FLAG_SKIP    0x04
#define FWCFG_DMA_FLAG_SELECT  0x08
#define FWCFG_DMA_FLAG_WRITE   0x10

#define FWCFG_FILE_DIR_SELECTOR 0x19

struct FWCfgDmaAccess {
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

struct FWCfgFileDir {
    uint32_t count;
    struct FWCfgFile files[];
} __attribute__((packed));

uint64_t kstrlen(const char* str) {
    uint64_t i = 0;
    while (*str++ != '\0') i++;
    return i;
}

bool kmemeq(const char* a, const char* b, uint64_t length) {
    for (uint64_t i = 0; i < length; i++) {
        if (*a++ != *b++) return false;
    }
    return true;
}

void fwcfg_select(uint16_t selector) {
    *QEMU_FWCFG_SELECTOR = SWAP16(selector);
}

uint64_t fwcfg_read(void) {
    return *QEMU_FWCFG_DATA;
}

void fwcfg_dma_transfer(volatile void* buf, uint32_t length, uint32_t control) {
    volatile struct FWCfgDmaAccess dma;
    dma.control = SWAP32(control);
    dma.address = SWAP64((uint64_t)buf);
    dma.length  = SWAP32(length);

    *QEMU_FWCFG_DMA = SWAP64((uint64_t)&dma);
    while (SWAP32(dma.control) & ~FWCFG_DMA_FLAG_ERROR);
}

void fwcfg_dma_read(volatile void* buf, int selector, int length) {
    uint32_t control = (selector << 16) | FWCFG_DMA_FLAG_SELECT | FWCFG_DMA_FLAG_READ;
    fwcfg_dma_transfer(buf, length, control);
}

void fwcfg_dma_write(void* buf, int selector, int length) {
    uint32_t control = (selector << 16) | FWCFG_DMA_FLAG_SELECT | FWCFG_DMA_FLAG_WRITE;
    fwcfg_dma_transfer(buf, length, control);
}

bool fwcfg_find_file(struct FWCfgFile* out, const char* name) {
    uint64_t name_len = kstrlen(name);

    volatile uint32_t file_count = 0;
    fwcfg_dma_read(&file_count, FWCFG_FILE_DIR_SELECTOR, sizeof(file_count));
    file_count = SWAP32(file_count);

    uint64_t dir_size = sizeof(struct FWCfgFileDir) + (sizeof(struct FWCfgFile) * file_count);
    struct FWCfgFileDir* dir = __builtin_alloca(dir_size);
    fwcfg_dma_read(dir, FWCFG_FILE_DIR_SELECTOR, dir_size);

    for (int i = 0; i < file_count; i++) {
        struct FWCfgFile* file = &dir->files[i];
        if (kmemeq(name, file->name, name_len)) {
            file->size   = SWAP32(file->size);
            file->select = SWAP16(file->select);
            *out = *file;
            return true;
        }
    }

    return false;
}

void qemu_ramfb_configure(struct QemuRamFBCfg* cfg) {
    struct FWCfgFile ramfb_file;
    fwcfg_find_file(&ramfb_file, "etc/ramfb");
    fwcfg_dma_write(cfg, ramfb_file.select, sizeof(struct QemuRamFBCfg));
}

void qemu_ramfb_make_cfg(struct QemuRamFBCfg* cfg, void* fb_address, uint32_t fb_width, uint32_t fb_height) {
    cfg->address = SWAP64((uint64_t)fb_address);
    cfg->fourcc  = SWAP32(PIXEL_FORMAT_XRGB8888);
    cfg->width   = SWAP32(fb_width);
    cfg->height  = SWAP32(fb_height);
    cfg->flags   = SWAP32(0);
    cfg->stride  = SWAP32(fb_width * sizeof(uint32_t));
}
#include "virtio_blk.h"
#include "pci.h"
#include "print.h"
#include "memory/page_alloc.h"
#include <stddef.h>
#include <stdbool.h>

// fixed physical addresses for blk driver state
#define BLK_BASE        0x44050000UL
#define BLK_VRING       (BLK_BASE + 0x0000)  // 0x1000
#define BLK_BUFFERS     (BLK_BASE + 0x1000)  // request buffers
#define BLK_STATE       (BLK_BASE + 0x2000)  // blk_t state

#define BAR_ALLOC_BASE  0x10008000UL  // after virtio-input bars

#define VRING_SIZE      16
#define SECTOR_SIZE     512

#define PCI_ECAM_BASE       0x4010000000ULL
#define PCI_CFG_COMMAND     0x04
#define PCI_CFG_BAR0        0x10
#define PCI_CFG_CAP_PTR     0x34
#define PCI_CMD_MEM_SPACE   (1u << 1)
#define PCI_CMD_BUS_MASTER  (1u << 2)
#define PCI_CAP_VENDOR      0x09

#define VIRTIO_PCI_CAP_COMMON_CFG   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2
#define VIRTIO_PCI_CAP_DEVICE_CFG   4

#define VIRTIO_STATUS_ACKNOWLEDGE   0x01
#define VIRTIO_STATUS_DRIVER        0x02
#define VIRTIO_STATUS_DRIVER_OK     0x04
#define VIRTIO_STATUS_FEATURES_OK   0x08
#define VIRTIO_STATUS_FAILED        0x80

#define VIRTIO_BLK_T_IN     0
#define VIRTIO_BLK_T_OUT    1
#define VIRTIO_BLK_S_OK     0

#define CC_DEVICE_FEATURE_SEL   0
#define CC_DEVICE_FEATURE       4
#define CC_DRIVER_FEATURE_SEL   8
#define CC_DRIVER_FEATURE       12
#define CC_DEVICE_STATUS        20
#define CC_QUEUE_SELECT         22
#define CC_QUEUE_SIZE           24
#define CC_QUEUE_MSIX_VECTOR    26
#define CC_QUEUE_ENABLE         28
#define CC_QUEUE_NOTIFY_OFF     30
#define CC_QUEUE_DESC_LO        32
#define CC_QUEUE_DESC_HI        36
#define CC_QUEUE_DRIVER_LO      40
#define CC_QUEUE_DRIVER_HI      44
#define CC_QUEUE_DEVICE_LO      48
#define CC_QUEUE_DEVICE_HI      52

#define VRING_DESC_F_NEXT   1u
#define VRING_DESC_F_WRITE  2u

static inline void     wr32(uintptr_t a, uint32_t v) { *(volatile uint32_t*)a = v; }
static inline void     wr16(uintptr_t a, uint16_t v) { *(volatile uint16_t*)a = v; }
static inline void     wr8 (uintptr_t a, uint8_t  v) { *(volatile uint8_t *)a = v; }
static inline uint32_t rd32(uintptr_t a) { return *(volatile uint32_t*)a; }
static inline uint16_t rd16(uintptr_t a) { return *(volatile uint16_t*)a; }
static inline uint8_t  rd8 (uintptr_t a) { return *(volatile uint8_t *)a; }

static uintptr_t ecam(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off) {
    return (uintptr_t)(PCI_ECAM_BASE
        | ((uint64_t)bus << 20) | ((uint64_t)dev << 15)
        | ((uint64_t)fn  << 12) | off);
}
static void pci_wr32(uint8_t b,uint8_t d,uint8_t f,uint16_t o,uint32_t v){ wr32(ecam(b,d,f,o),v); }
static void pci_wr16(uint8_t b,uint8_t d,uint8_t f,uint16_t o,uint16_t v){ wr16(ecam(b,d,f,o),v); }

// virtio-blk request header
typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed)) blk_req_t;

typedef struct {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
} __attribute__((packed)) vring_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VRING_SIZE];
    uint16_t used_event;
} __attribute__((packed)) vring_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} vring_used_elem_t;

typedef struct {
    uint16_t          flags;
    uint16_t          idx;
    vring_used_elem_t ring[VRING_SIZE];
    uint16_t          avail_event;
} __attribute__((packed)) vring_used_t;

typedef struct {
    uintptr_t cc;
    uintptr_t notify_addr;
    uintptr_t desc_pa;
    uintptr_t avail_pa;
    uintptr_t used_pa;
    uint16_t  avail_idx;
    uint16_t  last_used_idx;
    bool      ready;
} blk_t;

static blk_t *const blk = (blk_t*)BLK_STATE;

static void zero_mem(uintptr_t addr, size_t len) {
    volatile uint8_t *p = (volatile uint8_t*)addr;
    for (size_t i = 0; i < len; i++) p[i] = 0;
}

static void program_bars(uint8_t bus, uint8_t dev, uint8_t fn,
                         uintptr_t bar_base[6]) {
    uintptr_t alloc = BAR_ALLOC_BASE;
    for (int b = 0; b < 6; b++) {
        uint16_t off  = PCI_CFG_BAR0 + b * 4;
        uint32_t orig = pci_read32(bus, dev, fn, off);
        if (orig & 1) { bar_base[b] = 0; continue; }

        pci_wr32(bus, dev, fn, off, 0xFFFFFFFF);
        uint32_t sz = pci_read32(bus, dev, fn, off);
        pci_wr32(bus, dev, fn, off, orig);

        if (sz == 0 || sz == 0xFFFFFFFF) { bar_base[b] = 0; continue; }

        bool is64 = ((sz >> 1) & 3) == 2;
        uint32_t size = ~(sz & ~0xFu) + 1;

        alloc = (alloc + size - 1) & ~(uintptr_t)(size - 1);
        bar_base[b] = alloc;
        pci_wr32(bus, dev, fn, off, (uint32_t)alloc);

        if (is64) {
            pci_wr32(bus, dev, fn, PCI_CFG_BAR0 + (b+1)*4, 0);
            bar_base[b+1] = 0;
            b++;
        }

        uart_puts("  BLK BAR"); print_hex(b);
        uart_puts(" = "); print_hex((uint32_t)alloc);
        uart_puts(" size="); print_hex(size); uart_putc('\n');
        alloc += size;
    }
}

bool virtio_blk_init(uint8_t bus, uint8_t dev, uint8_t fn) {
    uart_puts("virtio-blk: probe\n");

    zero_mem(BLK_VRING,   0x1000);
    zero_mem(BLK_BUFFERS, 0x1000);
    zero_mem(BLK_STATE,   sizeof(blk_t));

    uintptr_t bar_base[6] = {0};
    program_bars(bus, dev, fn, bar_base);

    // find caps
    uint16_t cmd = pci_read16(bus, dev, fn, PCI_CFG_COMMAND);
    pci_wr16(bus, dev, fn, PCI_CFG_COMMAND,
             cmd | PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER);

    uint8_t ptr = (uint8_t)pci_read32(bus, dev, fn, PCI_CFG_CAP_PTR);
    uintptr_t cc = 0, notify = 0;
    uint32_t notify_mult = 0;

    while (ptr) {
        uint32_t w0   = pci_read32(bus, dev, fn, ptr);
        uint8_t  id   = w0 & 0xFF;
        uint8_t  next = (w0 >> 8) & 0xFF;
        uint8_t  type = (w0 >> 24) & 0xFF;

        if (id == PCI_CAP_VENDOR) {
            uint8_t  bar = pci_read32(bus, dev, fn, ptr + 4) & 0xFF;
            uint32_t off = pci_read32(bus, dev, fn, ptr + 8);
            if (bar < 6 && bar_base[bar]) {
                uintptr_t base = bar_base[bar] + off;
                if (type == VIRTIO_PCI_CAP_COMMON_CFG) cc = base;
                if (type == VIRTIO_PCI_CAP_NOTIFY_CFG) {
                    notify      = base;
                    notify_mult = pci_read32(bus, dev, fn, ptr + 16);
                }
            }
        }
        ptr = next;
    }

    if (!cc || !notify) {
        uart_puts("virtio-blk: missing caps\n");
        return false;
    }

    blk->cc = cc;

    // reset
    wr8(cc + CC_DEVICE_STATUS, 0);
    while (rd8(cc + CC_DEVICE_STATUS) != 0);
    wr8(cc + CC_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    // check VERSION_1
    wr32(cc + CC_DEVICE_FEATURE_SEL, 1);
    uint32_t feat_hi = rd32(cc + CC_DEVICE_FEATURE);
    if (!(feat_hi & 1)) {
        uart_puts("virtio-blk: no VERSION_1\n");
        wr8(cc + CC_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return false;
    }

    wr32(cc + CC_DRIVER_FEATURE_SEL, 0); wr32(cc + CC_DRIVER_FEATURE, 0);
    wr32(cc + CC_DRIVER_FEATURE_SEL, 1); wr32(cc + CC_DRIVER_FEATURE, 1);

    uint8_t s = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER
              | VIRTIO_STATUS_FEATURES_OK;
    wr8(cc + CC_DEVICE_STATUS, s);
    if (!(rd8(cc + CC_DEVICE_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
        uart_puts("virtio-blk: FEATURES_OK rejected\n");
        wr8(cc + CC_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return false;
    }

    // setup queue 0
    wr16(cc + CC_QUEUE_SELECT, 0);
    uint16_t max = rd16(cc + CC_QUEUE_SIZE);
    uint16_t sz  = (max < VRING_SIZE) ? max : VRING_SIZE;
    wr16(cc + CC_QUEUE_SIZE, sz);

    uint16_t  noff        = rd16(cc + CC_QUEUE_NOTIFY_OFF);
    uintptr_t notify_addr = notify + (uint64_t)noff * notify_mult;
    blk->notify_addr = notify_addr;

    uintptr_t desc_pa  = BLK_VRING;
    uintptr_t avail_pa = BLK_VRING + VRING_SIZE * sizeof(vring_desc_t);
    uintptr_t used_pa  = BLK_VRING + 0x800;

    wr32(cc + CC_QUEUE_DESC_LO,   (uint32_t) desc_pa);
    wr32(cc + CC_QUEUE_DESC_HI,   (uint32_t)(desc_pa  >> 32));
    wr32(cc + CC_QUEUE_DRIVER_LO, (uint32_t) avail_pa);
    wr32(cc + CC_QUEUE_DRIVER_HI, (uint32_t)(avail_pa >> 32));
    wr32(cc + CC_QUEUE_DEVICE_LO, (uint32_t) used_pa);
    wr32(cc + CC_QUEUE_DEVICE_HI, (uint32_t)(used_pa  >> 32));
    wr16(cc + CC_QUEUE_MSIX_VECTOR, 0xFFFF);
    wr16(cc + CC_QUEUE_ENABLE, 1);

    blk->desc_pa       = desc_pa;
    blk->avail_pa      = avail_pa;
    blk->used_pa       = used_pa;
    blk->avail_idx     = 0;
    blk->last_used_idx = 0;

    wr8(cc + CC_DEVICE_STATUS, s | VIRTIO_STATUS_DRIVER_OK);

    blk->ready = true;
    uart_puts("virtio-blk: ready\n");
    return true;
}

// synchronous read/write — submits request and polls until done
static bool blk_do(uint32_t type, uint64_t lba, uint32_t count, void* buf) {
    if (!blk->ready) return false;

    vring_desc_t  *desc  = (vring_desc_t  *)blk->desc_pa;
    vring_avail_t *avail = (vring_avail_t *)blk->avail_pa;
    vring_used_t  *used  = (vring_used_t  *)blk->used_pa;

    // layout in BLK_BUFFERS:
    // [0]  blk_req_t  header  (16 bytes)
    // [16] data buffer        (count * 512 bytes)
    // [16 + count*512] status (1 byte)
    uintptr_t req_pa    = BLK_BUFFERS;
    uintptr_t data_pa   = BLK_BUFFERS + 16;
    uintptr_t status_pa = BLK_BUFFERS + 16 + count * SECTOR_SIZE;

    blk_req_t *req = (blk_req_t*)req_pa;
    req->type      = type;
    req->reserved  = 0;
    req->sector    = lba;

    // copy write data into buffer
    if (type == VIRTIO_BLK_T_OUT) {
        uint8_t *src = (uint8_t*)buf;
        uint8_t *dst = (uint8_t*)data_pa;
        for (uint32_t i = 0; i < count * SECTOR_SIZE; i++) dst[i] = src[i];
    }

    *(volatile uint8_t*)status_pa = 0xFF; // will be written by device

    // 3 descriptors: header | data | status
    desc[0].addr  = req_pa;
    desc[0].len   = 16;
    desc[0].flags = VRING_DESC_F_NEXT;
    desc[0].next  = 1;

    desc[1].addr  = data_pa;
    desc[1].len   = count * SECTOR_SIZE;
    desc[1].flags = VRING_DESC_F_NEXT | (type == VIRTIO_BLK_T_IN ? VRING_DESC_F_WRITE : 0);
    desc[1].next  = 2;

    desc[2].addr  = status_pa;
    desc[2].len   = 1;
    desc[2].flags = VRING_DESC_F_WRITE;
    desc[2].next  = 0;

    avail->ring[blk->avail_idx & (VRING_SIZE - 1)] = 0;
    __asm__ volatile ("dmb ishst" ::: "memory");
    avail->idx = ++blk->avail_idx;
    __asm__ volatile ("dmb ishst" ::: "memory");

    wr16(blk->notify_addr, 0);

    // poll until used ring advances
    while (used->idx == blk->last_used_idx)
        __asm__ volatile ("dmb ish" ::: "memory");
    blk->last_used_idx++;

    uint8_t status = *(volatile uint8_t*)status_pa;
    if (status != VIRTIO_BLK_S_OK) {
        uart_puts("virtio-blk: request failed, status=");
        print_hex(status);
        uart_putc('\n');
        return false;
    }

    // copy read data out
    if (type == VIRTIO_BLK_T_IN) {
        uint8_t *src = (uint8_t*)data_pa;
        uint8_t *dst = (uint8_t*)buf;
        for (uint32_t i = 0; i < count * SECTOR_SIZE; i++) dst[i] = src[i];
    }

    return true;
}

bool virtio_blk_read(uint64_t lba, uint32_t count, void* buf) {
    return blk_do(VIRTIO_BLK_T_IN, lba, count, buf);
}

bool virtio_blk_write(uint64_t lba, uint32_t count, const void* buf) {
    return blk_do(VIRTIO_BLK_T_OUT, lba, count, (void*)buf);
}
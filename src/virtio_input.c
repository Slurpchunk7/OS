#include "virtio_input.h"
#include "pci.h"
#include "print.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#define VIRT_BASE        0x44000000UL
#define VIRT_REGION_SIZE 0x10000UL   // 64KB per device

#define BAR_ALLOC_BASE   0x10000000UL

// per-device memory layout (within each 64KB region):
// 0x0000 - vring evt
// 0x1000 - vring sts
// 0x2000 - evt bufs
// 0x3000 - vi_t state

#define VRING_SIZE      16
#define EVT_BUF_COUNT   VRING_SIZE
#define MAX_DEVICES     4

#define PCI_ECAM_BASE        0x4010000000ULL
#define PCI_CFG_COMMAND      0x04
#define PCI_CFG_BAR0         0x10
#define PCI_CFG_CAP_PTR      0x34
#define PCI_CMD_MEM_SPACE    (1u << 1)
#define PCI_CMD_BUS_MASTER   (1u << 2)
#define PCI_CAP_VENDOR       0x09

#define VIRTIO_PCI_CAP_COMMON_CFG   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2
#define VIRTIO_PCI_CAP_ISR_CFG      3
#define VIRTIO_PCI_CAP_DEVICE_CFG   4

#define VIRTIO_STATUS_ACKNOWLEDGE   0x01
#define VIRTIO_STATUS_DRIVER        0x02
#define VIRTIO_STATUS_DRIVER_OK     0x04
#define VIRTIO_STATUS_FEATURES_OK   0x08
#define VIRTIO_STATUS_FAILED        0x80

#define CC_DEVICE_FEATURE_SEL   0
#define CC_DEVICE_FEATURE       4
#define CC_DRIVER_FEATURE_SEL   8
#define CC_DRIVER_FEATURE       12
#define CC_MSIX_CONFIG          16
#define CC_NUM_QUEUES           18
#define CC_DEVICE_STATUS        20
#define CC_CONFIG_GEN           21
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

#define DC_SELECT   0
#define DC_SUBSEL   1
#define DC_SIZE     2

#define VIRTIO_INPUT_CFG_ID_NAME    0x01
#define VIRTIO_INPUT_CFG_ID_SERIAL  0x02
#define VIRTIO_INPUT_CFG_EV_BITS    0x11

static inline void     wr32(uintptr_t a, uint32_t v) { *(volatile uint32_t *)a = v; }
static inline void     wr16(uintptr_t a, uint16_t v) { *(volatile uint16_t *)a = v; }
static inline void     wr8 (uintptr_t a, uint8_t  v) { *(volatile uint8_t  *)a = v; }
static inline uint32_t rd32(uintptr_t a) { return *(volatile uint32_t *)a; }
static inline uint16_t rd16(uintptr_t a) { return *(volatile uint16_t *)a; }
static inline uint8_t  rd8 (uintptr_t a) { return *(volatile uint8_t  *)a; }

static uintptr_t ecam(uint8_t bus, uint8_t dev, uint8_t fn, uint16_t off) {
    return (uintptr_t)(PCI_ECAM_BASE
        | ((uint64_t)bus << 20) | ((uint64_t)dev << 15)
        | ((uint64_t)fn  << 12) | off);
}
static void pci_wr32(uint8_t b,uint8_t d,uint8_t f,uint16_t o,uint32_t v){ wr32(ecam(b,d,f,o),v); }
static void pci_wr16(uint8_t b,uint8_t d,uint8_t f,uint16_t o,uint16_t v){ wr16(ecam(b,d,f,o),v); }

#define VRING_DESC_F_WRITE   2u

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
    uintptr_t  desc_pa;
    uintptr_t  avail_pa;
    uintptr_t  used_pa;
    uint16_t   free_head;
    uint16_t   avail_idx;
    uint16_t   last_used_idx;
    uintptr_t  notify_addr;
} virtq_t;

typedef struct {
    uintptr_t cc;
    uintptr_t dc;
    uintptr_t notify_base;
    uint32_t  notify_mult;
    virtq_t   vq[2];
    bool      ready;
} vi_t;

// helpers to get per-device memory regions
static uintptr_t dev_base(int idx)      { return VIRT_BASE + idx * VIRT_REGION_SIZE; }
static uintptr_t dev_vring_evt(int idx) { return dev_base(idx) + 0x0000; }
static uintptr_t dev_vring_sts(int idx) { return dev_base(idx) + 0x1000; }
static uintptr_t dev_evt_bufs(int idx)  { return dev_base(idx) + 0x2000; }
static uintptr_t dev_vq_state(int idx)  { return dev_base(idx) + 0x3000; }
static vi_t*     dev_vi(int idx)        { return (vi_t *)dev_vq_state(idx); }

static void zero_mem(uintptr_t addr, size_t len) {
    volatile uint8_t *p = (volatile uint8_t *)addr;
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
            pci_wr32(bus, dev, fn, PCI_CFG_BAR0 + (b + 1) * 4, 0);
            bar_base[b + 1] = 0;
            b++;
        }

        uart_puts("  BAR"); print_hex(is64 ? b - 1 : b);
        uart_puts(" = "); print_hex((uint32_t)alloc);
        uart_puts(" size="); print_hex(size); uart_putc('\n');
        alloc += size;
    }
}

static bool find_caps(uint8_t bus, uint8_t dev, uint8_t fn,
                      uintptr_t bar_base[6],
                      uintptr_t *cc, uintptr_t *notify,
                      uint32_t *notify_mult, uintptr_t *dc) {
    uint16_t cmd = pci_read16(bus, dev, fn, PCI_CFG_COMMAND);
    pci_wr16(bus, dev, fn, PCI_CFG_COMMAND,
             cmd | PCI_CMD_MEM_SPACE | PCI_CMD_BUS_MASTER);

    uint8_t ptr = (uint8_t)pci_read32(bus, dev, fn, PCI_CFG_CAP_PTR);
    bool got_cc = false, got_notify = false, got_dc = false;

    while (ptr) {
        uint32_t w0   = pci_read32(bus, dev, fn, ptr);
        uint8_t  id   = w0 & 0xFF;
        uint8_t  next = (w0 >> 8) & 0xFF;
        uint8_t  type = (w0 >> 24) & 0xFF;

        if (id != PCI_CAP_VENDOR) { ptr = next; continue; }

        uint8_t  bar = pci_read32(bus, dev, fn, ptr + 4) & 0xFF;
        uint32_t off = pci_read32(bus, dev, fn, ptr + 8);

        if (bar < 6 && bar_base[bar]) {
            uintptr_t base = bar_base[bar] + off;
            switch (type) {
            case VIRTIO_PCI_CAP_COMMON_CFG:
                *cc = base; got_cc = true;
                break;
            case VIRTIO_PCI_CAP_NOTIFY_CFG:
                *notify      = base;
                *notify_mult = pci_read32(bus, dev, fn, ptr + 16);
                got_notify   = true;
                break;
            case VIRTIO_PCI_CAP_DEVICE_CFG:
                *dc = base; got_dc = true;
                break;
            }
        }
        ptr = next;
    }

    if (!got_cc)     { uart_puts("virtio: no common cfg\n");  return false; }
    if (!got_notify) { uart_puts("virtio: no notify cap\n");  return false; }
    if (!got_dc)     { uart_puts("virtio: no device cfg\n");  return false; }
    return true;
}

static bool setup_vq(vi_t *vi, uintptr_t cc, int idx,
                     uintptr_t desc_pa, uintptr_t avail_pa, uintptr_t used_pa,
                     uintptr_t notify_base, uint32_t notify_mult) {
    wr16(cc + CC_QUEUE_SELECT, (uint16_t)idx);

    uint16_t max = rd16(cc + CC_QUEUE_SIZE);
    if (!max) {
        uart_puts("virtio: queue "); print_hex(idx); uart_puts(" absent\n");
        return false;
    }
    uint16_t sz = (max < VRING_SIZE) ? max : VRING_SIZE;
    wr16(cc + CC_QUEUE_SIZE, sz);

    uint16_t  noff        = rd16(cc + CC_QUEUE_NOTIFY_OFF);
    uintptr_t notify_addr = notify_base + (uint64_t)noff * notify_mult;

    wr32(cc + CC_QUEUE_DESC_LO,   (uint32_t) desc_pa);
    wr32(cc + CC_QUEUE_DESC_HI,   (uint32_t)(desc_pa  >> 32));
    wr32(cc + CC_QUEUE_DRIVER_LO, (uint32_t) avail_pa);
    wr32(cc + CC_QUEUE_DRIVER_HI, (uint32_t)(avail_pa >> 32));
    wr32(cc + CC_QUEUE_DEVICE_LO, (uint32_t) used_pa);
    wr32(cc + CC_QUEUE_DEVICE_HI, (uint32_t)(used_pa  >> 32));
    wr16(cc + CC_QUEUE_MSIX_VECTOR, 0xFFFF);
    wr16(cc + CC_QUEUE_ENABLE, 1);

    virtq_t *vq       = &vi->vq[idx];
    vq->desc_pa       = desc_pa;
    vq->avail_pa      = avail_pa;
    vq->used_pa       = used_pa;
    vq->free_head     = 0;
    vq->avail_idx     = 0;
    vq->last_used_idx = 0;
    vq->notify_addr   = notify_addr;

    vring_desc_t *desc = (vring_desc_t *)desc_pa;
    for (int i = 0; i < VRING_SIZE - 1; i++) desc[i].next = i + 1;
    desc[VRING_SIZE - 1].next = 0;

    return true;
}

static void vq_add_inbuf(vi_t *vi, int idx, uintptr_t buf_pa, uint32_t len) {
    virtq_t       *vq    = &vi->vq[idx];
    vring_desc_t  *desc  = (vring_desc_t *)vq->desc_pa;
    vring_avail_t *avail = (vring_avail_t *)vq->avail_pa;

    uint16_t d    = vq->free_head;
    vq->free_head = desc[d].next;

    desc[d].addr  = (uint64_t)buf_pa;
    desc[d].len   = len;
    desc[d].flags = VRING_DESC_F_WRITE;
    desc[d].next  = 0;

    avail->ring[vq->avail_idx & (VRING_SIZE - 1)] = d;
    __asm__ volatile ("dmb ishst" ::: "memory");
    avail->idx = ++vq->avail_idx;
    __asm__ volatile ("dmb ishst" ::: "memory");
}

static void vq_kick(vi_t *vi, int idx) {
    __asm__ volatile ("dmb ish" ::: "memory");
    wr16(vi->vq[idx].notify_addr, (uint16_t)idx);
}

static uint8_t cfg_select(vi_t *vi, uint8_t sel, uint8_t subsel) {
    wr8(vi->dc + DC_SELECT, sel);
    wr8(vi->dc + DC_SUBSEL, subsel);
    return rd8(vi->dc + DC_SIZE);
}

bool virtio_input_init(uint8_t bus, uint8_t dev, uint8_t fn, int device_idx) {
    if (device_idx >= MAX_DEVICES) {
        uart_puts("virtio: device_idx out of range\n");
        return false;
    }

    uart_puts("virtio-input: probe idx="); print_hex(device_idx); uart_putc('\n');

    zero_mem(dev_vring_evt(device_idx), 0x1000);
    zero_mem(dev_vring_sts(device_idx), 0x1000);
    zero_mem(dev_evt_bufs(device_idx),  EVT_BUF_COUNT * sizeof(virtio_input_event_t));
    zero_mem(dev_vq_state(device_idx),  sizeof(vi_t));

    vi_t *vi = dev_vi(device_idx);

    uintptr_t bar_base[6] = {0};
    program_bars(bus, dev, fn, bar_base);

    uintptr_t cc = 0, notify = 0, dc = 0;
    uint32_t  notify_mult = 0;
    if (!find_caps(bus, dev, fn, bar_base, &cc, &notify, &notify_mult, &dc))
        return false;

    vi->cc          = cc;
    vi->dc          = dc;
    vi->notify_base = notify;
    vi->notify_mult = notify_mult;

    wr8(cc + CC_DEVICE_STATUS, 0);
    while (rd8(cc + CC_DEVICE_STATUS) != 0);

    wr8(cc + CC_DEVICE_STATUS, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);

    wr32(cc + CC_DEVICE_FEATURE_SEL, 0); uint32_t feat_lo = rd32(cc + CC_DEVICE_FEATURE);
    wr32(cc + CC_DEVICE_FEATURE_SEL, 1); uint32_t feat_hi = rd32(cc + CC_DEVICE_FEATURE);
    uart_puts("  features "); print_hex(feat_hi); print_hex(feat_lo); uart_putc('\n');

    if (!(feat_hi & 1)) {
        uart_puts("virtio: VIRTIO_F_VERSION_1 not offered\n");
        wr8(cc + CC_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return false;
    }

    wr32(cc + CC_DRIVER_FEATURE_SEL, 0); wr32(cc + CC_DRIVER_FEATURE, 0);
    wr32(cc + CC_DRIVER_FEATURE_SEL, 1); wr32(cc + CC_DRIVER_FEATURE, 1);

    uint8_t s = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER
              | VIRTIO_STATUS_FEATURES_OK;
    wr8(cc + CC_DEVICE_STATUS, s);
    if (!(rd8(cc + CC_DEVICE_STATUS) & VIRTIO_STATUS_FEATURES_OK)) {
        uart_puts("virtio: FEATURES_OK rejected\n");
        wr8(cc + CC_DEVICE_STATUS, VIRTIO_STATUS_FAILED);
        return false;
    }

    uintptr_t evt_desc_pa  = dev_vring_evt(device_idx);
    uintptr_t evt_avail_pa = dev_vring_evt(device_idx) + VRING_SIZE * sizeof(vring_desc_t);
    uintptr_t evt_used_pa  = dev_vring_evt(device_idx) + 0x800;

    uintptr_t sts_desc_pa  = dev_vring_sts(device_idx);
    uintptr_t sts_avail_pa = dev_vring_sts(device_idx) + VRING_SIZE * sizeof(vring_desc_t);
    uintptr_t sts_used_pa  = dev_vring_sts(device_idx) + 0x800;

    if (!setup_vq(vi, cc, 0, evt_desc_pa, evt_avail_pa, evt_used_pa, notify, notify_mult))
        return false;
    if (!setup_vq(vi, cc, 1, sts_desc_pa, sts_avail_pa, sts_used_pa, notify, notify_mult))
        return false;

    wr8(cc + CC_DEVICE_STATUS, s | VIRTIO_STATUS_DRIVER_OK);

    for (int i = 0; i < EVT_BUF_COUNT; i++) {
        uintptr_t buf_pa = dev_evt_bufs(device_idx) + i * sizeof(virtio_input_event_t);
        vq_add_inbuf(vi, 0, buf_pa, sizeof(virtio_input_event_t));
    }
    vq_kick(vi, 0);

    vi->ready = true;

    uint8_t nlen = cfg_select(vi, VIRTIO_INPUT_CFG_ID_NAME, 0);
    uart_puts("virtio-input: ready, name_len="); print_hex(nlen); uart_putc('\n');

    return true;
}

bool virtio_input_poll(virtio_input_event_t *out, int device_idx) {
    vi_t *vi = dev_vi(device_idx);
    if (!vi->ready) return false;

    virtq_t      *vq   = &vi->vq[0];
    vring_used_t *used = (vring_used_t *)vq->used_pa;

    __asm__ volatile ("dmb ish" ::: "memory");
    if (vq->last_used_idx == used->idx) return false;

    uint16_t slot    = vq->last_used_idx & (VRING_SIZE - 1);
    uint32_t desc_id = used->ring[slot].id;
    __asm__ volatile ("dmb ish" ::: "memory");

    uintptr_t buf_pa = dev_evt_bufs(device_idx) + desc_id * sizeof(virtio_input_event_t);
    volatile virtio_input_event_t *ev = (volatile virtio_input_event_t *)buf_pa;
    out->type  = ev->type;
    out->code  = ev->code;
    out->value = ev->value;

    vring_desc_t *desc = (vring_desc_t *)vq->desc_pa;
    desc[desc_id].next = vq->free_head;
    vq->free_head      = (uint16_t)desc_id;
    vq->last_used_idx++;

    vq_add_inbuf(vi, 0, buf_pa, sizeof(virtio_input_event_t));
    vq_kick(vi, 0);

    return true;
}
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_VIRTIO_INPUT_DEVICES 8

#define VIRTIO_PCI_CAP_COMMON_CFG   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2
#define VIRTIO_PCI_CAP_ISR_CFG      3
#define VIRTIO_PCI_CAP_DEVICE_CFG   4
#define VIRTIO_PCI_CAP_PCI_CFG      5

#define VIRTIO_F_VERSION_1          (1ULL << 32)

#define VIRTIO_STATUS_ACKNOWLEDGE   0x01
#define VIRTIO_STATUS_DRIVER        0x02
#define VIRTIO_STATUS_DRIVER_OK     0x04
#define VIRTIO_STATUS_FEATURES_OK   0x08
#define VIRTIO_STATUS_FAILED        0x80

#define VIRTIO_INPUT_CFG_UNSET      0x00
#define VIRTIO_INPUT_CFG_ID_NAME    0x01
#define VIRTIO_INPUT_CFG_ID_SERIAL  0x02
#define VIRTIO_INPUT_CFG_ID_DEVIDS  0x03
#define VIRTIO_INPUT_CFG_PROP_BITS  0x10
#define VIRTIO_INPUT_CFG_EV_BITS    0x11
#define VIRTIO_INPUT_CFG_ABS_INFO   0x12

#define EV_SYN  0x00
#define EV_KEY  0x01
#define EV_REL  0x02
#define EV_ABS  0x03

#define KEY_ESC         1
#define KEY_1           2
#define KEY_2           3
#define KEY_A           30
#define KEY_ENTER       28
#define KEY_BACKSPACE   14
#define KEY_SPACE       57
#define KEY_LEFTSHIFT   42
#define KEY_RIGHTSHIFT  54

typedef struct {
    uint16_t type;
    uint16_t code;
    uint32_t value;
} __attribute__((packed)) virtio_input_event_t;

bool virtio_input_init(
    uint8_t bus,
    uint8_t dev,
    uint8_t func,
    int device_idx
);

bool virtio_input_poll(
    virtio_input_event_t *out,
    int device_idx
);

#ifdef __cplusplus
}
#endif
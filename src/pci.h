#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);

uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset);

void pci_scan();

#ifdef __cplusplus
}
#endif
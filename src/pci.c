#include "pci.h"

#include "print.h"

#define PCI_ECAM_BASE 0x4010000000ULL

uint32_t pci_read32(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
    volatile uint32_t* ptr = (volatile uint32_t*)(PCI_ECAM_BASE + ((uint64_t)bus << 20) + ((uint64_t)device << 15) + ((uint64_t)function << 12) + offset);

    return *ptr;
}

uint16_t pci_read16(uint8_t bus, uint8_t device, uint8_t function, uint16_t offset) {
    uint32_t value = pci_read32(bus, device, function, offset & ~3);

    return (value >> ((offset & 2) * 8)) & 0xFFFF;
}

void pci_scan() {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            for (uint8_t func = 0; func < 8; func++) {

                uint16_t vendor = pci_read16(bus, dev, func, 0x00);

                if (vendor == 0xFFFF) {
                    continue;
                }
                
                uint16_t device = pci_read16(bus, dev, func, 0x02);

                uint32_t reg = pci_read32(bus, dev, func, 0x08);

                uint8_t class_code = (reg >> 24) & 0xFF;
                uint8_t subclass   = (reg >> 16) & 0xFF;
                uint8_t prog_if    = (reg >> 8)  & 0xFF;

                uart_puts("PCI: ");

                print_hex(bus);
                uart_putc(':');

                print_hex(dev);
                uart_putc(':');

                print_hex(func);

                uart_puts(" vendor=");
                print_hex(vendor);

                uart_puts(" device=");
                print_hex(device);

                uart_puts(" class=");
                print_hex(class_code);

                uart_puts(" subclass=");
                print_hex(subclass);

                uart_putc('\n');
            }
        }
    }
}
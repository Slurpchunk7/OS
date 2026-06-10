#include "fat32.h"
#include "../virtio_blk.h"
#include "../print.h"
#include "../memory/heap.h"
#include "../utils/mem.h"
#include <stddef.h>

// BPB offsets
#define BPB_BYTES_PER_SECTOR    11
#define BPB_SECTORS_PER_CLUSTER 13
#define BPB_RESERVED_SECTORS    14
#define BPB_NUM_FATS            16
#define BPB_TOTAL_SECTORS_32    32
#define BPB_FAT_SIZE_32         36
#define BPB_ROOT_CLUSTER        44

#define ATTR_READ_ONLY  0x01
#define ATTR_HIDDEN     0x02
#define ATTR_SYSTEM     0x04
#define ATTR_VOLUME_ID  0x08
#define ATTR_DIRECTORY  0x10
#define ATTR_ARCHIVE    0x20
#define ATTR_LFN        0x0F

#define EOC 0x0FFFFFF8  // end of chain

typedef struct {
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint32_t fat_size;       // in sectors
    uint32_t root_cluster;
    uint32_t fat_lba;        // LBA of FAT
    uint32_t data_lba;       // LBA of data region
} fat32_t;

typedef struct {
    uint8_t  name[11];
    uint8_t  attr;
    uint8_t  reserved;
    uint8_t  crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t cluster_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t cluster_lo;
    uint32_t file_size;
} __attribute__((packed)) dir_entry_t;

typedef struct {
    uint8_t  order;
    uint16_t name1[5];
    uint8_t  attr;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t cluster;
    uint16_t name3[2];
} __attribute__((packed)) lfn_entry_t;

static fat32_t fat;
static uint8_t sector_buf[512];

static uint32_t cluster_to_lba(uint32_t cluster) {
    return fat.data_lba + (cluster - 2) * fat.sectors_per_cluster;
}

static uint32_t next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat.fat_lba + fat_offset / 512;
    uint32_t fat_index  = (fat_offset % 512) / 4;

    virtio_blk_read(fat_sector, 1, sector_buf);
    uint32_t val = ((uint32_t*)sector_buf)[fat_index] & 0x0FFFFFFF;
    return val;
}

static void parse_83_name(uint8_t* raw, char* out) {
    int i = 0, j = 0;
    // name (8 chars)
    for (i = 0; i < 8 && raw[i] != ' '; i++)
        out[j++] = raw[i];
    // extension (3 chars)
    if (raw[8] != ' ') {
        out[j++] = '.';
        for (i = 8; i < 11 && raw[i] != ' '; i++)
            out[j++] = raw[i];
    }
    out[j] = '\0';
}

static void lfn_get_chars(lfn_entry_t* lfn, uint16_t* out) {
    for (int i = 0; i < 5; i++) out[i]    = lfn->name1[i];
    for (int i = 0; i < 6; i++) out[5+i]  = lfn->name2[i];
    for (int i = 0; i < 2; i++) out[11+i] = lfn->name3[i];
}

bool fat32_init() {
    if (!virtio_blk_read(0, 1, sector_buf)) {
        uart_puts("fat32: failed to read boot sector\n");
        return false;
    }

    fat.bytes_per_sector    = *(uint16_t*)(sector_buf + BPB_BYTES_PER_SECTOR);
    fat.sectors_per_cluster = sector_buf[BPB_SECTORS_PER_CLUSTER];
    fat.reserved_sectors    = *(uint16_t*)(sector_buf + BPB_RESERVED_SECTORS);
    fat.num_fats            = sector_buf[BPB_NUM_FATS];
    fat.fat_size            = *(uint32_t*)(sector_buf + BPB_FAT_SIZE_32);
    fat.root_cluster        = *(uint32_t*)(sector_buf + BPB_ROOT_CLUSTER);

    fat.fat_lba  = fat.reserved_sectors;
    fat.data_lba = fat.fat_lba + fat.num_fats * fat.fat_size;

    uart_puts("fat32: init\n");
    uart_puts("  bytes_per_sector=");    print_int(fat.bytes_per_sector);  uart_putc('\n');
    uart_puts("  sectors_per_cluster="); print_int(fat.sectors_per_cluster); uart_putc('\n');
    uart_puts("  root_cluster=");        print_int(fat.root_cluster);      uart_putc('\n');
    uart_puts("  fat_lba=");             print_int(fat.fat_lba);           uart_putc('\n');
    uart_puts("  data_lba=");            print_int(fat.data_lba);          uart_putc('\n');

    return true;
}

// read all dir entries from a cluster chain into entries[]
static int read_dir(uint32_t cluster, fat_entry_t* entries, int max) {
    int count = 0;
    uint16_t lfn_buf[256];
    int      lfn_len = 0;
    bool     has_lfn = false;

    while (cluster < EOC && count < max) {
        uint32_t lba = cluster_to_lba(cluster);

        for (int s = 0; s < fat.sectors_per_cluster && count < max; s++) {
            virtio_blk_read(lba + s, 1, sector_buf);
            dir_entry_t* dirs = (dir_entry_t*)sector_buf;

            for (int i = 0; i < 512 / 32; i++) {
                dir_entry_t* d = &dirs[i];

                if (d->name[0] == 0x00) goto done;  // no more entries
                if ((uint8_t)d->name[0] == 0xE5) continue;  // deleted

                if (d->attr == ATTR_LFN) {
                    lfn_entry_t* lfn = (lfn_entry_t*)d;
                    int seq = (lfn->order & 0x1F) - 1;
                    uint16_t chars[13];
                    lfn_get_chars(lfn, chars);
                    for (int c = 0; c < 13; c++) {
                        int pos = seq * 13 + c;
                        if (pos < 255) lfn_buf[pos] = chars[c];
                    }
                    has_lfn = true;
                    continue;
                }

                if (d->attr & ATTR_VOLUME_ID) { has_lfn = false; lfn_len = 0; continue; }

                fat_entry_t* e = &entries[count++];
                e->size    = d->file_size;
                e->is_dir  = (d->attr & ATTR_DIRECTORY) != 0;
                e->cluster = ((uint32_t)d->cluster_hi << 16) | d->cluster_lo;

                if (has_lfn) {
                    // convert LFN UTF-16 to ASCII
                    int j = 0;
                    for (j = 0; j < 255; j++) {
                        if (lfn_buf[j] == 0x0000 || lfn_buf[j] == 0xFFFF) break;
                        e->name[j] = (char)(lfn_buf[j] & 0x7F);
                    }
                    e->name[j] = '\0';
                    has_lfn = false;
                    lfn_len = 0;
                } else {
                    parse_83_name(d->name, e->name);
                }
            }
        }
        cluster = next_cluster(cluster);
    }
done:
    return count;
}

bool fat32_list(const char* path, fat_entry_t* entries, int max, int* count) {
    // for now only root dir supported, path ignored
    *count = read_dir(fat.root_cluster, entries, max);
    return true;
}

bool fat32_open(const char* path, fat_entry_t* out) {
    fat_entry_t entries[64];
    int count = 0;
    fat32_list(path, entries, 64, &count);

    // simple name match, skip leading slash
    const char* name = path;
    if (*name == '/') name++;

    for (int i = 0; i < count; i++) {
        // case-insensitive compare
        const char* a = entries[i].name;
        const char* b = name;
        bool match = true;
        while (*a && *b) {
            char ca = *a >= 'a' && *a <= 'z' ? *a - 32 : *a;
            char cb = *b >= 'a' && *b <= 'z' ? *b - 32 : *b;
            if (ca != cb) { match = false; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') {
            *out = entries[i];
            return true;
        }
    }
    return false;
}

bool fat32_read(fat_entry_t* entry, void* buf) {
    uint8_t* dst     = (uint8_t*)buf;
    uint32_t cluster = entry->cluster;
    uint32_t remain  = entry->size;

    while (cluster < EOC && remain > 0) {
        uint32_t lba = cluster_to_lba(cluster);
        for (int s = 0; s < fat.sectors_per_cluster && remain > 0; s++) {
            virtio_blk_read(lba + s, 1, sector_buf);
            uint32_t bytes = remain < 512 ? remain : 512;
            for (uint32_t i = 0; i < bytes; i++) dst[i] = sector_buf[i];
            dst    += bytes;
            remain -= bytes;
        }
        cluster = next_cluster(cluster);
    }
    return true;
}
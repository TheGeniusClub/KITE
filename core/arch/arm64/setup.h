/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch setup.h and preset.h
 */

#ifndef _KITE_SETUP_H
#define _KITE_SETUP_H

#ifndef __ASSEMBLY__
#include <kite/ktypes.h>
#endif

#define KITE_MAGIC "KITEv6"
#define KITE_MAGIC_LEN 6
#define KITE_HEADER_SIZE 0x40
#define SUPER_KEY_LEN 0x20
#define MAP_ALIGN 0x10
#define MAP_SYMBOL_NUM 7
#define MAP_SYMBOL_SIZE (MAP_SYMBOL_NUM * 8)

#define MAP_SYM_MEMBLOCK_PHYS_ALLOC_TRY_NID 1
#define MAP_SYM_MEMBLOCK_ALLOC_TRY_NID 2
#define MAP_SYM_MEMBLOCK_VIRT_ALLOC_TRY_NID 1
#define MAP_SYM_MEMBLOCK_VIRT_ALLOC_FROM_ALLOC_TRY_NID 2
#define MAP_MAX_SIZE 0xa00
#define HOOK_ALLOC_SIZE (1 << 20)
#define MEMORY_ROX_SIZE (4 << 20)
#define MEMORY_RW_SIZE (2 << 20)

#ifndef __ASSEMBLY__

typedef struct
{
    union
    {
        struct
        {
            uint64_t memblock_reserve_relo;
            uint64_t memblock_free_relo;
            uint64_t memblock_phys_alloc_relo;
            uint64_t memblock_virt_alloc_relo;
            uint64_t memblock_mark_nomap_relo;
            uint64_t memblock_phys_alloc_type;
            uint64_t memblock_virt_alloc_type;
        };
        char _cap[MAP_SYMBOL_SIZE];
    };
} map_symbol_t;

typedef struct
{
    uint32_t kernel_version;
    uint32_t _;
    uint64_t kimg_size;
    uint64_t kiteimg_size;
    uint64_t kernel_size;
    uint64_t page_shift;
    uint64_t setup_offset;
    uint64_t start_offset;
    uint64_t extra_size;
    uint64_t map_offset;
    uint64_t map_max_size;
    uint64_t kallsyms_lookup_name_offset;
    uint64_t paging_init_offset;
    uint64_t printk_offset;
    map_symbol_t map_symbol;
    uint8_t header_backup[8];
    uint8_t superkey[SUPER_KEY_LEN];
} setup_preset_t;

typedef struct
{
    uint32_t paging_init_backup;
    uint32_t __;
    int64_t map_offset;
    int64_t start_offset;
    int64_t start_size;
    int64_t start_img_size;
    int64_t extra_size;
    int64_t alloc_size;
    uint64_t kernel_pa;
    uint64_t paging_init_relo;
    map_symbol_t map_symbol;
    uint64_t printk_relo;
    int64_t va1_bits;
    int64_t page_shift;
    uint64_t kimage_voffset;
    uint64_t linear_voffset;
} map_data_t;

typedef int (*start_f)(uint64_t kimage_voffset, uint64_t linear_voffset, uint64_t alloc_pa, uint64_t alloc_size);

typedef struct
{
    uint32_t kernel_version;
    uint32_t _;
    uint64_t kallsyms_lookup_name_offset;
    uint64_t kernel_size;
    uint64_t start_offset;
    uint64_t extra_size;
    uint64_t kernel_pa;
    uint64_t map_offset;
    uint64_t map_backup_len;
    uint8_t map_backup[MAP_MAX_SIZE];
    uint8_t superkey[SUPER_KEY_LEN];
    uint64_t printk_offset;
} start_preset_t;

extern void _start_kernel();
extern void _paging_init();
extern void _link_base();
extern void _link_end();
extern void _setup_start();
extern void _setup_end();
extern void _map_start();
extern void _map_text();
extern void _map_data();
extern void _map_end();
extern void _kp_end();

#else

// assembly offsets for map_data_t
#define map_paging_init_backup_offset 0
#define map_map_offset_offset (map_paging_init_backup_offset + 8)
#define map_start_offset_offset (map_map_offset_offset + 8)
#define map_start_size_offset (map_start_offset_offset + 8)
#define map_start_img_size_offset (map_start_size_offset + 8)
#define map_extra_size_offset (map_start_img_size_offset + 8)
#define map_alloc_size_offset (map_extra_size_offset + 8)
#define map_kernel_pa_offset (map_alloc_size_offset + 8)
#define map_paging_init_relo_offset (map_kernel_pa_offset + 8)
#define map_map_symbol_offset (map_paging_init_relo_offset + 8)
#define map_printk_relo_offset (map_map_symbol_offset + MAP_SYMBOL_SIZE)
#define map_va1_bits_offset (map_printk_relo_offset + 8)
#define map_page_shift_offset (map_va1_bits_offset + 8)
#define map_kimage_voffset_offset (map_page_shift_offset + 8)
#define map_linear_voffset_offset (map_kimage_voffset_offset + 8)

// assembly offsets for setup_preset_t
#define setup_kernel_version_offset 0
#define setup_kimg_size_offset (setup_kernel_version_offset + 8)
#define setup_kiteimg_size_offset (setup_kimg_size_offset + 8)
#define setup_kernel_size_offset (setup_kiteimg_size_offset + 8)
#define setup_page_shift_offset (setup_kernel_size_offset + 8)
#define setup_setup_offset_offset (setup_page_shift_offset + 8)
#define setup_start_offset_offset (setup_setup_offset_offset + 8)
#define setup_extra_size_offset (setup_start_offset_offset + 8)
#define setup_map_offset_offset (setup_extra_size_offset + 8)
#define setup_map_max_size_offset (setup_map_offset_offset + 8)
#define setup_kallsyms_lookup_name_offset_offset (setup_map_max_size_offset + 8)
#define setup_paging_init_offset_offset (setup_kallsyms_lookup_name_offset_offset + 8)
#define setup_printk_offset_offset (setup_paging_init_offset_offset + 8)
#define setup_map_symbol_offset (setup_printk_offset_offset + 8)
#define setup_header_backup_offset (setup_map_symbol_offset + MAP_SYMBOL_SIZE)
#define setup_superkey_offset (setup_header_backup_offset + 8)

// assembly offsets for start_preset_t
#define start_kernel_version_offset 0
#define start_kallsyms_lookup_name_offset_offset (start_kernel_version_offset + 8)
#define start_kernel_size_offset (start_kallsyms_lookup_name_offset_offset + 8)
#define start_start_offset_offset (start_kernel_size_offset + 8)
#define start_extra_size_offset (start_start_offset_offset + 8)
#define start_kernel_pa_offset (start_extra_size_offset + 8)
#define start_map_offset_offset (start_kernel_pa_offset + 8)
#define start_map_backup_len_offset (start_map_offset_offset + 8)
#define start_map_backup_offset (start_map_backup_len_offset + 8)
#define start_superkey_offset (start_map_backup_offset + MAP_MAX_SIZE)
#define start_printk_offset_offset (start_superkey_offset + SUPER_KEY_LEN)

#endif // __ASSEMBLY__

#endif // _KITE_SETUP_H

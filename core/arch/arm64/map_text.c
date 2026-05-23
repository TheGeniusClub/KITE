/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Phase 2 map trampoline - called from paging_init
 * Reference from KernelPatch map.c
 */

#include "setup.h"

#define NUMA_NO_NODE (-1)

typedef uint64_t phys_addr_t;
typedef int (*memblock_reserve_f)(phys_addr_t base, phys_addr_t size);
typedef phys_addr_t (*memblock_phys_alloc_try_nid_f)(phys_addr_t size, phys_addr_t align, int nid);
typedef void *(*memblock_virt_alloc_try_nid_f)(phys_addr_t size, phys_addr_t align, phys_addr_t min_addr,
                                               phys_addr_t max_addr, int nid);
typedef int (*memblock_free_f)(phys_addr_t base, phys_addr_t size);
typedef int (*memblock_mark_nomap_f)(phys_addr_t base, phys_addr_t size);
typedef int (*printk_f)(const char *fmt, ...);
typedef void (*paging_init_f)(void);

map_data_t map_data __section(".map.data") __aligned(MAP_ALIGN) = {
};

map_data_t *__noinline get_data()
{
    uint64_t map_data_va;
    asm volatile("adr %0, map_data" : "=r"(map_data_va));
    return (map_data_t *)map_data_va;
}

static uint64_t get_kva()
{
    map_data_t *data = get_data();
    uint64_t kernel_va = (uint64_t)data - data->map_offset;
    return kernel_va;
}

static inline uint64_t phys_to_lm(map_data_t *data, uint64_t phys)
{
    return phys + data->linear_voffset;
}

static void flush_tlb_all()
{
    asm volatile("dsb ishst" : : : "memory");
    asm volatile("tlbi vmalle1is\n"
                 "dsb ish\n"
                 "tlbi vmalle1is\n");
    asm volatile("dsb ish" : : : "memory");
    asm volatile("isb" : : : "memory");
}

static void flush_icache_all(void)
{
    asm volatile("dsb ish" : : : "memory");
    asm volatile("ic ialluis");
    asm volatile("dsb ish" : : : "memory");
    asm volatile("isb" : : : "memory");
}

static uint64_t map_phys_alloc(map_data_t *data, uint64_t size, uint64_t align)
{
    if (data->map_symbol.memblock_phys_alloc_type == MAP_SYM_MEMBLOCK_PHYS_ALLOC_TRY_NID ||
        data->map_symbol.memblock_phys_alloc_type == MAP_SYM_MEMBLOCK_ALLOC_TRY_NID) {
        return ((memblock_phys_alloc_try_nid_f)data->map_symbol.memblock_phys_alloc_relo)(size, align, NUMA_NO_NODE);
    }
    return 0;
}

static void *map_virt_alloc(map_data_t *data, uint64_t size, uint64_t align, phys_addr_t min_addr, phys_addr_t max_addr)
{
    if (data->map_symbol.memblock_virt_alloc_type == MAP_SYM_MEMBLOCK_VIRT_ALLOC_TRY_NID) {
        return ((memblock_virt_alloc_try_nid_f)data->map_symbol.memblock_virt_alloc_relo)(
            size, align, min_addr, max_addr, NUMA_NO_NODE);
    }
    return NULL;
}

static __noinline void mem_copy(map_data_t *dst, map_data_t *src)
{
    uint64_t *d = (uint64_t *)dst;
    uint64_t *s = (uint64_t *)src;
    for (int i = 0; i < sizeof(map_data_t) / 8; i++) {
        d[i] = s[i];
    }
}

static __noinline void mem_proc(map_data_t *data)
{
    mem_copy(data, get_data());
    uint64_t kernel_va = get_kva();

    data->kimage_voffset = kernel_va - data->kernel_pa;
    data->paging_init_relo += kernel_va;

    if (data->map_symbol.memblock_reserve_relo)
        data->map_symbol.memblock_reserve_relo += kernel_va;
    if (data->map_symbol.memblock_free_relo)
        data->map_symbol.memblock_free_relo += kernel_va;
    if (data->map_symbol.memblock_phys_alloc_relo)
        data->map_symbol.memblock_phys_alloc_relo += kernel_va;
    if (data->map_symbol.memblock_virt_alloc_relo)
        data->map_symbol.memblock_virt_alloc_relo += kernel_va;
    if (data->map_symbol.memblock_mark_nomap_relo)
        data->map_symbol.memblock_mark_nomap_relo += kernel_va;
    if (data->printk_relo)
        data->printk_relo += kernel_va;

    uint64_t tcr_el1;
    asm volatile("mrs %0, tcr_el1" : "=r"(tcr_el1));
    uint64_t t1sz = tcr_el1 << 42 >> 58;
    uint64_t va1_bits = 64 - t1sz;
    data->va1_bits = va1_bits;
    uint64_t tg1 = tcr_el1 << 32 >> 62;
    uint64_t page_shift = 12;
    if (tg1 == 1) {
        page_shift = 14;
    } else if (tg1 == 3) {
        page_shift = 16;
    }
    data->page_shift = page_shift;

    if (data->map_symbol.memblock_virt_alloc_relo) {
        uint64_t detect_phys = map_phys_alloc(data, 0, 0x10);
        uint64_t detect_virt = (uint64_t)map_virt_alloc(data, 0, 0x10, detect_phys, detect_phys);
        data->linear_voffset = detect_virt - detect_phys;
    } else {
        __builtin_trap();
    }

    if (data->alloc_size < MEMORY_RW_SIZE + MEMORY_ROX_SIZE) {
        data->alloc_size = MEMORY_RW_SIZE + MEMORY_ROX_SIZE;
    }
}

static uint64_t __noinline get_or_create_pte(map_data_t *data, uint64_t va, uint64_t pa, uint64_t attr_indx)
{
    uint64_t page_shift = data->page_shift;
    uint64_t va_bits = data->va1_bits;
    uint64_t page_level = (va_bits - 4) / (page_shift - 3);
    uint64_t pxd_bits = page_shift - 3;
    uint64_t pxd_ptrs = 1u << pxd_bits;

    uint64_t ttbr1_el1;
    asm volatile("mrs %0, ttbr1_el1" : "=r"(ttbr1_el1));
    uint64_t baddr = ttbr1_el1 & 0xFFFFFFFFFFFE;
    uint64_t page_size = 1 << page_shift;
    uint64_t page_size_mask = ~(page_size - 1);
    uint64_t attr_prot = 0x40000000000703 | attr_indx;

    uint64_t pxd_pa = baddr & page_size_mask;
    uint64_t pxd_va = phys_to_lm(data, pxd_pa);
    uint64_t pxd_entry_va = 0;

    for (uint64_t lv = 4 - page_level; lv < 4; lv++) {
        uint64_t pxd_shift = (page_shift - 3) * (4 - lv) + 3;
        uint64_t pxd_index = (va >> pxd_shift) & (pxd_ptrs - 1);
        uint64_t alloc_flag = 0;
        uint64_t block_flag = 0;

        pxd_entry_va = pxd_va + pxd_index * 8;

        uint64_t pxd_desc = *((uint64_t *)pxd_entry_va);

        if ((pxd_desc & 0b11) == 0b11) {
            pxd_pa = pxd_desc & (((1ul << (48 - page_shift)) - 1) << page_shift);
        } else if ((pxd_desc & 0b11) == 0b01) {
            uint64_t block_bits = (3 - lv) * pxd_bits + page_shift;
            pxd_pa = pxd_desc & (((1ul << (48 - block_bits)) - 1) << block_bits);
            block_flag = 1;
        } else {
            if (lv != 3) {
                pxd_pa = map_phys_alloc(data, page_size, page_size);
                alloc_flag = 1;
            } else {
                pxd_pa = pa;
            }
            pxd_desc = (pxd_pa) | attr_prot;
            *((uint64_t *)pxd_entry_va) = pxd_desc;
        }
        pxd_va = phys_to_lm(data, pxd_pa);
        if (alloc_flag) {
            for (uint64_t i = pxd_va; i < pxd_va + page_size; i += 8) {
                *(uint64_t *)i = 0;
            }
        }
        if (block_flag) {
            break;
        }
    }
    return pxd_entry_va;
}

void __noinline _paging_init()
{
    map_data_t buf;
    map_data_t *data = &buf;
    mem_proc(data);

    uint64_t page_size = 1 << data->page_shift;
    uint64_t old_start_pa = data->start_offset + data->kernel_pa;
    uint64_t reserve_size = data->start_img_size + data->extra_size;
    uint64_t align_extra_size = (data->extra_size + page_size - 1) & ~(page_size - 1);
    uint64_t all_size = data->start_size + align_extra_size + data->alloc_size;

    ((memblock_reserve_f)data->map_symbol.memblock_reserve_relo)(old_start_pa, reserve_size);
    uint64_t start_pa = map_phys_alloc(data, all_size, page_size);
    if (data->map_symbol.memblock_mark_nomap_relo)
        ((memblock_mark_nomap_f)(data->map_symbol.memblock_mark_nomap_relo))(start_pa, all_size);

    uint64_t paging_init_va = data->paging_init_relo;
    *(uint32_t *)(paging_init_va) = data->paging_init_backup;
    flush_icache_all();
    ((paging_init_f)(paging_init_va))();

    uint64_t ktext_pte = get_or_create_pte(data, data->paging_init_relo, 0, 0);
    uint64_t attrs = *(uint64_t *)ktext_pte;
    uint64_t attr_indx = attrs & 0b11100;

    uint64_t sctlr_el1 = 0;
    asm volatile("mrs %[reg], sctlr_el1" : [reg] "+r"(sctlr_el1));
    sctlr_el1 &= 0xFFFFFFFFFFF7FFFF;
    asm volatile("msr sctlr_el1, %[reg]" : : [reg] "r"(sctlr_el1));

    uint64_t old_start_va = phys_to_lm(data, old_start_pa);
    uint64_t start_va = start_pa + data->kimage_voffset;

    for (uint64_t off = 0; off < all_size; off += page_size) {
        uint64_t entry = get_or_create_pte(data, start_va + off, start_pa + off, attr_indx);
        *(uint64_t *)entry = (*(uint64_t *)entry | 0x8000000000000) & 0xFFDFFFFFFFFFFF7F;
    }
    flush_tlb_all();

    for (uint64_t i = start_va; i < start_va + all_size; i += 8) {
        *(uint64_t *)i = 0;
    }
    for (uint64_t i = 0; i < data->start_img_size; i += 8) {
        *(uint64_t *)(start_va + i) = *(uint64_t *)(old_start_va + i);
    }
    for (uint64_t i = 0; i < data->extra_size; i += 8) {
        *(uint64_t *)(start_va + data->start_size + i) = *(uint64_t *)(old_start_va + data->start_img_size + i);
    }

    flush_icache_all();

    ((memblock_free_f)data->map_symbol.memblock_free_relo)(old_start_pa, reserve_size);

    uint64_t alloc_pa = start_pa + data->start_size + align_extra_size;
    ((start_f)start_va)(data->kimage_voffset, data->linear_voffset, alloc_pa, data->alloc_size);
}

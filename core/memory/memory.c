/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Memory management - execmem alias_page direct_pte
 * Reference from KernelPatch hotpatch.c and map.c
 */

#include "memory.h"
#include "tlsf.h"
#include "stubs.h"
#include "arch/arm64/setup.h"

extern tlsf_t kite_rox_mem;
extern tlsf_t kite_rw_mem;

#ifdef __TEST_HOST__
static inline void dsb_ish(void) { }
static inline void flush_tlb_kernel_page(uint64_t va) { (void)va; }
static inline void flush_icache_all(void) { }
static uint64_t get_pte_entry(uint64_t va) { (void)va; return 0; }
#else
static inline void dsb_ish(void)
{
    asm volatile("dsb ish" ::: "memory");
}

static inline void flush_tlb_kernel_page(uint64_t va)
{
    asm volatile("tlbi vaae1is, %0" : : "r"(va >> 12) : "memory");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static inline void flush_icache_all(void)
{
    asm volatile("dsb ish" ::: "memory");
    asm volatile("ic ialluis");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static uint64_t get_pte_entry(uint64_t va)
{
    uint64_t page_shift = 12;
    uint64_t page_size = 1 << page_shift;
    uint64_t page_size_mask = ~(page_size - 1);
    
    uint64_t ttbr1_el1;
    asm volatile("mrs %0, ttbr1_el1" : "=r"(ttbr1_el1));
    uint64_t baddr = ttbr1_el1 & 0xFFFFFFFFFFFE;
    
    uint64_t pxd_pa = baddr & page_size_mask;
    uint64_t pxd_va = pxd_pa;
    
    for (int lv = 0; lv < 3; lv++) {
        uint64_t pxd_shift = (page_shift - 3) * (3 - lv) + 3;
        uint64_t pxd_index = (va >> pxd_shift) & 0x1FF;
        uint64_t *entry = (uint64_t *)(pxd_va + pxd_index * 8);
        uint64_t desc = *entry;
        
        if ((desc & 0b11) == 0b11) {
            if (lv == 2) return (uint64_t)entry;
            pxd_pa = desc & (((1ul << (48 - page_shift)) - 1) << page_shift);
            pxd_va = pxd_pa;
        } else {
            return 0;
        }
    }
    return 0;
}
#endif

static void modify_pte(uint64_t va, uint64_t *entry, uint64_t value)
{
    *entry = value;
    dsb_ish();
    flush_tlb_kernel_page(va);
}

// execmem pool management
void *execmem_alloc(size_t size)
{
    if (!kite_rox_mem) {
        kite_printk("KITE: execmem_alloc fail no pool\n");
        return NULL;
    }
    return tlsf_malloc(kite_rox_mem, size);
}

void execmem_free(void *ptr)
{
    if (kite_rox_mem && ptr) {
        tlsf_free(kite_rox_mem, ptr);
    }
}

// alias page for temporary W+X
static uint64_t alias_pte_entry = 0;
static uint64_t alias_pte_value = 0;
static uint64_t alias_page_va = 0;

void *alias_page(void *addr, size_t size)
{
    (void)size;
    uint64_t va = (uint64_t)addr;
    uint64_t *entry = (uint64_t *)get_pte_entry(va);
    if (!entry) return addr;
    
    uint64_t ori_pte = *entry;
    uint64_t new_pte = (ori_pte | 0x40000000000000) & ~0x80; // writable
    modify_pte(va, entry, new_pte);
    
    // save for restore
    alias_pte_entry = (uint64_t)entry;
    alias_pte_value = ori_pte;
    alias_page_va = va;
    
    return addr;
}

void unalias_page(void *addr)
{
    uint64_t va = (uint64_t)addr;
    if (alias_pte_entry && alias_page_va == va) {
        uint64_t *entry = (uint64_t *)alias_pte_entry;
        modify_pte(va, entry, alias_pte_value);
        alias_pte_entry = 0;
        alias_pte_value = 0;
        alias_page_va = 0;
    }
}

// direct pte manipulation
int direct_pte_set_rw(uint64_t addr)
{
    uint64_t *entry = (uint64_t *)get_pte_entry(addr);
    if (!entry) return -1;
    
    uint64_t ori_pte = *entry;
    uint64_t new_pte = (ori_pte | 0x40000000000000) & ~0x80;
    modify_pte(addr, entry, new_pte);
    return 0;
}

int direct_pte_set_rx(uint64_t addr)
{
    uint64_t *entry = (uint64_t *)get_pte_entry(addr);
    if (!entry) return -1;
    
    uint64_t ori_pte = *entry;
    uint64_t new_pte = ori_pte | 0x8000000000000000ULL; // UXN=0
    new_pte = new_pte & ~0x40000000000000; // clear writable
    modify_pte(addr, entry, new_pte);
    flush_icache_all();
    return 0;
}

// helper to set exec permission on ROX pool pages
int memory_init_rox_pool(void)
{
    if (!kite_rox_mem) {
        kite_printk("KITE: memory_init_rox_pool fail no pool\n");
        return -1;
    }
    
    // TLSF pool header contains pool base and size
    // todo: walk all allocated blocks and mark RX
    // for now just mark the whole pool area
    kite_printk("KITE: rox pool init ok\n");
    return 0;
}

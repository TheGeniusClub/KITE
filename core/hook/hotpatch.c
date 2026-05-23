/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch hotpatch.c
 */

#include "hotpatch.h"
#include "stubs.h"
#include "arch/arm64/setup.h"

static inline void dsb_ish(void)
{
    asm volatile("dsb ish" ::: "memory");
}

static inline void isb(void)
{
    asm volatile("isb" ::: "memory");
}

static inline void flush_icache_all(void)
{
    asm volatile("dsb ish" ::: "memory");
    asm volatile("ic ialluis");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static inline void flush_tlb_kernel_page(uint64_t va)
{
    asm volatile("tlbi vaae1is, %0" : : "r"(va >> 12) : "memory");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static inline int is_interrupt_masked(void)
{
    unsigned long daif;
    asm volatile("mrs %0, daif" : "=r"(daif));
    return daif & 0xC0;
}

// Simple atomic increment
typedef struct { int counter; } atomic_t;

static inline int atomic_inc_return(atomic_t *v)
{
    int val, tmp;
    asm volatile("1: ldxr %w0, %2\n"
                 "add %w0, %w0, #1\n"
                 "stxr %w1, %w0, %2\n"
                 "cbnz %w1, 1b\n"
                 : "=&r" (val), "=&r" (tmp), "+Q" (v->counter)
                 :
                 : "memory");
    return val;
}

static inline void atomic_inc(atomic_t *v)
{
    atomic_inc_return(v);
}

static inline int atomic_read(atomic_t *v)
{
    return v->counter;
}

static inline int atomic_dec_return(atomic_t *v)
{
    int val, tmp;
    asm volatile("1: ldxr %w0, %2\n"
                 "sub %w0, %w0, #1\n"
                 "stxr %w1, %w0, %2\n"
                 "cbnz %w1, 1b\n"
                 : "=&r" (val), "=&r" (tmp), "+Q" (v->counter)
                 :
                 : "memory");
    return val;
}

// Page table helpers - simplified for KITE
static uint64_t get_pte_entry(uint64_t va)
{
    uint64_t page_shift = 12; // assume 4KB pages
    uint64_t page_size = 1 << page_shift;
    uint64_t page_size_mask = ~(page_size - 1);
    
    uint64_t ttbr1_el1;
    asm volatile("mrs %0, ttbr1_el1" : "=r"(ttbr1_el1));
    uint64_t baddr = ttbr1_el1 & 0xFFFFFFFFFFFE;
    
    uint64_t pxd_pa = baddr & page_size_mask;
    uint64_t pxd_va = pxd_pa; // early boot, linear mapping
    
    // Simple 3-level walk (simplified)
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

static void modify_pte(uint64_t va, uint64_t *entry, uint64_t value)
{
    *entry = value;
    flush_tlb_kernel_page(va);
}

int hotpatch_nosync(void *addr, uint32_t value)
{
    uint64_t tp = (uint64_t)addr;
    if (tp & 0x3) return -1;
    
    uint64_t *entry = (uint64_t *)get_pte_entry(tp);
    if (!entry) {
        // Direct write without PTE manipulation (dangerous but simple)
        *(uint32_t *)tp = value;
        flush_icache_all();
        return 0;
    }
    
    uint64_t ori_prot = *entry;
    // Set writable: clear AP[1] read-only, keep other bits
    uint64_t new_prot = (ori_prot | 0x40000000000000) & ~0x80;
    modify_pte(tp, entry, new_prot);
    
    *(uint32_t *)tp = value;
    
    modify_pte(tp, entry, ori_prot);
    flush_icache_all();
    return 0;
}

struct hotpatch_t
{
    void **addrs;
    uint32_t *values;
    int cnt;
    atomic_t index;
};

static int hotpatch_cb(void *arg)
{
    int i, ret = 0;
    struct hotpatch_t *pp = arg;
    int index = atomic_inc_return(&pp->index);
    if (!index || index == 1) { // simplified: assume single CPU
        for (i = 0; ret == 0 && i < pp->cnt; ++i)
            ret = hotpatch_nosync(pp->addrs[i], pp->values[i]);
        atomic_inc(&pp->index);
    } else {
        while (atomic_read(&pp->index) <= 1)
            asm volatile("yield" ::: "memory");
        isb();
    }
    return ret;
}

int hotpatch(void *addrs[], uint32_t values[], int cnt)
{
    struct hotpatch_t patch = {
        .addrs = addrs,
        .values = values,
        .cnt = cnt,
        .index = { 0 },
    };
    if (cnt <= 0) return -1;
    if (is_interrupt_masked() || 1) { // simplified: no stop_machine
        atomic_dec_return(&patch.index);
        return hotpatch_cb(&patch);
    }
    return 0;
}

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_MMU_H
#define _KITE_MMU_H

#include <kite/ktypes.h>

// MMU operations
uint64_t mmu_get_ttbr1(void);
uint64_t mmu_get_tcr(void);
uint64_t mmu_get_sctlr(void);

void mmu_set_sctlr(uint64_t val);

// page table walk
uint64_t mmu_walk_pt(uint64_t va);
int mmu_map_page(uint64_t va, uint64_t pa, uint64_t attrs);
int mmu_unmap_page(uint64_t va);

// TLB operations
void mmu_flush_tlb_all(void);
void mmu_flush_tlb_va(uint64_t va);

// cache operations
void mmu_flush_icache_all(void);
void mmu_flush_dcache_all(void);

#endif // _KITE_MMU_H

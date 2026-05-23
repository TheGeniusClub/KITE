/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch fphook.c
 */

#include "hook_engine.h"
#include "stubs.h"
#include "memory/memory.h"
#include "memory/tlsf.h"

static inline void dsb_ish(void)
{
    asm volatile("dsb ish" ::: "memory");
}

static void *fp_hook_mem_zalloc(void)
{
    extern tlsf_t kite_rw_mem;
    if (!kite_rw_mem) return NULL;
    void *mem = tlsf_malloc(kite_rw_mem, sizeof(fp_hook_chain_t));
    if (mem) {
        uint64_t *p = mem;
        for (size_t i = 0; i < sizeof(fp_hook_chain_t) / 8; i++) {
            p[i] = 0;
        }
    }
    return mem;
}

static void fp_hook_mem_free(void *hook)
{
    extern tlsf_t kite_rw_mem;
    if (kite_rw_mem && hook) {
        tlsf_free(kite_rw_mem, hook);
    }
}

void fp_hook(uintptr_t fp_addr, void *replace, void **backup)
{
    if (!fp_addr || !replace || !backup) return;
    fp_hook_t *h = (fp_hook_t *)fp_addr;
    *backup = (void *)h->origin_fp;
    h->origin_fp = h->replace_addr;
    h->replace_addr = (uint64_t)replace;
    kite_printk("KITE fp hook addr=0x%llx replace=0x%llx\n", fp_addr, replace);
}

void fp_unhook(uintptr_t fp_addr, void *backup)
{
    if (!fp_addr || !backup) return;
    fp_hook_t *h = (fp_hook_t *)fp_addr;
    h->replace_addr = h->origin_fp;
    h->origin_fp = (uint64_t)backup;
    kite_printk("KITE fp unhook addr=0x%llx\n", fp_addr);
}

static void fp_transit_prepare(uint32_t *transit, int32_t argno)
{
    // Simplified transit for FP hooks - just jump to replace
    transit[0] = ARM64_NOP;
}

hook_err_t fp_hook_wrap(uintptr_t fp_addr, int32_t argno, void *before, void *after, void *udata)
{
    if (!fp_addr) return -HOOK_BAD_ADDRESS;
    fp_hook_chain_t *chain = (fp_hook_chain_t *)fp_addr; // simplified
    if (!chain) {
        chain = (fp_hook_chain_t *)fp_hook_mem_zalloc();
        if (!chain) return -HOOK_NO_MEM;
    }
    
    for (int i = 0; i < FP_HOOK_CHAIN_NUM; i++) {
        if ((before && chain->befores[i] == before) || (after && chain->afters[i] == after))
            return -HOOK_DUPLICATED;
        
        if (chain->states[i] == CHAIN_ITEM_STATE_EMPTY) {
            chain->states[i] = CHAIN_ITEM_STATE_BUSY;
            dsb_ish();
            chain->udata[i] = udata;
            chain->befores[i] = before;
            chain->afters[i] = after;
            if (i + 1 > chain->chain_items_max) {
                chain->chain_items_max = i + 1;
            }
            dsb_ish();
            chain->states[i] = CHAIN_ITEM_STATE_READY;
            kite_printk("KITE fp wrap addr=0x%llx ok\n", fp_addr);
            return HOOK_NO_ERR;
        }
    }
    return -HOOK_CHAIN_FULL;
}

void fp_hook_unwrap(uintptr_t fp_addr, void *before, void *after)
{
    if (!fp_addr) return;
    fp_hook_chain_t *chain = (fp_hook_chain_t *)fp_addr;
    if (!chain) return;
    
    for (int i = 0; i < FP_HOOK_CHAIN_NUM; i++) {
        if (chain->states[i] == CHAIN_ITEM_STATE_READY)
            if ((before && chain->befores[i] == before) || (after && chain->afters[i] == after)) {
                chain->states[i] = CHAIN_ITEM_STATE_BUSY;
                dsb_ish();
                chain->udata[i] = 0;
                chain->befores[i] = 0;
                chain->afters[i] = 0;
                dsb_ish();
                chain->states[i] = CHAIN_ITEM_STATE_EMPTY;
                break;
            }
    }
    
    int empty = 1;
    for (int i = 0; i < FP_HOOK_CHAIN_NUM; i++) {
        if (chain->states[i] != CHAIN_ITEM_STATE_EMPTY) {
            empty = 0;
            break;
        }
    }
    if (empty) {
        fp_hook_mem_free(chain);
    }
    kite_printk("KITE fp unwrap addr=0x%llx\n", fp_addr);
}

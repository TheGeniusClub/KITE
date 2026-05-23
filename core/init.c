/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Framework initialization
 */

#include <kite/ktypes.h>
#include <kite/kite.h>
#include <kite/version.h>

#include "arch/arm64/setup.h"
#include "arch/arm64/start.h"
#include "symbol/symbol_engine.h"
#include "stubs.h"

// preset data
uint8_t kite_header_padding[KITE_HEADER_SIZE] __attribute__((section(".preset.data")));
setup_preset_t setup_preset __attribute__((section(".preset.data")));
start_preset_t start_preset __attribute__((section(".start.data")));
extern map_data_t map_data;
uint8_t header[KITE_HEADER_SIZE] __attribute__((section(".setup.data")));

// memory pools
tlsf_t kite_rw_mem = 0;
tlsf_t kite_rox_mem = 0;

// section markers (defined in linker script)
extern void _link_base();
extern void _link_end();
extern void _setup_start();
extern void _setup_end();
extern void _map_start();
extern void _map_end();
extern void _kp_start();
extern void _kp_end();

// weak refs for optional modules
__attribute__((weak)) extern int kite_symbol_init(void *kernel_base, size_t kernel_size);
__attribute__((weak)) extern int kite_hook_init(void);
__attribute__((weak)) extern int kite_comm_init(void);

// stub tlsf_create if not linked
__attribute__((weak)) tlsf_t tlsf_create(void *mem, size_t bytes)
{
    return 0;
}

int kite_init_framework(void *pool_addr, size_t pool_size,
                        void *execmem_addr, size_t execmem_size,
                        void *kernel_base, size_t kernel_size)
{
    // 1. TLSF memory
    kite_rw_mem = tlsf_create(pool_addr, pool_size);
    if (!kite_rw_mem) return -1;
    
    // 2. Symbol engine
    if (kite_symbol_init) {
        kite_symbol_init(kernel_base, kernel_size);
    }
    
    // 3. Hook engine
    if (kite_hook_init) {
        kite_hook_init();
    }
    
    // 4. Comm
    if (kite_comm_init) {
        kite_comm_init();
    }
    
    return 0;
}

// called from map_trampoline after paging_init
int kite_late_init(void *preset)
{
    // TODO: full framework init
    return 0;
}

// Stage 2 entry - called from _paging_init after memory relocation
int __attribute__((section(".start.text"))) __noinline start(uint64_t kimage_voffset, uint64_t linear_voffset,
                                                              uint64_t alloc_pa, uint64_t alloc_size)
{
    uint64_t kernel_va = kimage_voffset + start_preset.kernel_pa;
    uint64_t printk_addr = kernel_va + start_preset.printk_offset;
    
    kite_printk_init(printk_addr);
    kite_printk("KITE: Stage 2 loaded, kimage_voffset=0x%llx alloc=0x%llx+0x%llx\n",
                kimage_voffset, alloc_pa, alloc_size);
    
    // init memory pools
    if (alloc_size >= MEMORY_RW_SIZE + MEMORY_ROX_SIZE) {
        uint64_t pool_va = alloc_pa + kimage_voffset;
        
        kite_rw_mem = tlsf_create((void *)pool_va, MEMORY_RW_SIZE);
        if (kite_rw_mem) {
            kite_printk("KITE: RW pool ok size=0x%x\n", MEMORY_RW_SIZE);
        } else {
            kite_printk("KITE: RW pool fail\n");
        }
        
        kite_rox_mem = tlsf_create((void *)(pool_va + MEMORY_RW_SIZE), MEMORY_ROX_SIZE);
        if (kite_rox_mem) {
            kite_printk("KITE: ROX pool ok size=0x%x\n", MEMORY_ROX_SIZE);
        } else {
            kite_printk("KITE: ROX pool fail\n");
        }
    } else {
        kite_printk("KITE: alloc_size too small 0x%llx < 0x%x\n",
                    alloc_size, MEMORY_RW_SIZE + MEMORY_ROX_SIZE);
    }
    
    // init symbol engine
    if (kite_symbol_init) {
        int ret = kite_symbol_init((void *)kernel_va, start_preset.kernel_size);
        if (ret == 0) {
            uint64_t printk_lookup = symbol_lookup_name("printk");
            kite_printk("KITE: symbol test printk=0x%llx expect=0x%llx\n",
                        printk_lookup, printk_addr);
        } else {
            kite_printk("KITE: symbol init fail\n");
        }
    }
    
    return 0;
}

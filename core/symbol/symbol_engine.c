/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch kallsyms usage
 */

#include <kite/ktypes.h>
#include <kite/kite.h>

#include "arch/arm64/setup.h"

static unsigned long (*kallsyms_lookup_name)(const char *name) = 0;

int kite_symbol_init(void *kernel_base, size_t kernel_size)
{
    uint64_t kernel_va = (uint64_t)kernel_base;
    
    if (!start_preset.kallsyms_lookup_name_offset) {
        return -1;
    }
    
    kallsyms_lookup_name = (typeof(kallsyms_lookup_name))(kernel_va + start_preset.kallsyms_lookup_name_offset);
    
    if (!kallsyms_lookup_name) {
        return -1;
    }
    
    return 0;
}

uint64_t symbol_lookup_name(const char *name)
{
    if (!kallsyms_lookup_name) {
        return 0;
    }
    return kallsyms_lookup_name(name);
}

uint64_t symbol_lookup_pattern(const char *pattern, size_t len)
{
    return 0;
}

int symbol_foreach(int (*cb)(const char *name, uint64_t addr, void *data), void *data)
{
    return 0;
}

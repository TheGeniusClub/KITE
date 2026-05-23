/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_MEMORY_H
#define _KITE_MEMORY_H

#include <kite/ktypes.h>

// execmem pool for dynamic code generation
void *execmem_alloc(size_t size);
void execmem_free(void *ptr);

// alias page for temporary W+X
void *alias_page(void *addr, size_t size);
void unalias_page(void *addr);

// direct pte manipulation
int direct_pte_set_rw(uint64_t addr);
int direct_pte_set_rx(uint64_t addr);

#endif // _KITE_MEMORY_H

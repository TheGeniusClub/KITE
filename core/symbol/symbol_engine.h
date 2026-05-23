/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_SYMBOL_ENGINE_H
#define _KITE_SYMBOL_ENGINE_H

#include <kite/ktypes.h>

// symbol engine for kernel symbol resolution
int symbol_engine_init(void);

// lookup by name
uint64_t symbol_lookup_name(const char *name);

// lookup by pattern (for stripped kernels)
uint64_t symbol_lookup_pattern(const char *pattern, size_t len);

// iterate all symbols
int symbol_foreach(int (*cb)(const char *name, uint64_t addr, void *data), void *data);

#endif // _KITE_SYMBOL_ENGINE_H

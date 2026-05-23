/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_LOADER_H
#define _KITE_LOADER_H

#include <kite/ktypes.h>

// ELF loader for modules
int loader_init(void);
void *loader_load_elf(const char *path, size_t *size);
void *loader_load_mem(void *mem, size_t size);
int loader_relocate(void *base, size_t size);
int loader_resolve_syms(void *base);

#endif // _KITE_LOADER_H

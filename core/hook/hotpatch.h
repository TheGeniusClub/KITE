/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_HOTPATCH_H
#define _KITE_HOTPATCH_H

#include <kite/ktypes.h>

// hotpatch - atomic multi-instruction replacement
int hotpatch_init(void);
int hotpatch_apply(void *addr, void *new_code, size_t len);
int hotpatch_revert(void *addr);

// patch methods
#define PATCH_METHOD_TEXTPoke 0
#define PATCH_METHOD_ALIAS 1
#define PATCH_METHOD_DIRECT_PTE 2
#define PATCH_METHOD_CPU_REG 3

int hotpatch_set_method(int method);

#endif // _KITE_HOTPATCH_H

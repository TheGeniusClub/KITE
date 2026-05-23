/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_TRAMPOLINE_H
#define _KITE_TRAMPOLINE_H

#include <kite/ktypes.h>

// trampoline generation
typedef enum {
    TRAMPOLINE_STATIC,
    TRAMPOLINE_DYNAMIC
} trampoline_type_t;

void *trampoline_create(void *target, void *replace, trampoline_type_t type);
void trampoline_destroy(void *trampoline);

// PAC-aware trampoline
void *trampoline_create_pac(void *target, void *replace);

#endif // _KITE_TRAMPOLINE_H

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_KALLSYMS_H
#define _KITE_KALLSYMS_H

#include <kite/ktypes.h>

#define KALLSYMS_MAX_NAME 512

typedef struct {
    uint64_t addr;
    char name[KALLSYMS_MAX_NAME];
} kallsym_t;

int kallsyms_init(void);
uint64_t kallsyms_lookup_name(const char *name);
int kallsyms_on_each_symbol(int (*fn)(void *data, const char *name, struct module *mod, unsigned long addr), void *data);

#endif // _KITE_KALLSYMS_H

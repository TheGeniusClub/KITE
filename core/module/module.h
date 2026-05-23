/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_MODULE_H
#define _KITE_MODULE_H

#include <kite/ktypes.h>

#define KITE_MODULE_NAME_LEN 64
#define KITE_MODULE_VERSION_LEN 32

typedef struct kite_module {
    char name[KITE_MODULE_NAME_LEN];
    char version[KITE_MODULE_VERSION_LEN];
    int (*init)(void);
    void (*exit)(void);
    int refcnt;
    struct kite_module *next;
    struct kite_module *deps;
} kite_module_t;

int module_load(const char *path);
int module_unload(const char *name);
kite_module_t *module_find(const char *name);
void module_list(void);

#endif // _KITE_MODULE_H

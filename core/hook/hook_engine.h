/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_HOOK_ENGINE_H
#define _KITE_HOOK_ENGINE_H

#include <kite/ktypes.h>

#define HOOK_TYPE_INLINE 0
#define HOOK_TYPE_WRAP 1
#define HOOK_TYPE_FP 2
#define HOOK_TYPE_SYSCALL 3
#define HOOK_TYPE_HOTPATCH 4

typedef struct kite_hook {
    void *target;
    void *replace;
    void *trampoline;
    int type;
    int active;
    struct kite_hook *next;
} kite_hook_t;

int hook_engine_init(void);
int hook_inline(void *target, void *replace, void **trampoline);
int hook_wrap(void *target, void *before, void *after);
int hook_fp(void **fp, void *replace);
int hook_syscall(int nr, void *replace, void **orig);
int hook_hotpatch(void *target, void *replace, size_t size);
void hook_uninstall(kite_hook_t *hook);

#endif // _KITE_HOOK_ENGINE_H

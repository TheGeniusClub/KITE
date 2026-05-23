/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_H
#define _KITE_H

#include <kite/ktypes.h>
#include <kite/version.h>

#define KITE_SUCCESS 0
#define KITE_ERROR -1

typedef int (*kite_hook_fn_t)(void *data);

int kite_init(void);
void kite_exit(void);

// hook api
int kite_hook_inline(void *target, void *replace, void **trampoline);
int kite_hook_wrap(void *target, kite_hook_fn_t before, kite_hook_fn_t after);
int kite_hook_fp(void **fp, void *replace);
int kite_hook_syscall(int nr, void *replace, void **orig);

// memory api
void *kite_alloc(size_t size);
void kite_free(void *ptr);
void *kite_execmem_alloc(size_t size);
void kite_execmem_free(void *ptr);

// symbol api
uint64_t kite_kallsyms_lookup(const char *name);

#endif // _KITE_H

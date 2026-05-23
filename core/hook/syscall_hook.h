/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_SYSCALL_HOOK_H
#define _KITE_SYSCALL_HOOK_H

#include <kite/ktypes.h>

// syscall hook interface
int syscall_hook_init(void);
int syscall_hook_install(int nr, void *replace, void **orig);
int syscall_hook_uninstall(int nr);

// get syscall table
void *syscall_get_table(void);

#endif // _KITE_SYSCALL_HOOK_H

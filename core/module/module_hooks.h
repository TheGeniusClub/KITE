/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_MODULE_HOOKS_H
#define _KITE_MODULE_HOOKS_H

#include <kite/ktypes.h>

// module lifecycle hooks
typedef int (*module_init_fn_t)(void);
typedef void (*module_exit_fn_t)(void);
typedef int (*module_ctl0_fn_t)(void *data, size_t len);
typedef int (*module_ctl1_fn_t)(void *data, size_t len);

struct module_hooks {
    module_init_fn_t init;
    module_exit_fn_t exit;
    module_ctl0_fn_t ctl0;
    module_ctl1_fn_t ctl1;
};

int module_hooks_register(const char *name, struct module_hooks *hooks);
int module_hooks_unregister(const char *name);

#endif // _KITE_MODULE_HOOKS_H

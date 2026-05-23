/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_MODULE_DEPS_H
#define _KITE_MODULE_DEPS_H

#include <kite/ktypes.h>
#include "module.h"

// module dependency resolution
int module_deps_resolve(kite_module_t *mod);
int module_deps_add(kite_module_t *mod, const char *dep_name);
int module_deps_remove(kite_module_t *mod, const char *dep_name);

#endif // _KITE_MODULE_DEPS_H

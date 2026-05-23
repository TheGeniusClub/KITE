/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_DYNAMIC_DEV_H
#define _KITE_DYNAMIC_DEV_H

#include <kite/ktypes.h>

// dynamic device node
int dynamic_dev_init(void);
int dynamic_dev_create(const char *name);
void dynamic_dev_destroy(void);
const char *dynamic_dev_get_name(void);

#endif // _KITE_DYNAMIC_DEV_H

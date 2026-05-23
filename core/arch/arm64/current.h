/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_CURRENT_H
#define _KITE_CURRENT_H

#include <kite/ktypes.h>

// get current task
void *kite_get_current(void);
u32 kite_get_current_pid(void);
const char *kite_get_current_comm(void);

#endif // _KITE_CURRENT_H

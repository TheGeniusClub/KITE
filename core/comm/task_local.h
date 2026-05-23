/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_TASK_LOCAL_H
#define _KITE_TASK_LOCAL_H

#include <kite/ktypes.h>

// task local storage
int task_local_init(void);
int task_local_set(u32 pid, const char *key, void *value, size_t size);
void *task_local_get(u32 pid, const char *key, size_t *size);
int task_local_del(u32 pid, const char *key);
void task_local_clear(u32 pid);

#endif // _KITE_TASK_LOCAL_H

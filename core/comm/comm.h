/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_COMM_H
#define _KITE_COMM_H

#include <kite/ktypes.h>

// kstorage - kernel key-value storage
int kstorage_set(const char *key, void *value, size_t size);
void *kstorage_get(const char *key, size_t *size);
int kstorage_del(const char *key);

// task local storage
int task_local_set(u32 pid, const char *key, void *value, size_t size);
void *task_local_get(u32 pid, const char *key, size_t *size);
int task_local_del(u32 pid, const char *key);

// challenge auth
int challenge_auth_init(void);
int challenge_auth_verify(u64 challenge, u64 response);

#endif // _KITE_COMM_H

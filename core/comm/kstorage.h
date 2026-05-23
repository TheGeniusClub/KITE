/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_KSTORAGE_H
#define _KITE_KSTORAGE_H

#include <kite/ktypes.h>

// kernel storage - key-value store
int kstorage_init(void);
int kstorage_set(const char *key, const void *value, size_t size);
void *kstorage_get(const char *key, size_t *size);
int kstorage_del(const char *key);
void kstorage_clear(void);

#endif // _KITE_KSTORAGE_H

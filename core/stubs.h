/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_STUBS_H
#define _KITE_STUBS_H

#include <kite/ktypes.h>

// stub functions for unresolved symbols
void *kite_memset(void *s, int c, size_t n);
void *kite_memcpy(void *dest, const void *src, size_t n);
int kite_memcmp(const void *s1, const void *s2, size_t n);
void *kite_memmove(void *dest, const void *src, size_t n);

size_t kite_strlen(const char *s);
int kite_strcmp(const char *s1, const char *s2);
char *kite_strcpy(char *dest, const char *src);
char *kite_strncpy(char *dest, const char *src, size_t n);

int kite_snprintf(char *buf, size_t size, const char *fmt, ...);

#endif // _KITE_STUBS_H

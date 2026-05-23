/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_TLSF_H
#define _KITE_TLSF_H

#include <kite/ktypes.h>

typedef void* tlsf_t;

tlsf_t tlsf_create(void *mem, size_t bytes);
void tlsf_destroy(tlsf_t tlsf);
void *tlsf_malloc(tlsf_t tlsf, size_t size);
void *tlsf_memalign(tlsf_t tlsf, size_t align, size_t size);
void tlsf_free(tlsf_t tlsf, void *ptr);

#endif // _KITE_TLSF_H

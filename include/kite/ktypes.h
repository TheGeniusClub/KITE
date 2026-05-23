/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_KTYPES_H
#define _KITE_KTYPES_H

#ifndef __ASSEMBLY__
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define __section(name) __attribute__((section(#name)))
#define __aligned(x) __attribute__((aligned(x)))
#define __noinline __attribute__((noinline))
#define __packed __attribute__((packed))

#endif // _KITE_KTYPES_H

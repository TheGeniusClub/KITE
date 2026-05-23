/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_KALLSYMS_TYPES_H
#define _KITE_KALLSYMS_TYPES_H

#include <kite/ktypes.h>

#define KALLSYMS_MAX_NAME 512
#define KALLSYMS_TOKEN_TABLE_SIZE 256

typedef struct {
    u8 *token_table;
    u32 token_table_size;
    u8 token_index[KALLSYMS_TOKEN_TABLE_SIZE];
    u32 *addresses;
    u32 *offsets;
    u8 *names;
    u32 num_syms;
    u32 elem_size;
    u32 kernel_version;
} kallsyms_info_t;

#endif // _KITE_KALLSYMS_TYPES_H

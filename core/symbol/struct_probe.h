/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_STRUCT_PROBE_H
#define _KITE_STRUCT_PROBE_H

#include <kite/ktypes.h>

// struct probe - dynamic offset detection
int struct_probe_init(void);

// probe struct offsets
u32 probe_task_struct_offset(const char *member);
u32 probe_cred_offset(const char *member);
u32 probe_mm_struct_offset(const char *member);
u32 probe_file_offset(const char *member);

// preset offsets
int struct_probe_load_preset(const char *path);
int struct_probe_save_preset(const char *path);

#endif // _KITE_STRUCT_PROBE_H

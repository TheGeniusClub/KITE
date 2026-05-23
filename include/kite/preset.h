/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_PRESET_H
#define _KITE_PRESET_H

#include <kite/ktypes.h>

// preset data structure for patcher
struct kite_preset {
    u32 magic;          // 'KITE'
    u32 version;
    u64 kernel_pa;
    u64 paging_init_offset;
    u64 map_cave_offset;
    u64 start_offset;
    u32 kernel_version;
    u32 flags;
};

#define KITE_PRESET_MAGIC 0x4B495445  // 'KITE'

#endif // _KITE_PRESET_H

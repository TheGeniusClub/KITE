/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_OFFSETS_H
#define _KITE_OFFSETS_H

#include <kite/ktypes.h>

// kernel struct offsets (auto-probed or preset)
struct kite_offsets {
    // task_struct
    u32 task_pid;
    u32 task_comm;
    u32 task_cred;
    
    // cred
    u32 cred_uid;
    u32 cred_gid;
    u32 cred_suid;
    u32 cred_sgid;
    u32 cred_euid;
    u32 cred_egid;
    u32 cred_fsuid;
    u32 cred_fsgid;
    u32 cred_cap_inheritable;
    u32 cred_cap_permitted;
    u32 cred_cap_effective;
    u32 cred_cap_bset;
    u32 cred_securebits;
    
    // mm_struct
    u32 mm_mmap;
    u32 mm_pgd;
    
    // file
    u32 file_f_op;
    u32 file_private_data;
};

extern struct kite_offsets kite_offsets;

int offsets_init(void);
u32 offset_task_struct(const char *member);
u32 offset_cred(const char *member);

#endif // _KITE_OFFSETS_H

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_PARASITE_IPC_H
#define _KITE_PARASITE_IPC_H

#include <kite/ktypes.h>

// parasite mode IPC
int parasite_ipc_init(void);
int parasite_ipc_inject(u32 pid, const char *payload);
int parasite_ipc_hide_proc(u32 pid);

#endif // _KITE_PARASITE_IPC_H

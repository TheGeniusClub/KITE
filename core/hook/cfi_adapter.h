/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_CFI_ADAPTER_H
#define _KITE_CFI_ADAPTER_H

#include <kite/ktypes.h>

// CFI (Control Flow Integrity) bypass
int cfi_bypass_init(void);
int cfi_bypass_enable(void);
int cfi_bypass_disable(void);

// BTI (Branch Target Identification)
u32 kite_emit_bti(void);

#endif // _KITE_CFI_ADAPTER_H

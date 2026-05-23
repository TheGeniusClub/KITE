/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_INSN_RELOC_H
#define _KITE_INSN_RELOC_H

#include <kite/ktypes.h>

// ARM64 instruction relocation
#define A64_INSN_SIZE 4

// branch instructions
#define A64_B 0x14000000
#define A64_BL 0x94000000
#define A64_CBZ 0x34000000
#define A64_CBNZ 0x35000000
#define A64_TBZ 0x36000000
#define A64_TBNZ 0x37000000

// load/store
#define A64_LDR 0x58000000
#define A64_STR 0x58000000

// system
#define A64_SVC 0xd4000001
#define A64_HVC 0xd4000002
#define A64_SMC 0xd4000003

// instruction helpers
static inline u32 a64_insn_encode_b(u32 insn, s64 offset)
{
    return (insn & 0xFC000000) | ((offset >> 2) & 0x3FFFFFF);
}

static inline s64 a64_insn_decode_b(u32 insn)
{
    s64 offset = (insn & 0x3FFFFFF) << 2;
    if (offset & 0x8000000) offset |= 0xFFFFFFFFF0000000ULL;
    return offset;
}

int insn_reloc(void *dst, void *src, size_t len);
int insn_patch(void *addr, u32 insn);
u32 insn_read(void *addr);

#endif // _KITE_INSN_RELOC_H

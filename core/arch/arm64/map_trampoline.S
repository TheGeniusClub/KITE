/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Phase 2 map trampoline - called from paging_init
 */

#define __ASSEMBLY__

.section .map.text, "ax"
.align 2
.global map_trampoline
.type map_trampoline, %function
map_trampoline:
    // Save registers
    stp x29, x30, [sp, #-16]!
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!

    // x19 = KITE base = map_trampoline - 0x1400
    adr x0, map_trampoline
    mov x1, #0x1400
    sub x19, x0, x1

    // x20 = data_area (KITE base + 0x80)
    add x20, x19, #0x80

    // Read paging_init_va from data_area
    ldr x21, [x20, #0]

    // Read paging_init_backup from data_area
    ldr w22, [x20, #8]

    // Restore original paging_init instruction
    str w22, [x21]

    // Flush caches
    dsb ish
    ic ivau, x21
    dsb ish
    isb

    // Call original paging_init
    blr x21

    // Read kite_late_init_va from data_area
    ldr x2, [x20, #16]

    // Read preset address (kite_base_va) from data_area
    ldr x0, [x20, #24]

    // Call kite_late_init(preset)
    blr x2

    // Restore registers
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16
    ldp x29, x30, [sp], #16

    // Return to continue kernel boot
    ret

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch setup1.S
 * Phase 1: MMU-off startup stub
 */

#define __ASSEMBLY__
#include "setup.h"

.section .setup.text, "ax"
.align 2
.type memcpy8, %function
memcpy8:
    cmp x2, 0
    ble .l8end
.l8loop:
    ldrb w3, [x1], 1
    strb w3, [x0], 1
    subs x2, x2, 1
    cbnz x2, .l8loop
.l8end:
    ret

.section .setup.text, "ax"
.align 2
.type rmemcpy32, %function
rmemcpy32:
    subs x2, x2, #0x4
    b.lt .r32end
.r32loop:
    ldr w3, [x1, x2]
    str w3, [x0, x2]
    subs x2, x2, #0x4
    b.ge .r32loop
.r32end:
    ret

.section .setup.text, "ax"
.align 2
.type start_prepare, %function
start_prepare:
    stp x29, x30, [sp, -16]!
    stp x19, x20, [sp, -16]!
    stp x21, x22, [sp, -16]!
    stp x23, x24, [sp, -16]!
    // kernel_pa
    mov x19, x0
    
    // map_data
    adrp x9, map_data
    add x9, x9, :lo12:map_data

    // setup_preset
    adrp x10, setup_preset
    add x10, x10, :lo12:setup_preset

    // start_preset
    adrp x11, start_preset
    add x11, x11, :lo12:start_preset

    // start_preset.kernel_version = setup_preset.kernel_version;
    ldr w13, [x10, #setup_kernel_version_offset]
    str w13, [x11, #start_kernel_version_offset]

    // start_preset.kallsyms_lookup_name_offset = setup_preset.kallsyms_lookup_name_offset;
    ldr x13, [x10, #setup_kallsyms_lookup_name_offset_offset]
    str x13, [x11, #start_kallsyms_lookup_name_offset_offset]

    // start_preset.kernel_size = setup_preset.kernel_size;
    ldr x13, [x10, #setup_kernel_size_offset]
    str x13, [x11, #start_kernel_size_offset]

    // start_preset.start_offset = setup_preset.start_offset;
    ldr x13, [x10, #setup_start_offset_offset]
    str x13, [x11, #start_start_offset_offset]
    mov x21, x13

    // start_preset.extra_size = setup_preset.extra_size;
    ldr x13, [x10, #setup_extra_size_offset]
    str x13, [x11, #start_extra_size_offset]

    // start_preset.kernel_pa = kernel_pa;
    str x19, [x11, #start_kernel_pa_offset]
    
    // start_preset.map_offset = setup_preset.map_offset
    ldr x13, [x10, #setup_map_offset_offset]
    str x13, [x11, #start_map_offset_offset]
    mov x20, x13

    // start_preset.printk_offset = setup_preset.printk_offset
    ldr x13, [x10, #setup_printk_offset_offset]
    str x13, [x11, #start_printk_offset_offset]

    // copy superkey
    add x0, x11, #start_superkey_offset
    add x1, x10, #setup_superkey_offset
    mov x2, #SUPER_KEY_LEN
    bl memcpy8

    // backup map area
    adrp x13, _map_end
    add x13, x13, :lo12:_map_end
    adrp x14, _map_start
    add x14, x14, :lo12:_map_start
    sub x2, x13, x14

    str x2, [x11, #start_map_backup_len_offset]
    add x0, x11, #start_map_backup_offset
    add x1, x19, x20
    bl memcpy8

    // uint64_t start_img_size = setup_preset.kiteimg_size - (_kp_start - _link_base)
    ldr x22, [x10, #setup_kiteimg_size_offset]
    adrp x23, _kp_start
    add x23, x23, :lo12:_kp_start
    adrp x24, _link_base
    add x24, x24, :lo12:_link_base
    sub x23, x23, x24
    sub x22, x22, x23

    // map_data.start_img_size = start_img_size;
    str x22, [x9, #map_start_img_size_offset]

    // memcpy(kernel_pa + start_offset, _kp_start, start_img_size + setup_preset.extra_size)
    add x0, x19, x21
    adrp x1, _kp_start
    add x1, x1, :lo12:_kp_start
    ldr x2, [x10, #setup_extra_size_offset]
    add x2, x2, x22
    bl rmemcpy32

    // Restore
    ldp x23, x24, [sp], 16
    ldp x21, x22, [sp], 16
    ldp x19, x20, [sp], 16
    ldp x29, x30, [sp], 16
    ret

.section .setup.text, "ax"
.align 2
.type map_prepare, %function
map_prepare:
    stp x29, x30, [sp, -16]!
    stp x19, x20, [sp, -16]!
    mov x19, x0
    // map_data
    adrp x9, map_data
    add x9, x9, :lo12:map_data
    // setup_preset
    adrp x10, setup_preset
    add x10, x10, :lo12:setup_preset

    // map_data.kernel_pa = kernel_pa;
    str x19, [x9, #map_kernel_pa_offset]

    // map_data.map_offset = setup_preset.map_offset;
    ldr x11, [x10, #setup_map_offset_offset]
    str x11, [x9, #map_map_offset_offset]
    mov x14, x11

    // map_data.paging_init_relo = setup_preset.paging_init_offset;
    ldr x11, [x10, #setup_paging_init_offset_offset]
    str x11, [x9, #map_paging_init_relo_offset]
    mov x15, x11

    // map_data.map_symbol = setup_preset.map_symbol
    add x0, x9, #map_map_symbol_offset
    add x1, x10, #setup_map_symbol_offset
    mov x2, #MAP_SYMBOL_SIZE
    bl memcpy8

    // set start memory info
    // map_data.start_offset = setup_preset.start_offset;
    ldr x11, [x10, #setup_start_offset_offset]
    str x11, [x9, #map_start_offset_offset]

    // map_data.start_size = (int64_t)(_kp_end - _kp_start);
    adrp x11, _kp_end
    add x11, x11, :lo12:_kp_end
    adrp x12, _kp_start
    add x12, x12, :lo12:_kp_start
    sub x11, x11, x12
    str x11, [x9, #map_start_size_offset]
    
    // map_data.extra_size = setup_preset.extra_size;
    ldr x11, [x10, #setup_extra_size_offset]
    str x11, [x9, #map_extra_size_offset]

    // map_data.alloc_size = HOOK_ALLOC_SIZE + MEMORY_ROX_SIZE + MEMORY_RW_SIZE;
    mov x11, #HOOK_ALLOC_SIZE
    add x11, x11, #MEMORY_ROX_SIZE
    add x11, x11, #MEMORY_RW_SIZE
    str x11, [x9, #map_alloc_size_offset]

    // backup and hook paging_init
    add x13, x15, x19
    ldr w12, [x13]

    mov w3, #0x201F
    movk w3, #0xD503, lsl#16
    orr w1, w3, #0x100
    mov w2, #0xFFFFFD1F
    and w0, w12, w2
    cmp w0, w1
    b.ne .backup
    mov w12, w3
    add x11, x13, #4
.cmp_auti:
    ldr w0, [x11], #4
    and w0, w0, w2
    cmp w0, w1
    b.ne .cmp_auti
    stur w3, [x11, #-4]

.backup:
    str w12, [x9, #map_paging_init_backup_offset]
    dsb ish

    // uint64_t replace_offset = (uint64_t)(_paging_init - _map_start) + map_offset;
    adrp x11, _paging_init
    add x11, x11, :lo12:_paging_init
    adrp x12, _map_start
    add x12, x12, :lo12:_map_start
    sub x11, x11, x12
    add x11, x11, x14

    // B_REL(paging_init_offset, replace_offset)
    sub x15, x11, x15
    ubfx w15, w15, #2, #26
    mov w12, #0x14000000
    orr w15, w15, w12
    str w15, [x13]

    // relocate map
    adrp x2, _map_end
    add x2, x2, :lo12:_map_end
    adrp x1, _map_start
    add x1, x1, :lo12:_map_start
    sub x2, x2, x1
    add x0, x19, x14
    bl memcpy8

    // Restore
    ldp x19, x20, [sp], 16
    ldp x29, x30, [sp], 16
    dsb ish
    ret

.section .setup.text, "ax"
.align 2
.type setup, %function
setup:
    stp x29, x30, [sp, -16]!
    stp x0, x1, [sp, -16]!
    stp x2, x3, [sp, -16]!
    stp x4, x5, [sp, -16]!
    stp x6, x7, [sp, -16]!
    stp x8, x18, [sp, -16]!
    stp x19, x20, [sp, -16]!
    stp x21, x22, [sp, -16]!
    stp x23, x24, [sp, -16]!
    stp x25, x26, [sp, -16]!
    stp x27, x28, [sp, -16]!

    // _link_base
    adrp x9, _link_base
    add x9, x9, :lo12:_link_base

    // setup_preset
    adrp x10, setup_preset
    add x10, x10, :lo12:setup_preset
    mov x20, x10

    // uint64 kernel_pa = (uint64_t)_link_base - setup_preset.setup_offset;
    ldr x11, [x10, #setup_setup_offset_offset]
    sub x12, x9, x11
    mov x19, x12

    mov x0, x19
    bl start_prepare
    mov x0, x19
    bl map_prepare

    // memcpy(kernel_pa, setup_preset.header_backup, sizeof(setup_preset.header_backup));
    mov x0, x19
    add x1, x20, #setup_header_backup_offset
    mov x2, #8
    bl memcpy8

    dsb ish
    ic iallu
    dsb ish
    isb

    mov x16, x19

    // Restore
    ldp x27, x28, [sp], 16
    ldp x25, x26, [sp], 16
    ldp x23, x24, [sp], 16
    ldp x21, x22, [sp], 16
    ldp x19, x20, [sp], 16
    ldp x8, x18, [sp], 16
    ldp x6, x7, [sp], 16
    ldp x4, x5, [sp], 16
    ldp x2, x3, [sp], 16
    ldp x0, x1, [sp], 16
    ldp x29, x30, [sp], 16

    // Restore sp
    ldp x9, x10, [sp], 16
    mov sp, x9
    br x16

.section .entry.text, "ax"
.global setup_entry
.type setup_entry, %function
setup_entry:
    mov x9, sp
    adrp x11, stack
    add x11, x11, :lo12:stack
    add x11, x11, #0x800
    mov sp, x11
    stp x9, x10, [sp, -16]!
    b setup

.section .bss
.align 4
stack:
    .space 0x800

#undef __ASSEMBLY__

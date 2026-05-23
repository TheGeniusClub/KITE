/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KITE linker script - Two-stage layout matching KernelPatch
 * Stage 1: setup (MMU-off, temporary)
 * Stage 2: kp engine (copied to kernel padding, persistent)
 * Reference from KernelPatch kernel/kpimg.lds
 */

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(setup_entry)

SECTIONS
{
    /*
     * _link_base = 0xD000 places the setup block in the last 12KB
     * of a conceptual 64KB frame. This ensures _kp_start at ALIGN(64K)
     * maps to file offset 0x3000 with no holes.
     */
    . = 0xD000;
    _link_base = .;

    /*
     * Stage 1 Data (.setup.data)
     * Fixed 4KB. Contains preset, map_data, and setup variables.
     * Patcher writes setup_preset at offset 0 within this block.
     */
    .setup.data : {
        _setup_start = .;
        . = _setup_start + 0x40; /* skip kite_header_t padding */
        *(.preset.data)
        *(.setup.data)
        . = _setup_start + 0x1000;
    }

    /*
     * Stage 1 Code (.setup.text)
     * Entry point (setup_entry), memcpy, and setup logic.
     * Patcher redirects kernel header B-instruction to offset 0x1000
     * within the KITE image (i.e., setup_entry).
     */
    .setup.text : {
        *(.entry.text)
        *(.text.startup)
        *(.text.setup)
        *startup.o(.text)
        . = ALIGN(16);
        _setup_end = .;
    }

    /*
     * Stage 1 Map (.setup.map)
     * Map trampoline code copied to kernel .text padding at runtime.
     * Must be small (KP limits to < 0xA00).
     */
    .setup.map : {
        _map_start = .;
        /* Place map_data at start of .setup.map to align with _map_start */
        *map_text.o(.map.data)
        *(.map.data)
        *(.map.text)
        *(.text.map)
        *map_trampoline.o(.text)
        *map_text.o(.text)
        . = ALIGN(16);
        _map_end = .;
    }

    /*
     * Assert setup stage fits within 12KB (0xD000 to 0x10000)
     * so that _kp_start at ALIGN(64K) lands cleanly.
     */
    ASSERT(SIZEOF(.setup.data) + SIZEOF(.setup.text) + SIZEOF(.setup.map) <= 0x3000,
           "Setup stage exceeds 12KB limit")
    ASSERT(SIZEOF(.setup.map) < 0xa00, "Map section too large")
    ASSERT(_paging_init - _map_start < 0x100000, "adr range exceeded")

    /*
     * Stage 2 starts at the 64KB boundary (link addr 0x10000).
     * File offset: 0x10000 - 0xD000 = 0x3000.
     * This is the persistent payload copied to kernel_pa + start_offset.
     */
    . = ALIGN(0x10000);
    _kp_start = .;

    /*
     * Stage 2 Code and Read-Only Data
     * All remaining .text, .rodata, and per-function text sections.
     */
    .kp.text : {
        _kp_text_start = .;
        *(.start.text)
        *(.text)
        *(.text.*)
        *(.rodata)
        *(.rodata.*)
        . = ALIGN(16);
        _kp_text_end = .;
    }

    /*
     * Stage 2 Data and BSS
     * Initialized data, BSS, and common symbols.
     */
    .kp.data : {
        _kp_data_start = .;
        *(.start.data)
        *(.data)
        *(.data.*)
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(16);
        _kp_data_end = .;
    }

    /* Use actual data end, not 64KB alignment */
    _kp_end = _kp_data_end;
    _link_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.note)
        *(.note.*)
        *(.eh_frame)
        *(.interp .dynamic)
        *(.dynsym .dynstr .hash .gnu.hash)
    }
}

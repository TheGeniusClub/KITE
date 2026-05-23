# KITE v6 Development Plan
## Copyright (C) 2026 Dere3046
## SPDX-License-Identifier: GPL-2.0

## Current Status: Phase 2.5 Complete

### Completed

- [x] P0: Phase 1 startup.S (MMU-off stub)
- [x] P0: Phase 2 paging_init handler
- [x] P0: Phase 2.5 memory relocation (memblock_alloc + get_or_create_pte + RX mapping + jump)
- [x] P0: Text Cave v2 (runtime copy + runtime hook)
- [x] P1: Kallsyms parser v3
- [x] P6: Boot.img parser
- [x] P6: Kernel injection

### In Progress

- [ ] QEMU full boot test with rootfs
- [ ] Stage 2 full implementation (TLSF + symbols + hooks)

### TODO

- [ ] Real device testing
- [ ] P1: SKRoot features (version tier, entropy verification)
- [ ] P2: PAC-aware hook
- [ ] P4: SELinux bypass

## Latest Decisions

### 2026-05-23: Copy KP setup phase
- **Reason**: KITE custom implementation had subtle bugs in execution order
- **Action**: Copied KP setup1.S and map.c, adapted for KITE data structures
- **Result**: QEMU boots successfully, Stage 2 reached

### 2026-05-23: get_data() approach
- **Choice**: Use `adr map_data` (not `get_myva()-sizeof`)
- **Reason**: KITE linker layout differs from KP, `adr` is layout-independent within ±1MB
- **Safety**: Added linker assertion `_paging_init - _map_start < 0x100000`

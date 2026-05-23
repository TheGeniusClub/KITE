# KITE v6 Setup Phase Rewrite Decisions
## Copyright (C) 2026 Dere3046
## SPDX-License-Identifier: GPL-2.0

## Date: 2026-05-23
## Context: QEMU boots with KP but not KITE, root cause analysis complete

---

## Root Cause Summary

QEMU output "KSMHB" then hangs. Primary issue: **setup phase execution order and address calculation methods deviate from proven KP design**.

### Specific Bugs Identified

1. **get_data() uses `adr map_data` instead of `get_myva() - sizeof(map_data_t)`**
   - KITE: `adr map_data` assumes map_data is within ±1MB of _paging_init
   - KP: `get_myva() - sizeof(map_data_t)` uses current PC alignment, layout-independent
   - Risk: Silent failure if map_data moves

2. **start_prepare/map_prepare order reversed**
   - KITE: memcpy8 .setup.map → init fields → hook
   - KP: init fields → hook → memcpy8 .setup.map
   - Impact: KP copies already-initialized map_data to map area; KITE overwrites then re-inits

3. **Hardcoded constants instead of runtime calculation**
   - KITE: MAP_FUNC_OFFSET=0x464, KITEIMG_HEADER_SIZE=0x3000, KITEIMG_STAGE2_SIZE=0x1490
   - KP: dynamic `adrp _paging_init - adrp _map_start`, `adrp _kp_start - adrp _link_base`
   - Risk: Linker script changes break silently

4. **mem_proc uses manual loop instead of struct assignment**
   - KITE: `for(i=0; i<sizeof/8; i++) dst[i]=s[i]`
   - KP: `*data = *get_data()`
   - Impact: Less readable, potential alignment issues

---

## Design Document Alignment

KITE_V6_DESIGN.md states:
- "Phase 1 startup.S: 来源 KP setup1.S"
- "Text Cave v2: 参考 KP setup1.S:220-283"
- "KASLR 动态计算: 运行时通过 B-offset 计算 paging_init_va"

**Current deviations are implementation artifacts, not design decisions.**

---

## Decisions

### Take from KP (must fix)

| Component | KP Approach | Reason |
|-----------|-------------|--------|
| get_data() | `get_myva() - sizeof(map_data_t)` | Layout-independent, safer |
| start_prepare order | init fields → calc size → memcpy | Natural flow, matches linker |
| map_prepare order | init fields → hook → memcpy8 | Copies initialized data |
| start_img_size | Dynamic `adrp _kp_start - adrp _link_base` | Linker-proof |
| B-instruction calc | Dynamic `adrp _paging_init - adrp _map_start` | Linker-proof |
| mem_proc | `*data = *get_data()` | Clean, compiler-optimized |
| get_myva() | Keep KP's `adr .` + align | Already same |

### Keep KITE-specific (design choice)

| Component | KITE Approach | Reason |
|-----------|---------------|--------|
| setup_preset_t | Compact struct (no root/su fields) | KITE is generic hook framework, not root tool |
| kite_printk() | Wrapped printk via preset | Modular, consistent with KITE style |
| MAP_DEBUG | Conditional compilation | Useful for Phase 2 debugging |
| header_backup[8] | Keep 8 bytes (not KP's 64) | KITE preset is compact, patcher handles header correctly |

### Removed from consideration

- Hardcoded constants: completely removed, use dynamic calculation
- Manual memcpy loops: replaced with struct assignment or memcpy8

---

## Implementation Plan

1. **Phase 1**: Rewrite startup.S
   - start_prepare: KP order, dynamic calc
   - map_prepare: KP order, dynamic B-calc
   - setup: keep KITE register save/restore

2. **Phase 2**: Update map_text.c
   - get_data(): use get_myva() pattern
   - mem_proc(): struct assignment

3. **Phase 3**: Verify linker script
   - Ensure .map.data is at start of .setup.map
   - Ensure _paging_init is within .setup.map

4. **Phase 4**: Build and QEMU test
   - Expect "KSMHB" + Linux banner

---

## Files Modified

- `core/arch/arm64/startup.S` - Full rewrite of start_prepare, map_prepare
- `core/arch/arm64/map_text.c` - get_data(), mem_proc()
- `core/arch/arm64/kite.lds` - Verify layout (may need minor adjust)
- `core/arch/arm64/setup.h` - Verify offsets match

## References

- KP setup1.S: `kernelpatch/kernel/base/setup1.S`
- KP map.c: `kernelpatch/kernel/base/map.c`
- KP setup.h: `kernelpatch/kernel/base/setup.h`
- KITE_V6_DESIGN.md: P0 Phase 1/2/2.5 specifications

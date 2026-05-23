# KITE v6 Development Plan
## Copyright (C) 2026 Dere3046
## SPDX-License-Identifier: GPL-2.0

## Current Status: Phase 2.5 Verified, Stage 2 Stub Only

### P0: Two-Stage Injection Core (DONE)

- [x] Phase 1 startup.S (MMU-off stub, Text Cave v2 runtime copy + hook)
- [x] Phase 2 paging_init handler (map_text.c, memblock_alloc + get_or_create_pte + RX mapping + jump)
- [x] Phase 2.5 memory relocation (verified on QEMU, prints kimage_voffset)
- [x] Patcher boot.img parser + kernel injection + preset generation

### Stage 2 Framework (IN PROGRESS)

**Blocker: No working alloc/printk in Stage 2 context**

- [x] **S1: Runtime Stubs** — memset/memcpy/strcmp/memmove/printk/vsnprintf/snprintf
- [x] **S2: TLSF Pool Init** — Allocate 2MB RW + 4MB ROX via memblock_alloc, call tlsf_create
- [x] **S3: Symbol Engine** — Hash cache, B-follow, stripped fallback scanner, entropy verify
- [x] **S4: Hook Engine Core** — hook_engine.c + fphook.c + hotpatch.c, full KP logic with KITE TLSF pool
- [x] **S5: Memory Mgmt** — execmem_alloc, alias_page, direct_pte with PTE walk
- [x] **S6: Module System** — ELF loader, init/ctl0/ctl1/exit lifecycle, KIMC, task_local
- [ ] **S7: Full QEMU Boot** — ARM64 initrd, verify to shell

### P1-P6 Feature Status (Reality Check)

| Feature | Status | Note |
|---------|--------|------|
| Kallsyms Parser v3 | Partial | symbol_engine.c wraps kallsyms_lookup_name only |
| Inline Hook | Header Only | hook_engine.h defined, no impl |
| Wrap Hook | Header Only | Need transit stubs from KP |
| FP Hook | Header Only | Need fphook.c from KP |
| Syscall Hook | Header Only | syscall_hook.h only |
| Hotpatch | Header Only | hotpatch.h only, need PTE + stop_machine impl |
| CFI/BTI | Header Only | cfi_adapter.h only |
| Trampoline | Header Only | trampoline.h only |
| Module Loader | Header Only | module.h only |
| SELinux Bypass | Not Started | P4 deferred |
| ARM64 Initrd | Not Started | Need for full boot test |

### TODO

1. **Immediate**: S1 + S2 (stubs + TLSF init) — unblock all other Stage 2 work
2. **Next**: S4 Hook Engine — reference KP hook.c + hotpatch.c, adapt to KITE mem pool
3. **Then**: S3 Symbol Engine — stripped fallback for devices without kallsyms_lookup_name
4. **Finally**: S6 Module + S7 QEMU full boot

### Design Decisions Log

#### 2026-05-23: Hook Engine Reference Strategy
- **KP hook.c** provides complete inline hook + wrap hook + instruction relocation
- **KP hotpatch.c** provides atomic cross-CPU patching via stop_machine + PTE
- **KITE adaptation**: Use KITE TLSF pools for transit stub allocation, keep KP logic
- **Rule**: Copy KP hook core, adapt memory allocation, keep file header reference

#### 2026-05-23: Stage 2 Entry Architecture
- `start()` in `.start.text` section called from `_paging_init` after relocation
- `start_preset` contains kernel_pa, printk_offset, kallsyms_lookup_name_offset
- Must initialize memory pools before any other subsystem

#### 2026-05-23: printk Strategy (S1 Decision)
- Use kernel printk directly via `start_preset.printk_offset` during early Stage 2
- Formatting done via `%` format strings passed directly (kernel handles it)
- Later: add `kite_snprintf` for complex formatting when needed

### Reference Sources

- Phase 1/2/2.5: KernelPatch `setup1.S`, `map.c`, `start.c`
- Hook Engine: KernelPatch `hook.c`, `fphook.c`, `hotpatch.c`
- TLSF: KernelPatch (direct reuse)
- Kallsyms: KernelPatch `kallsym.c` + SKRoot version tier/entropy ideas

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * ELF64 module loader with ARM64 RELA relocation
 * Reference from KernelPatch module.c and relo.c
 */

#include "loader.h"
#include "module.h"
#include "elf64.h"
#include "stubs.h"
#include "memory/tlsf.h"
#include "symbol/symbol_engine.h"
#include "arch/arm64/setup.h"

extern tlsf_t kite_rw_mem;
extern tlsf_t kite_rox_mem;

struct elf64_shdr {
    Elf64_Word sh_name;
    Elf64_Word sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr sh_addr;
    Elf64_Off sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word sh_link;
    Elf64_Word sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
};

struct elf64_sym {
    Elf64_Word st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half st_shndx;
    Elf64_Addr st_value;
    Elf64_Xword st_size;
};

struct elf64_rela {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;
};

#define ELF64_R_SYM(i) ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffff)

struct load_info {
    void *hdr;
    size_t len;
    struct elf64_shdr *sechdrs;
    char *secstrings;
    char *strtab;
    struct elf64_sym *syms;
    unsigned int nsyms;
    unsigned int index_sym;
    unsigned int index_str;
};

#ifdef __TEST_HOST__
static inline void flush_icache_all(void) { }
#else
static inline void flush_icache_all(void)
{
    asm volatile("dsb ish" ::: "memory");
    asm volatile("ic ialluis");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
}
#endif

static int elf_header_check(struct elf64_hdr *hdr)
{
    if (hdr->e_ident[0] != ELFMAG0 || hdr->e_ident[1] != ELFMAG1 ||
        hdr->e_ident[2] != ELFMAG2 || hdr->e_ident[3] != ELFMAG3) {
        kite_printk("KITE: bad ELF magic\n");
        return -1;
    }
    if (hdr->e_type != ET_REL) {
        kite_printk("KITE: not relocatable ELF\n");
        return -1;
    }
    if (hdr->e_machine != EM_AARCH64) {
        kite_printk("KITE: not ARM64 ELF\n");
        return -1;
    }
    return 0;
}

static int setup_load_info(struct load_info *info)
{
    struct elf64_hdr *hdr = (struct elf64_hdr *)info->hdr;
    struct elf64_shdr *sechdrs = (struct elf64_shdr *)((char *)info->hdr + hdr->e_shoff);
    info->sechdrs = sechdrs;
    info->secstrings = (char *)info->hdr + sechdrs[hdr->e_shstrndx].sh_offset;
    
    for (unsigned int i = 0; i < hdr->e_shnum; i++) {
        sechdrs[i].sh_addr = (Elf64_Addr)((char *)info->hdr + sechdrs[i].sh_offset);
        
        if (sechdrs[i].sh_type == SHT_SYMTAB) {
            info->index_sym = i;
            info->index_str = sechdrs[i].sh_link;
            info->syms = (struct elf64_sym *)((char *)info->hdr + sechdrs[i].sh_offset);
            info->nsyms = sechdrs[i].sh_size / sizeof(struct elf64_sym);
            info->strtab = (char *)info->hdr + sechdrs[info->index_str].sh_offset;
        }
    }
    
    if (!info->index_sym) {
        kite_printk("KITE: no symtab\n");
        return -1;
    }
    return 0;
}

static int layout_sections(struct load_info *info, size_t *text_size, size_t *ro_size, size_t *total_size)
{
    struct elf64_hdr *hdr = (struct elf64_hdr *)info->hdr;
    struct elf64_shdr *sechdrs = info->sechdrs;
    
    size_t offset = 0;
    *text_size = 0;
    *ro_size = 0;
    
    // Priority: text > rodata > data > bss
    unsigned int priorities[] = { SHF_EXECINSTR | SHF_ALLOC, SHF_ALLOC, SHF_WRITE | SHF_ALLOC, 0 };
    
    for (int p = 0; p < 4; p++) {
        for (unsigned int i = 0; i < hdr->e_shnum; i++) {
            if ((sechdrs[i].sh_flags & (SHF_ALLOC | SHF_WRITE | SHF_EXECINSTR)) != priorities[p])
                continue;
            if (!(sechdrs[i].sh_flags & SHF_ALLOC))
                continue;
            
            sechdrs[i].sh_entsize = offset;
            if (sechdrs[i].sh_addralign > 1) {
                offset = (offset + sechdrs[i].sh_addralign - 1) & ~(sechdrs[i].sh_addralign - 1);
                sechdrs[i].sh_entsize = offset;
            }
            
            if (sechdrs[i].sh_flags & SHF_EXECINSTR) {
                if (offset + sechdrs[i].sh_size > *text_size)
                    *text_size = offset + sechdrs[i].sh_size;
            } else if (!(sechdrs[i].sh_flags & SHF_WRITE)) {
                if (offset + sechdrs[i].sh_size > *ro_size)
                    *ro_size = offset + sechdrs[i].sh_size;
            }
            
            offset += sechdrs[i].sh_size;
        }
    }
    
    *total_size = offset;
    return 0;
}

static int simplify_symbols(struct load_info *info, void *base)
{
    struct elf64_hdr *hdr = (struct elf64_hdr *)info->hdr;
    struct elf64_shdr *sechdrs = info->sechdrs;
    struct elf64_sym *syms = info->syms;
    
    for (unsigned int i = 1; i < info->nsyms; i++) {
        const char *name = info->strtab + syms[i].st_name;
        
        switch (syms[i].st_shndx) {
        case SHN_COMMON:
            kite_printk("KITE: common symbol %s not supported\n", name);
            return -1;
        case SHN_ABS:
            break;
        case SHN_UNDEF:
            if (syms[i].st_info & (STB_GLOBAL | STB_WEAK)) {
                uint64_t addr = symbol_lookup_name(name);
                if (addr) {
                    syms[i].st_value = addr;
                } else {
                    kite_printk("KITE: undef symbol %s not found\n", name);
                    if (syms[i].st_info & STB_WEAK) {
                        syms[i].st_value = 0;
                    } else {
                        return -1;
                    }
                }
            }
            break;
        default:
            if (syms[i].st_shndx < hdr->e_shnum) {
                syms[i].st_value += (Elf64_Addr)base + sechdrs[syms[i].st_shndx].sh_entsize;
            }
            break;
        }
    }
    return 0;
}

static uint64_t do_reloc(enum aarch64_reloc_op op, void *loc, uint64_t val, int len)
{
    uint64_t result = 0;
    switch (op) {
    case RELOC_OP_ABS:
        result = val;
        break;
    case RELOC_OP_PREL:
        result = val - (uint64_t)loc;
        break;
    case RELOC_OP_PAGE:
        result = (val & ~0xfff) - ((uint64_t)loc & ~0xfff);
        break;
    default:
        return 0;
    }
    
    if (len == 64) {
        *(uint64_t *)loc = result;
    } else if (len == 32) {
        *(uint32_t *)loc = (uint32_t)result;
    } else if (len == 16) {
        *(uint16_t *)loc = (uint16_t)result;
    }
    return result;
}

static int apply_relocation(struct load_info *info, void *base, struct elf64_rela *rela, struct elf64_sym *sym)
{
    void *loc = (char *)base + rela->r_offset;
    uint64_t val = sym->st_value + rela->r_addend;
    uint64_t insn;
    int64_t sval;
    
    switch (ELF64_R_TYPE(rela->r_info)) {
    case R_AARCH64_ABS64:
        do_reloc(RELOC_OP_ABS, loc, val, 64);
        break;
    case R_AARCH64_ABS32:
        do_reloc(RELOC_OP_ABS, loc, val, 32);
        break;
    case R_AARCH64_ABS16:
        do_reloc(RELOC_OP_ABS, loc, val, 16);
        break;
    case R_AARCH64_PREL64:
        do_reloc(RELOC_OP_PREL, loc, val, 64);
        break;
    case R_AARCH64_PREL32:
        do_reloc(RELOC_OP_PREL, loc, val, 32);
        break;
    case R_AARCH64_PREL16:
        do_reloc(RELOC_OP_PREL, loc, val, 16);
        break;
    case R_AARCH64_MOVW_UABS_G0:
    case R_AARCH64_MOVW_UABS_G0_NC:
        insn = *(uint32_t *)loc;
        insn &= ~0x1fffe0;
        insn |= ((val & 0xffff) << 5);
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_MOVW_UABS_G1:
    case R_AARCH64_MOVW_UABS_G1_NC:
        insn = *(uint32_t *)loc;
        insn &= ~0x1fffe0;
        insn |= (((val >> 16) & 0xffff) << 5);
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_MOVW_UABS_G2:
    case R_AARCH64_MOVW_UABS_G2_NC:
        insn = *(uint32_t *)loc;
        insn &= ~0x1fffe0;
        insn |= (((val >> 32) & 0xffff) << 5);
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_MOVW_UABS_G3:
        insn = *(uint32_t *)loc;
        insn &= ~0x1fffe0;
        insn |= (((val >> 48) & 0xffff) << 5);
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_MOVW_SABS_G0:
        sval = (int64_t)val;
        insn = *(uint32_t *)loc;
        insn &= ~0x1fffe0;
        insn |= ((sval & 0xffff) << 5);
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_LD_PREL_LO19:
    case R_AARCH64_ADR_PREL_LO21:
        sval = (int64_t)(val - (uint64_t)loc);
        insn = *(uint32_t *)loc;
        insn &= ~0x7fffff;
        insn |= ((sval >> 2) & 0x7ffff) << 5;
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_ADR_PREL_PG_HI21:
        sval = (int64_t)((val & ~0xfff) - ((uint64_t)loc & ~0xfff));
        insn = *(uint32_t *)loc;
        insn &= ~0x60ffffe0;
        insn |= (((sval >> 12) & 0x1fffff) << 5);
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_ADD_ABS_LO12_NC:
    case R_AARCH64_LDST8_ABS_LO12_NC:
    case R_AARCH64_LDST16_ABS_LO12_NC:
    case R_AARCH64_LDST32_ABS_LO12_NC:
    case R_AARCH64_LDST64_ABS_LO12_NC:
    case R_AARCH64_LDST128_ABS_LO12_NC:
        insn = *(uint32_t *)loc;
        insn &= ~0x3ffc00;
        insn |= ((val & 0xfff) << 10);
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_JUMP26:
    case R_AARCH64_CALL26:
        sval = (int64_t)(val - (uint64_t)loc);
        insn = *(uint32_t *)loc;
        insn &= ~0x3ffffff;
        insn |= ((sval >> 2) & 0x3ffffff);
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_TSTBR14:
        sval = (int64_t)(val - (uint64_t)loc);
        insn = *(uint32_t *)loc;
        insn &= ~0x3ffe0;
        insn |= ((sval >> 2) & 0x3fff) << 5;
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_CONDBR19:
        sval = (int64_t)(val - (uint64_t)loc);
        insn = *(uint32_t *)loc;
        insn &= ~0x7ffff0;
        insn |= ((sval >> 2) & 0x7ffff) << 5;
        *(uint32_t *)loc = insn;
        break;
    case R_AARCH64_NULL:
        break;
    default:
        kite_printk("KITE: unsupported reloc type %lu\n", ELF64_R_TYPE(rela->r_info));
        return -1;
    }
    return 0;
}

static int apply_relocations(struct load_info *info, void *base)
{
    struct elf64_hdr *hdr = (struct elf64_hdr *)info->hdr;
    struct elf64_shdr *sechdrs = info->sechdrs;
    
    for (unsigned int i = 0; i < hdr->e_shnum; i++) {
        if (sechdrs[i].sh_type != SHT_RELA)
            continue;
        if (!(sechdrs[sechdrs[i].sh_info].sh_flags & SHF_ALLOC))
            continue;
        
        struct elf64_rela *relas = (struct elf64_rela *)((char *)info->hdr + sechdrs[i].sh_offset);
        unsigned int nrelas = sechdrs[i].sh_size / sizeof(struct elf64_rela);
        
        for (unsigned int j = 0; j < nrelas; j++) {
            unsigned int symidx = ELF64_R_SYM(relas[j].r_info);
            if (symidx >= info->nsyms) {
                kite_printk("KITE: bad symidx %u\n", symidx);
                return -1;
            }
            int ret = apply_relocation(info, base, &relas[j], &info->syms[symidx]);
            if (ret) return ret;
        }
    }
    return 0;
}

int loader_init(void)
{
    return 0;
}

void *loader_load_mem(void *mem, size_t size)
{
    struct load_info info = {0};
    info.hdr = mem;
    info.len = size;
    
    if (elf_header_check((struct elf64_hdr *)mem) != 0) {
        return NULL;
    }
    
    if (setup_load_info(&info) != 0) {
        return NULL;
    }
    
    size_t text_size, ro_size, total_size;
    if (layout_sections(&info, &text_size, &ro_size, &total_size) != 0) {
        return NULL;
    }
    
    if (!kite_rox_mem || !kite_rw_mem) {
        kite_printk("KITE: loader no mem pools\n");
        return NULL;
    }
    
    // Allocate executable memory
    void *base = tlsf_malloc(kite_rox_mem, total_size);
    if (!base) {
        kite_printk("KITE: loader alloc fail\n");
        return NULL;
    }
    
    // Copy sections
    struct elf64_hdr *hdr = (struct elf64_hdr *)info.hdr;
    for (unsigned int i = 0; i < hdr->e_shnum; i++) {
        if (info.sechdrs[i].sh_flags & SHF_ALLOC) {
            void *dest = (char *)base + info.sechdrs[i].sh_entsize;
            if (info.sechdrs[i].sh_type != SHT_NOBITS) {
                kite_memcpy(dest, (char *)info.hdr + info.sechdrs[i].sh_offset, info.sechdrs[i].sh_size);
            } else {
                kite_memset(dest, 0, info.sechdrs[i].sh_size);
            }
        }
    }
    
    // Resolve symbols
    if (simplify_symbols(&info, base) != 0) {
        tlsf_free(kite_rox_mem, base);
        return NULL;
    }
    
    // Apply relocations
    if (apply_relocations(&info, base) != 0) {
        tlsf_free(kite_rox_mem, base);
        return NULL;
    }
    
    flush_icache_all();
    
    kite_printk("KITE: module loaded at %p size=%zu\n", base, total_size);
    return base;
}

int loader_relocate(void *base, size_t size)
{
    (void)base;
    (void)size;
    return 0;
}

int loader_resolve_syms(void *base)
{
    (void)base;
    return 0;
}

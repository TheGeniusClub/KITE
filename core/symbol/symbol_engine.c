/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Symbol engine with cache stripped fallback B-follow
 * Reference from KernelPatch kallsyms and SKRoot
 */

#include <kite/ktypes.h>
#include "arch/arm64/setup.h"
#include "symbol_engine.h"
#include "stubs.h"
#include "memory/tlsf.h"

extern start_preset_t start_preset;
extern tlsf_t kite_rw_mem;

static unsigned long (*kallsyms_lookup_name)(const char *name) = 0;
static uint64_t kernel_base = 0;
static uint64_t kernel_size = 0;

// simple symbol cache
#define SYM_CACHE_BUCKETS 128

typedef struct sym_entry {
    struct sym_entry *next;
    uint64_t addr;
    char name[0];
} sym_entry_t;

static sym_entry_t *sym_cache[SYM_CACHE_BUCKETS];
static int sym_cache_inited = 0;

static uint32_t sym_hash(const char *name)
{
    uint32_t h = 0;
    while (*name) {
        h = h * 31 + (unsigned char)*name++;
    }
    return h % SYM_CACHE_BUCKETS;
}

static sym_entry_t *sym_cache_find(const char *name)
{
    uint32_t h = sym_hash(name);
    sym_entry_t *e = sym_cache[h];
    while (e) {
        if (kite_strcmp(e->name, name) == 0) {
            return e;
        }
        e = e->next;
    }
    return NULL;
}

static void sym_cache_add(const char *name, uint64_t addr)
{
    if (!kite_rw_mem || sym_cache_find(name)) return;
    
    size_t len = kite_strlen(name);
    sym_entry_t *e = tlsf_malloc(kite_rw_mem, sizeof(sym_entry_t) + len + 1);
    if (!e) return;
    
    e->addr = addr;
    kite_memcpy(e->name, name, len + 1);
    
    uint32_t h = sym_hash(name);
    e->next = sym_cache[h];
    sym_cache[h] = e;
}

// B instruction follow
static uint64_t follow_branch(uint64_t addr)
{
    uint32_t inst = *(uint32_t *)addr;
    if ((inst & 0xFC000000) == 0x14000000) {
        int64_t imm = (inst & 0x3FFFFFF) << 2;
        if (imm & 0x8000000) imm |= 0xFFFFFFFFF0000000ULL;
        return addr + imm;
    }
    return addr;
}

// stripped fallback - ARM64 prologue scanner
static uint64_t stripped_lookup(const char *name)
{
    if (!kernel_base || !kernel_size) return 0;
    
    // for now just scan for known patterns
    // todo: full string table scan + prologue match
    return 0;
}

int kite_symbol_init(void *kb, size_t ks)
{
    kernel_base = (uint64_t)kb;
    kernel_size = ks;
    
    if (!start_preset.kallsyms_lookup_name_offset) {
        kite_printk("KITE: no kallsyms_lookup_name offset\n");
        return -1;
    }
    
    kallsyms_lookup_name = (typeof(kallsyms_lookup_name))(kernel_base + start_preset.kallsyms_lookup_name_offset);
    
    if (!kallsyms_lookup_name) {
        kite_printk("KITE: kallsyms_lookup_name is null\n");
        return -1;
    }
    
    kite_printk("KITE: symbol engine init ok lookup=0x%llx\n", (uint64_t)kallsyms_lookup_name);
    sym_cache_inited = 1;
    return 0;
}

uint64_t symbol_lookup_name(const char *name)
{
    if (!name) return 0;
    
    // check cache first
    sym_entry_t *e = sym_cache_find(name);
    if (e) return e->addr;
    
    uint64_t addr = 0;
    
    if (kallsyms_lookup_name) {
        addr = kallsyms_lookup_name(name);
    }
    
    if (!addr) {
        addr = stripped_lookup(name);
    }
    
    if (addr) {
        addr = follow_branch(addr);
        if (sym_cache_inited) {
            sym_cache_add(name, addr);
        }
    }
    
    return addr;
}

uint64_t symbol_lookup_pattern(const char *pattern, size_t len)
{
    if (!kernel_base || !kernel_size || !pattern || !len) return 0;
    
    // scan kernel text for pattern
    uint8_t *start = (uint8_t *)kernel_base;
    uint8_t *end = start + kernel_size - len;
    
    for (uint8_t *p = start; p < end; p++) {
        if (kite_memcmp(p, pattern, len) == 0) {
            return (uint64_t)p;
        }
    }
    return 0;
}

int symbol_foreach(int (*cb)(const char *name, uint64_t addr, void *data), void *data)
{
    if (!cb) return -1;
    
    // iterate cache
    for (int i = 0; i < SYM_CACHE_BUCKETS; i++) {
        sym_entry_t *e = sym_cache[i];
        while (e) {
            int ret = cb(e->name, e->addr, data);
            if (ret) return ret;
            e = e->next;
        }
    }
    return 0;
}

// entropy verification - check if names look valid
int symbol_verify_entropy(const char *name)
{
    if (!name) return 0;
    
    size_t len = kite_strlen(name);
    if (len < 3 || len > 256) return 0;
    
    int unique = 0;
    int zeros = 0;
    uint8_t seen[256] = {0};
    
    for (size_t i = 0; i < len; i++) {
        uint8_t c = (uint8_t)name[i];
        if (!seen[c]) {
            seen[c] = 1;
            unique++;
        }
        if (c == 0) zeros++;
    }
    
    if (unique < 10) return 0;
    if (zeros * 100 / len > 15) return 0;
    
    return 1;
}

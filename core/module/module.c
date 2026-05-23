/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Module lifecycle management
 * Reference from KernelPatch module.c
 */

#include "module.h"
#include "loader.h"
#include "elf64.h"
#include "stubs.h"
#include "memory/tlsf.h"
#include "hook/hook_engine.h"
#include "symbol/symbol_engine.h"

extern tlsf_t kite_rw_mem;
extern tlsf_t kite_rox_mem;

// simple strncmp
static int kite_strncmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && *s1 == *s2) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) return 0;
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

#define KITE_MODULE_MAX 32

// module callback types
typedef long (*mod_initcall_t)(const char *args, const char *event, void *reserved);
typedef long (*mod_ctl0call_t)(const char *ctl_args, char *out_msg, int outlen);
typedef long (*mod_ctl1call_t)(void *a1, void *a2, void *a3);
typedef long (*mod_exitcall_t)(void *reserved);

typedef struct kite_module_entry {
    char name[KITE_MODULE_NAME_LEN];
    char version[KITE_MODULE_VERSION_LEN];
    char args[256];
    mod_initcall_t init;
    mod_ctl0call_t ctl0;
    mod_ctl1call_t ctl1;
    mod_exitcall_t exit;
    void *base;
    size_t size;
    int loaded;
    int refcnt;
    struct kite_module_entry *next;
} kite_module_entry_t;

static kite_module_entry_t *module_list_head = NULL;
static int module_count = 0;

static kite_module_entry_t *module_find_entry(const char *name)
{
    kite_module_entry_t *m = module_list_head;
    while (m) {
        if (kite_strcmp(m->name, name) == 0) {
            return m;
        }
        m = m->next;
    }
    return NULL;
}

kite_module_t *module_find(const char *name)
{
    kite_module_entry_t *e = module_find_entry(name);
    if (!e) return NULL;
    return (kite_module_t *)e;
}

static int extract_module_info(void *base, struct elf64_hdr *hdr, struct elf64_shdr *sechdrs, 
                                char *secstrings, kite_module_entry_t *mod)
{
    // Find .kite.info section
    for (unsigned int i = 0; i < hdr->e_shnum; i++) {
        if (kite_strcmp(secstrings + sechdrs[i].sh_name, ".kite.info") == 0) {
            char *info = (char *)base + sechdrs[i].sh_entsize;
            char *end = info + sechdrs[i].sh_size;
            char *p = info;
            
            while (p < end && *p) {
                if (kite_strncmp(p, "name=", 5) == 0) {
                    kite_strncpy(mod->name, p + 5, KITE_MODULE_NAME_LEN - 1);
                } else if (kite_strncmp(p, "version=", 8) == 0) {
                    kite_strncpy(mod->version, p + 8, KITE_MODULE_VERSION_LEN - 1);
                }
                // skip to next null-terminated string
                while (p < end && *p) p++;
                if (p < end) p++;
            }
            break;
        }
    }
    
    // Find callback sections
    for (unsigned int i = 0; i < hdr->e_shnum; i++) {
        if (kite_strcmp(secstrings + sechdrs[i].sh_name, ".kite.init") == 0) {
            mod->init = (mod_initcall_t)((char *)base + sechdrs[i].sh_entsize);
        } else if (kite_strcmp(secstrings + sechdrs[i].sh_name, ".kite.ctl0") == 0) {
            mod->ctl0 = (mod_ctl0call_t)((char *)base + sechdrs[i].sh_entsize);
        } else if (kite_strcmp(secstrings + sechdrs[i].sh_name, ".kite.ctl1") == 0) {
            mod->ctl1 = (mod_ctl1call_t)((char *)base + sechdrs[i].sh_entsize);
        } else if (kite_strcmp(secstrings + sechdrs[i].sh_name, ".kite.exit") == 0) {
            mod->exit = (mod_exitcall_t)((char *)base + sechdrs[i].sh_entsize);
        }
    }
    
    return 0;
}

int module_load(const char *path)
{
    (void)path;
    kite_printk("KITE: module_load not implemented use module_load_mem\n");
    return -1;
}

int module_load_mem(void *mem, size_t size, const char *args, const char *event, void *reserved)
{
    if (!kite_rw_mem) {
        kite_printk("KITE: module_load_mem no mem pool\n");
        return -1;
    }
    
    if (module_count >= KITE_MODULE_MAX) {
        kite_printk("KITE: module max reached\n");
        return -1;
    }
    
    void *base = loader_load_mem(mem, size);
    if (!base) {
        kite_printk("KITE: module_load_mem loader fail\n");
        return -1;
    }
    
    kite_module_entry_t *mod = tlsf_malloc(kite_rw_mem, sizeof(kite_module_entry_t));
    if (!mod) {
        kite_printk("KITE: module_load_mem alloc fail\n");
        return -1;
    }
    
    kite_memset(mod, 0, sizeof(kite_module_entry_t));
    mod->base = base;
    mod->size = size;
    
    struct elf64_hdr *hdr = (struct elf64_hdr *)mem;
    struct elf64_shdr *sechdrs = (struct elf64_shdr *)((char *)mem + hdr->e_shoff);
    char *secstrings = (char *)mem + sechdrs[hdr->e_shstrndx].sh_offset;
    
    extract_module_info(base, hdr, sechdrs, secstrings, mod);
    
    if (args) {
        kite_strncpy(mod->args, args, sizeof(mod->args) - 1);
    }
    
    // Call init
    if (mod->init) {
        long ret = mod->init(mod->args, event, reserved);
        if (ret != 0) {
            kite_printk("KITE: module %s init fail ret=%ld\n", mod->name, ret);
            if (mod->exit) {
                mod->exit(reserved);
            }
            tlsf_free(kite_rw_mem, mod);
            return -1;
        }
    }
    
    mod->loaded = 1;
    mod->refcnt = 1;
    mod->next = module_list_head;
    module_list_head = mod;
    module_count++;
    
    kite_printk("KITE: module %s loaded ok\n", mod->name);
    return 0;
}

int module_unload(const char *name)
{
    kite_module_entry_t **pp = &module_list_head;
    while (*pp) {
        kite_module_entry_t *m = *pp;
        if (kite_strcmp(m->name, name) == 0) {
            if (m->refcnt > 1) {
                kite_printk("KITE: module %s busy refcnt=%d\n", name, m->refcnt);
                return -1;
            }
            
            if (m->exit) {
                m->exit(NULL);
            }
            
            *pp = m->next;
            module_count--;
            
            if (m->base && kite_rox_mem) {
                tlsf_free(kite_rox_mem, m->base);
            }
            tlsf_free(kite_rw_mem, m);
            
            kite_printk("KITE: module %s unloaded\n", name);
            return 0;
        }
        pp = &m->next;
    }
    
    kite_printk("KITE: module %s not found\n", name);
    return -1;
}

int module_control0(const char *name, const char *ctl_args, char *out_msg, int outlen)
{
    kite_module_entry_t *m = module_find_entry(name);
    if (!m || !m->loaded) {
        kite_printk("KITE: module %s not loaded\n", name);
        return -1;
    }
    
    if (m->ctl0) {
        return m->ctl0(ctl_args, out_msg, outlen);
    }
    return -1;
}

int module_control1(const char *name, void *a1, void *a2, void *a3)
{
    kite_module_entry_t *m = module_find_entry(name);
    if (!m || !m->loaded) {
        kite_printk("KITE: module %s not loaded\n", name);
        return -1;
    }
    
    if (m->ctl1) {
        return m->ctl1(a1, a2, a3);
    }
    return -1;
}

void module_list(void)
{
    kite_printk("KITE: module list count=%d\n", module_count);
    kite_module_entry_t *m = module_list_head;
    while (m) {
        kite_printk("KITE:  %s v%s base=%p ref=%d\n", m->name, m->version, m->base, m->refcnt);
        m = m->next;
    }
}

int module_get_count(void)
{
    return module_count;
}

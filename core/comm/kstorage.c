/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * KIMC - Kernel Inter-Module Communication
 * Key-value storage for module state sharing
 */

#include "comm/kstorage.h"
#include "stubs.h"
#include "memory/tlsf.h"

extern tlsf_t kite_rw_mem;

#define KIMC_BUCKET_NUM 64
#define KIMC_KEY_MAX 128
#define KIMC_VAL_MAX 1024

typedef struct kimc_entry {
    struct kimc_entry *next;
    char key[KIMC_KEY_MAX];
    char val[KIMC_VAL_MAX];
    size_t val_len;
} kimc_entry_t;

static kimc_entry_t *kimc_buckets[KIMC_BUCKET_NUM];

static uint32_t kimc_hash(const char *key)
{
    uint32_t h = 0;
    while (*key) {
        h = h * 31 + (unsigned char)*key++;
    }
    return h % KIMC_BUCKET_NUM;
}

static kimc_entry_t *kimc_find(const char *key)
{
    uint32_t h = kimc_hash(key);
    kimc_entry_t *e = kimc_buckets[h];
    while (e) {
        if (kite_strcmp(e->key, key) == 0) {
            return e;
        }
        e = e->next;
    }
    return NULL;
}

int kstorage_set(const char *key, const void *val, size_t len)
{
    if (!key || !val || !len || len > KIMC_VAL_MAX) {
        return -1;
    }
    
    kimc_entry_t *e = kimc_find(key);
    if (e) {
        kite_memcpy(e->val, val, len);
        e->val_len = len;
        return 0;
    }
    
    if (!kite_rw_mem) return -1;
    e = tlsf_malloc(kite_rw_mem, sizeof(kimc_entry_t));
    if (!e) return -1;
    
    kite_strncpy(e->key, key, KIMC_KEY_MAX - 1);
    kite_memcpy(e->val, val, len);
    e->val_len = len;
    
    uint32_t h = kimc_hash(key);
    e->next = kimc_buckets[h];
    kimc_buckets[h] = e;
    return 0;
}

void *kstorage_get(const char *key, size_t *size)
{
    if (!key) return NULL;
    
    kimc_entry_t *e = kimc_find(key);
    if (!e) return NULL;
    
    if (size) *size = e->val_len;
    return e->val;
}

int kstorage_del(const char *key)
{
    if (!key) return -1;
    
    uint32_t h = kimc_hash(key);
    kimc_entry_t **pp = &kimc_buckets[h];
    while (*pp) {
        kimc_entry_t *e = *pp;
        if (kite_strcmp(e->key, key) == 0) {
            *pp = e->next;
            if (kite_rw_mem) tlsf_free(kite_rw_mem, e);
            return 0;
        }
        pp = &e->next;
    }
    return -1;
}

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Task local storage - per-task key-value data
 */

#include "comm/task_local.h"
#include "stubs.h"
#include "memory/tlsf.h"

extern tlsf_t kite_rw_mem;

#define TASK_LOCAL_BUCKET 32
#define TASK_LOCAL_KEY_MAX 64
#define TASK_LOCAL_VAL_MAX 512
#define TASK_LOCAL_MAX_ENTRIES 256

typedef struct task_local_entry {
    struct task_local_entry *next;
    u32 pid;
    char key[TASK_LOCAL_KEY_MAX];
    char val[TASK_LOCAL_VAL_MAX];
    size_t val_len;
} task_local_entry_t;

static task_local_entry_t *task_local_buckets[TASK_LOCAL_BUCKET];
static int task_local_count = 0;

static uint32_t task_local_hash(u32 pid, const char *key)
{
    uint32_t h = pid * 31;
    while (*key) {
        h = h * 31 + (unsigned char)*key++;
    }
    return h % TASK_LOCAL_BUCKET;
}

static task_local_entry_t *task_local_find(u32 pid, const char *key)
{
    uint32_t h = task_local_hash(pid, key);
    task_local_entry_t *e = task_local_buckets[h];
    while (e) {
        if (e->pid == pid && kite_strcmp(e->key, key) == 0) {
            return e;
        }
        e = e->next;
    }
    return NULL;
}

int task_local_set(u32 pid, const char *key, void *value, size_t size)
{
    if (!key || !value || !size || size > TASK_LOCAL_VAL_MAX) {
        return -1;
    }
    
    task_local_entry_t *e = task_local_find(pid, key);
    if (e) {
        kite_memcpy(e->val, value, size);
        e->val_len = size;
        return 0;
    }
    
    if (task_local_count >= TASK_LOCAL_MAX_ENTRIES || !kite_rw_mem) {
        return -1;
    }
    
    e = tlsf_malloc(kite_rw_mem, sizeof(task_local_entry_t));
    if (!e) return -1;
    
    e->pid = pid;
    kite_strncpy(e->key, key, TASK_LOCAL_KEY_MAX - 1);
    kite_memcpy(e->val, value, size);
    e->val_len = size;
    
    uint32_t h = task_local_hash(pid, key);
    e->next = task_local_buckets[h];
    task_local_buckets[h] = e;
    task_local_count++;
    return 0;
}

void *task_local_get(u32 pid, const char *key, size_t *size)
{
    if (!key) return NULL;
    
    task_local_entry_t *e = task_local_find(pid, key);
    if (!e) return NULL;
    
    if (size) *size = e->val_len;
    return e->val;
}

int task_local_del(u32 pid, const char *key)
{
    if (!key) return -1;
    
    uint32_t h = task_local_hash(pid, key);
    task_local_entry_t **pp = &task_local_buckets[h];
    while (*pp) {
        task_local_entry_t *e = *pp;
        if (e->pid == pid && kite_strcmp(e->key, key) == 0) {
            *pp = e->next;
            if (kite_rw_mem) tlsf_free(kite_rw_mem, e);
            task_local_count--;
            return 0;
        }
        pp = &e->next;
    }
    return -1;
}

void task_local_clear(u32 pid)
{
    for (int i = 0; i < TASK_LOCAL_BUCKET; i++) {
        task_local_entry_t **pp = &task_local_buckets[i];
        while (*pp) {
            task_local_entry_t *e = *pp;
            if (e->pid == pid) {
                *pp = e->next;
                if (kite_rw_mem) tlsf_free(kite_rw_mem, e);
                task_local_count--;
            } else {
                pp = &e->next;
            }
        }
    }
}

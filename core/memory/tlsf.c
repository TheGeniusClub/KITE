/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch tlsf.c
 */

#include "tlsf.h"

#define TLSF_MAX_LOG2_SLI 5
#define TLSF_MAX_SIZE (1 << 30)
#define TLSF_BLOCK_HDR_SIZE 8
#define TLSF_FLI_OFFSET 7
#define TLSF_SLI 4

#define block_size(b) ((b)->size & ~0x1)
#define block_set_size(b, s) (b)->size = ((b)->size & 0x1) | (s)
#define block_is_free(b) (!((b)->size & 0x1))
#define block_set_free(b, f) (b)->size = ((b)->size & ~0x1) | (!(f))
#define block_from_ptr(p) ((block_t *)((char *)(p) - TLSF_BLOCK_HDR_SIZE))
#define ptr_from_block(b) ((void *)((char *)(b) + TLSF_BLOCK_HDR_SIZE))
#define next_block(b) ((block_t *)((char *)(b) + block_size(b) + TLSF_BLOCK_HDR_SIZE))
#define prev_block(b) ((block_t *)((char *)(b) - ((b)->prev_size & ~0x1) - TLSF_BLOCK_HDR_SIZE))

typedef struct block {
    size_t size;
    size_t prev_size;
    struct block *next;
    struct block *prev;
} block_t;

struct tlsf {
    size_t total_size;
    size_t used_size;
    size_t fli;
    size_t sli;
    block_t *blocks[TLSF_MAX_LOG2_SLI][1 << TLSF_SLI];
};

static inline size_t tlsf_fli(size_t size)
{
    size_t fli = 0;
    while (size > 1) {
        size >>= 1;
        fli++;
    }
    return fli > TLSF_FLI_OFFSET ? fli - TLSF_FLI_OFFSET : 0;
}

static inline size_t tlsf_sli(size_t size, size_t fli)
{
    if (fli == 0) return size >> 3;
    return (size >> (fli + TLSF_FLI_OFFSET - TLSF_SLI)) & ((1 << TLSF_SLI) - 1);
}

static inline void remove_block(tlsf_t tlsf, block_t *block)
{
    struct tlsf *t = (struct tlsf *)tlsf;
    size_t fli = tlsf_fli(block_size(block));
    size_t sli = tlsf_sli(block_size(block), fli);
    
    if (block->next) block->next->prev = block->prev;
    if (block->prev) block->prev->next = block->next;
    else t->blocks[fli][sli] = block->next;
}

static inline void insert_block(tlsf_t tlsf, block_t *block)
{
    struct tlsf *t = (struct tlsf *)tlsf;
    size_t fli = tlsf_fli(block_size(block));
    size_t sli = tlsf_sli(block_size(block), fli);
    
    block->prev = NULL;
    block->next = t->blocks[fli][sli];
    if (t->blocks[fli][sli]) t->blocks[fli][sli]->prev = block;
    t->blocks[fli][sli] = block;
}

static inline block_t *find_suitable_block(tlsf_t tlsf, size_t size)
{
    struct tlsf *t = (struct tlsf *)tlsf;
    size_t fli = tlsf_fli(size);
    size_t sli = tlsf_sli(size, fli);
    
    for (size_t f = fli; f < t->fli; f++) {
        for (size_t s = (f == fli ? sli : 0); s < (1 << t->sli); s++) {
            if (t->blocks[f][s]) return t->blocks[f][s];
        }
    }
    return NULL;
}

static inline void split_block(tlsf_t tlsf, block_t *block, size_t size)
{
    size_t remain = block_size(block) - size - TLSF_BLOCK_HDR_SIZE;
    if (remain < 8) return;
    
    block_set_size(block, size);
    block_t *next = next_block(block);
    block_set_size(next, remain);
    block_set_free(next, 1);
    next->prev_size = size;
    block_t *next2 = next_block(next);
    next2->prev_size = remain;
    insert_block(tlsf, next);
}

tlsf_t tlsf_create(void *mem, size_t bytes)
{
    if (!mem || bytes < sizeof(struct tlsf) + TLSF_BLOCK_HDR_SIZE + 8) return NULL;
    
    struct tlsf *t = (struct tlsf *)mem;
    t->total_size = bytes;
    t->used_size = sizeof(struct tlsf);
    t->fli = TLSF_MAX_LOG2_SLI;
    t->sli = TLSF_SLI;
    
    for (size_t i = 0; i < TLSF_MAX_LOG2_SLI; i++) {
        for (size_t j = 0; j < (1 << TLSF_SLI); j++) {
            t->blocks[i][j] = NULL;
        }
    }
    
    block_t *block = (block_t *)((char *)mem + sizeof(struct tlsf));
    block_set_size(block, bytes - sizeof(struct tlsf) - TLSF_BLOCK_HDR_SIZE);
    block_set_free(block, 1);
    block->prev_size = 0;
    
    block_t *end = next_block(block);
    block_set_size(end, 0);
    block_set_free(end, 0);
    end->prev_size = block_size(block);
    
    insert_block(t, block);
    return t;
}

void tlsf_destroy(tlsf_t tlsf)
{
    // nothing to do
}

void *tlsf_malloc(tlsf_t tlsf, size_t size)
{
    if (!tlsf || size == 0 || size > TLSF_MAX_SIZE) return NULL;
    
    size = (size + 7) & ~7;
    block_t *block = find_suitable_block(tlsf, size);
    if (!block) return NULL;
    
    remove_block(tlsf, block);
    split_block(tlsf, block, size);
    block_set_free(block, 0);
    
    ((struct tlsf *)tlsf)->used_size += block_size(block) + TLSF_BLOCK_HDR_SIZE;
    return ptr_from_block(block);
}

void *tlsf_memalign(tlsf_t tlsf, size_t align, size_t size)
{
    if (!tlsf || align < 8) return tlsf_malloc(tlsf, size);
    
    void *ptr = tlsf_malloc(tlsf, size + align + TLSF_BLOCK_HDR_SIZE);
    if (!ptr) return NULL;
    
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t aligned = (addr + align - 1) & ~(align - 1);
    
    if (aligned != addr) {
        block_t *block = block_from_ptr(ptr);
        size_t old_size = block_size(block);
        size_t offset = aligned - addr;
        
        block_set_size(block, offset - TLSF_BLOCK_HDR_SIZE);
        block_set_free(block, 1);
        
        block_t *new_block = (block_t *)((char *)block + offset);
        block_set_size(new_block, old_size - offset);
        block_set_free(new_block, 0);
        new_block->prev_size = block_size(block);
        
        block_t *next = next_block(new_block);
        next->prev_size = block_size(new_block);
        
        insert_block(tlsf, block);
        ptr = ptr_from_block(new_block);
    }
    
    return ptr;
}

void tlsf_free(tlsf_t tlsf, void *ptr)
{
    if (!tlsf || !ptr) return;
    
    block_t *block = block_from_ptr(ptr);
    if (block_is_free(block)) return;
    
    block_set_free(block, 1);
    ((struct tlsf *)tlsf)->used_size -= block_size(block) + TLSF_BLOCK_HDR_SIZE;
    
    // merge with next
    block_t *next = next_block(block);
    if (block_is_free(next)) {
        remove_block(tlsf, next);
        block_set_size(block, block_size(block) + block_size(next) + TLSF_BLOCK_HDR_SIZE);
        block_t *next2 = next_block(block);
        next2->prev_size = block_size(block);
    }
    
    // merge with prev
    if (block->prev_size & ~0x1) {
        block_t *prev = prev_block(block);
        if (block_is_free(prev)) {
            remove_block(tlsf, prev);
            block_set_size(prev, block_size(prev) + block_size(block) + TLSF_BLOCK_HDR_SIZE);
            block_t *next2 = next_block(prev);
            next2->prev_size = block_size(prev);
            block = prev;
        }
    }
    
    insert_block(tlsf, block);
}

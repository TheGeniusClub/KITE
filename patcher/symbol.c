/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch tools/symbol.c
 */

#include "patcher.h"

struct on_each_symbol_struct
{
    const char *symbol;
    uint64_t addr;
};

static int32_t on_each_symbol_callbackup(int32_t index, char type, const char *symbol, int32_t offset, void *userdata)
{
    struct on_each_symbol_struct *data = (struct on_each_symbol_struct *)userdata;
    int len = strlen(data->symbol);
    if (strstr(symbol, data->symbol) == symbol && (symbol[len] == '.' || symbol[len] == '$') &&
        !strstr(symbol, ".cfi_jt")) {
        tools_logi("%s -> %s: type: %c, offset: 0x%08x\n", data->symbol, symbol, type, offset);
        data->addr = offset;
        return 1;
    }
    return 0;
}

int32_t find_suffixed_symbol(kallsym_t *kallsym, char *img_buf, const char *symbol)
{
    struct on_each_symbol_struct udata = { symbol, 0 };
    on_each_symbol(kallsym, img_buf, &udata, on_each_symbol_callbackup);
    return udata.addr;
}

int32_t get_symbol_offset_zero(kallsym_t *info, char *img, char *symbol)
{
    int32_t offset = get_symbol_offset(info, img, symbol);
    return offset > 0 ? offset : 0;
}

int32_t get_symbol_offset_exit(kallsym_t *info, char *img, char *symbol)
{
    int32_t offset = get_symbol_offset(info, img, symbol);
    if (offset >= 0) {
        return offset;
    } else {
        tools_loge_exit("no symbol %s\n", symbol);
    }
}

int32_t try_get_symbol_offset_zero(kallsym_t *info, char *img, char *symbol)
{
    int32_t offset = get_symbol_offset(info, img, symbol);
    if (offset > 0) return offset;
    return find_suffixed_symbol(info, img, symbol);
}

static int32_t get_map_anchor_offset(kallsym_t *kallsym, char *img_buf, int imglen, const char **selected)
{
    static const char *map_anchor_candidates[] = {
        "tcp_init_sock",
        "udp_init_sock",
        "inet_create",
        "inet_release",
        "sock_init_data",
        "sk_alloc",
        "input_handle_event",
        "slow_avc_audit",
        "avc_denied",
        "nmi_panic",
        "panic",
        "kern_addr_valid",
        "set_memory_rw",
        "set_memory_ro",
        "free_initmem",
    };
    int cand_num = sizeof(map_anchor_candidates) / sizeof(map_anchor_candidates[0]);
    int32_t offset = 0;
    for (int i = 0; i < cand_num; i++) {
        offset = get_symbol_offset_zero(kallsym, img_buf, (char *)map_anchor_candidates[i]);
        if (offset) {
            *selected = map_anchor_candidates[i];
            tools_logi("map anchor: %s, offset: 0x%08x\n", map_anchor_candidates[i], offset);
            return offset;
        }
    }
    return 0;
}

int32_t get_map_anchor_addr(kallsym_t *kallsym, char *image_buf, int imglen)
{
    const char *selected = NULL;
    return get_map_anchor_offset(kallsym, image_buf, imglen, &selected);
}

void select_map_area(kallsym_t *kallsym, char *image_buf, int imglen, int32_t *map_start, int32_t *max_size, bool is_gki)
{
    const char *selected = NULL;
    int32_t addr = get_map_anchor_offset(kallsym, image_buf, imglen, &selected);
    if (!addr) {
        tools_loge_exit("no usable map anchor symbol\n");
    }

    if (!is_gki) {
        *map_start = align_ceil(addr, 16);
    } else {
        *map_start = align_floor(addr, 16);
    }
    *max_size = 0x800;

#define NOP 0xD503201F
#define PAC 0xd503233f
#define AUT 0xd50323bf
#define PAC_MASK 0xFFFFFD1F
#define PAC_PATTERN 0xD503211F

    uint32_t pos = 0;
    uint32_t count = 0;
    uint32_t asmbit = sizeof(uint32_t);
    bool is_first_pac = false;
    for (uint32_t i = 0; i < *max_size; i += asmbit) {
        uint32_t insn = *(uint32_t *)(image_buf + addr + i);
        if (!is_first_pac && insn == PAC && i < asmbit * 5) {
            is_first_pac = true;
        }
        if ((insn & 0xFFFFFD1F) == 0xD503211F) {
            pos = i;
            count++;
            *(uint32_t *)(image_buf + addr + pos) = NOP;
        }
    }

    if (!is_first_pac) {
        tools_logi("no first pac instruction found \n");
    }

    if (count % 2 != 0) {
        tools_logi("pac verify not pair  pos: %x  count: %d\n", pos, count);

        uint32_t second_pos = 0;
        for (uint32_t j = *max_size; j < *max_size * 2; j += asmbit) {
            uint32_t insn = *(uint32_t *)(image_buf + addr + j);
            if ((insn & 0xFFFFFD1F) == 0xD503211F) {
                second_pos = j;
                break;
            }
        }
        tools_logi("second_pos: %x \n", second_pos);
        *(uint32_t *)(image_buf + addr + second_pos) = NOP;
    }

#undef NOP
#undef PAC
#undef AUT
#undef PAC_MASK
#undef PAC_PATTERN
}

int fillin_map_symbol(kallsym_t *kallsym, char *img_buf, map_symbol_t *symbol, int32_t target_is_be)
{
    symbol->memblock_reserve_relo = get_symbol_offset_exit(kallsym, img_buf, "memblock_reserve");
    symbol->memblock_free_relo = get_symbol_offset_exit(kallsym, img_buf, "memblock_free");

    symbol->memblock_mark_nomap_relo = get_symbol_offset_zero(kallsym, img_buf, "memblock_mark_nomap");

    symbol->memblock_phys_alloc_relo = get_symbol_offset_zero(kallsym, img_buf, "memblock_phys_alloc_try_nid");
    if (symbol->memblock_phys_alloc_relo) {
        symbol->memblock_phys_alloc_type = MAP_SYM_MEMBLOCK_PHYS_ALLOC_TRY_NID;
    }

    symbol->memblock_virt_alloc_relo = get_symbol_offset_zero(kallsym, img_buf, "memblock_virt_alloc_try_nid");
    if (symbol->memblock_virt_alloc_relo) {
        symbol->memblock_virt_alloc_type = MAP_SYM_MEMBLOCK_VIRT_ALLOC_TRY_NID;
    }

    uint64_t memblock_alloc_try_nid = get_symbol_offset_zero(kallsym, img_buf, "memblock_alloc_try_nid");
    
    if (!symbol->memblock_phys_alloc_relo) {
        uint64_t memblock_phys_alloc = get_symbol_offset_zero(kallsym, img_buf, "memblock_phys_alloc");
        if (memblock_phys_alloc) {
            symbol->memblock_phys_alloc_relo = memblock_phys_alloc;
            symbol->memblock_phys_alloc_type = MAP_SYM_MEMBLOCK_PHYS_ALLOC_TRY_NID;
            tools_logi("use memblock_phys_alloc as map phys alloc\n");
        } else if (memblock_alloc_try_nid) {
            symbol->memblock_phys_alloc_relo = memblock_alloc_try_nid;
            symbol->memblock_phys_alloc_type = MAP_SYM_MEMBLOCK_ALLOC_TRY_NID;
        }
    }
    
    if (!symbol->memblock_virt_alloc_relo && memblock_alloc_try_nid) {
        symbol->memblock_virt_alloc_relo = memblock_alloc_try_nid;
        symbol->memblock_virt_alloc_type = MAP_SYM_MEMBLOCK_VIRT_ALLOC_FROM_ALLOC_TRY_NID;
    }

    if (!symbol->memblock_phys_alloc_relo) {
        tools_loge_exit("no symbol memblock_phys_alloc_try_nid or memblock_alloc_try_nid\n");
    }
    if (!symbol->memblock_virt_alloc_relo) {
        tools_loge_exit("no symbol memblock_virt_alloc_try_nid or memblock_alloc_try_nid\n");
    }

    if (symbol->memblock_phys_alloc_type == MAP_SYM_MEMBLOCK_ALLOC_TRY_NID) {
        tools_logi("use memblock_alloc_try_nid as map phys alloc\n");
    }

    if ((is_be() ^ target_is_be)) {
        for (int64_t *pos = (int64_t *)symbol; pos < (int64_t *)((char *)symbol + sizeof(*symbol)); pos++) {
            *pos = i64swp(*pos);
        }
    }
    return 0;
}

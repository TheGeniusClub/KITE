/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch tools/patch.c
 */

#define _GNU_SOURCE
#define __USE_GNU

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "patcher.h"

static void hexstr_to_bytes(const char *hexstr, size_t out_len, unsigned char *out)
{
    for (size_t i = 0; i < out_len; i++) {
        char tmp[3] = { hexstr[i * 2], hexstr[i * 2 + 1], 0 };
        out[i] = (unsigned char)strtoul(tmp, NULL, 16);
    }
}

static int hex_patch(char *img, size_t imglen,
                     const char *pattern_hex,
                     const char *replace_hex)
{
    size_t patternlen = strlen(pattern_hex) / 2;
    size_t replacelen = strlen(replace_hex) / 2;

    unsigned char pattern[32];
    unsigned char replace[32];

    hexstr_to_bytes(pattern_hex, patternlen, pattern);
    hexstr_to_bytes(replace_hex, replacelen, replace);

    unsigned char *p = memmem(img, imglen, pattern, patternlen);
    if (p) {
        memcpy(p, replace, replacelen);
    } else {
        return -1;
    }
    return 0;
}

static int disable_pi_map(char *img, size_t imglen)
{
    return hex_patch(
        img,
        imglen,
        "E60316AAE7031F2A3411889A",
        "E60316AAE7031F2AF40309AA"
    );
}

void read_kernel_file(const char *path, kernel_file_t *kernel_file)
{
    int img_offset = 0;
    read_file(path, &kernel_file->kfile, &kernel_file->kfile_len);
    kernel_file->is_uncompressed_img = kernel_file->kfile_len >= 20 &&
                                       !strncmp("UNCOMPRESSED_IMG", kernel_file->kfile, 16);
    if (kernel_file->is_uncompressed_img) img_offset = 20;
    kernel_file->kimg = kernel_file->kfile + img_offset;
    kernel_file->kimg_len = kernel_file->kfile_len - img_offset;
}

void update_kernel_file_img_len(kernel_file_t *kernel_file, int kimg_len, bool is_different_endian)
{
    kernel_file->kimg_len = kimg_len;
    if (kernel_file->is_uncompressed_img) {
        *(uint32_t *)(kernel_file->kfile + 16) = (uint32_t)(is_different_endian ? i32swp(kimg_len) : kimg_len);
        kernel_file->kfile_len = kimg_len + 20;
    } else {
        kernel_file->kfile_len = kimg_len;
    }
}

void new_kernel_file(kernel_file_t *kernel_file, kernel_file_t *old, int kimg_len, bool is_different_endian)
{
    int prefix_len = old->kimg - old->kfile;
    int new_len = kimg_len + prefix_len;
    kernel_file->kfile = (char *)malloc(new_len);
    kernel_file->kimg = kernel_file->kfile + prefix_len;
    memcpy(kernel_file->kfile, old->kfile, prefix_len);
    kernel_file->is_uncompressed_img = old->is_uncompressed_img;
    update_kernel_file_img_len(kernel_file, kimg_len, is_different_endian);
}

void write_kernel_file(kernel_file_t *kernel_file, const char *path)
{
    write_file(path, kernel_file->kfile, kernel_file->kfile_len, false);
}

void free_kernel_file(kernel_file_t *kernel_file)
{
    free(kernel_file->kfile);
    kernel_file->kfile = NULL;
    kernel_file->kimg = NULL;
}

kite_preset_t *get_preset(const char *kimg, int kimg_len)
{
    char magic[KITE_MAGIC_LEN] = KITE_MAGIC;
    return (kite_preset_t *)kite_memmem(kimg, kimg_len, magic, sizeof(magic));
}

typedef struct
{
    const char *kimg;
    int32_t kimg_len;
    int32_t ori_kimg_len;
    const char *banner;
    kernel_info_t kinfo;
    kite_preset_t *preset;
} patched_kimg_t;

static int parse_image_patch_info(const char *kimg, int kimg_len, patched_kimg_t *pimg)
{
    pimg->kimg = kimg;
    pimg->kimg_len = kimg_len;

    kernel_info_t *kinfo = &pimg->kinfo;
    if (get_kernel_info(kinfo, kimg, kimg_len)) tools_loge_exit("get_kernel_info error\n");

    char linux_banner_prefix[] = "Linux version ";
    size_t prefix_len = strlen(linux_banner_prefix);
    const char *imgend = pimg->kimg + pimg->kimg_len;
    const char *banner = (char *)pimg->kimg;
    while ((banner = (char *)kite_memmem(banner + 1, imgend - banner, linux_banner_prefix, prefix_len)) != NULL) {
        if (isdigit(*(banner + prefix_len)) && *(banner + prefix_len + 1) == '.') {
            pimg->banner = banner;
            break;
        }
    }
    if (!pimg->banner) tools_loge_exit("can't find linux banner\n");

    kite_preset_t *old_preset = NULL;
    const char *search_ptr = kimg;
    int search_len = kimg_len;
    int32_t saved_kimg_len = 0;
    int align_kimg_len = 0;

    while (search_len > 0) {
        old_preset = get_preset(search_ptr, search_len);
        if (!old_preset) break;

        saved_kimg_len = old_preset->setup.kimg_size;
        if (is_be() ^ kinfo->is_be) saved_kimg_len = i32swp(saved_kimg_len);

        align_kimg_len = (char *)old_preset - kimg;
        if (align_kimg_len == (int)align_ceil(saved_kimg_len, SZ_4K)) {
            break;
        }

        tools_logw("found magic string at 0x%x but saved kernel image size mismatch, ignoring (false positive?)\n",
                   align_kimg_len);

        search_ptr = (char *)old_preset + 1;
        search_len = kimg_len - (search_ptr - kimg);
        old_preset = NULL;
    }

    pimg->preset = old_preset;

    if (!old_preset) {
        tools_logi("new kernel image ...\n");
        pimg->ori_kimg_len = pimg->kimg_len;
        return 0;
    }

    tools_logi("patched kernel image ...\n");
    pimg->ori_kimg_len = saved_kimg_len;

    memcpy((char *)kimg, old_preset->setup.header_backup, sizeof(old_preset->setup.header_backup));

    return 0;
}

int patch_kernel(const char *kimg_path, const char *kiteimg_path, const char *out_path, const char *superkey)
{
    set_log_enable(true);

    if (!kimg_path) tools_loge_exit("empty kernel image\n");
    if (!kiteimg_path) tools_loge_exit("empty kite image\n");
    if (!out_path) tools_loge_exit("empty out image path\n");
    if (!superkey) tools_loge_exit("empty superkey\n");

    patched_kimg_t pimg = { 0 };
    kernel_file_t kernel_file;
    read_kernel_file(kimg_path, &kernel_file);
    if (kernel_file.is_uncompressed_img) tools_logw("kernel image with UNCOMPRESSED_IMG header\n");

    int rc = parse_image_patch_info(kernel_file.kimg, kernel_file.kimg_len, &pimg);
    if (rc) tools_loge_exit("parse kernel image error\n");

    kernel_info_t *kinfo = &pimg.kinfo;
    int align_kernel_size = align_ceil(kinfo->kernel_size, SZ_4K);

    char *kallsym_kimg = (char *)malloc(pimg.ori_kimg_len);
    memcpy(kallsym_kimg, pimg.kimg, pimg.ori_kimg_len);
    kallsym_t kallsym = { 0 };
    int kver = 0;
    find_linux_banner(&kallsym, kallsym_kimg, pimg.ori_kimg_len, &kver);
    bool is_gki = kver >= 330240;
    tools_logi("is_gki: %s\n", is_gki ? "true" : "false");

    if (kver > 395008) {  // > 6.12.23
        if (disable_pi_map(kernel_file.kimg, kernel_file.kimg_len)) {
            tools_logi("kernel have patched or not found\n");
        } else {
            tools_logi("disabled PI_MAP for kernel version > 6.12.23\n");
        }
    }

    if (analyze_kallsym_info(&kallsym, kallsym_kimg, pimg.ori_kimg_len, ARM64, 1)) {
        tools_loge_exit("analyze_kallsym_info error\n");
    }

    char *kiteimg = NULL;
    int kiteimg_len = 0;
    read_file_align(kiteimg_path, &kiteimg, &kiteimg_len, 0x10);

    int ori_kimg_len = pimg.ori_kimg_len;
    int align_kimg_len = align_ceil(ori_kimg_len, SZ_4K);
    int out_img_len = align_kimg_len + kiteimg_len;
    int out_all_len = out_img_len;

    int start_offset = align_kernel_size;
    if (out_all_len > start_offset) {
        start_offset = align_ceil(out_all_len, SZ_4K);
        tools_logi("patch overlap, move start from 0x%x to 0x%x\n", align_kernel_size, start_offset);
    }
    tools_logi("layout kimg: 0x0,0x%x, kiteimg: 0x%x,0x%x, end: 0x%x, start: 0x%x\n", ori_kimg_len,
               align_kimg_len, kiteimg_len, out_all_len, start_offset);

    kernel_file_t out_kernel_file;
    new_kernel_file(&out_kernel_file, &kernel_file, out_all_len, (bool)(is_be() ^ kinfo->is_be));
    memcpy(out_kernel_file.kimg, pimg.kimg, ori_kimg_len);
    memset(out_kernel_file.kimg + ori_kimg_len, 0, align_kimg_len - ori_kimg_len);
    memcpy(out_kernel_file.kimg + align_kimg_len, kiteimg, kiteimg_len);

    kite_preset_t *preset = (kite_preset_t *)(out_kernel_file.kimg + align_kimg_len);
    memset(preset, 0, sizeof(kite_preset_t));
    memcpy(preset->header.magic, KITE_MAGIC, KITE_MAGIC_LEN);

    setup_preset_t *setup = &preset->setup;

    setup->kernel_version = (kallsym.version.major << 16) + (kallsym.version.minor << 8) + kallsym.version.patch;
    setup->kimg_size = ori_kimg_len;
    setup->kiteimg_size = kiteimg_len;

    setup->kernel_size = kinfo->kernel_size;
    setup->page_shift = kinfo->page_shift;
    setup->setup_offset = align_kimg_len;
    setup->start_offset = start_offset;
    setup->extra_size = 0;

    int map_start, map_max_size;
    select_map_area(&kallsym, kallsym_kimg, ori_kimg_len, &map_start, &map_max_size, is_gki);
    setup->map_offset = map_start;
    setup->map_max_size = map_max_size;
    tools_logi("map_start: 0x%x, max_size: 0x%x\n", map_start, map_max_size);

    int32_t sync_start = get_map_anchor_addr(&kallsym, kallsym_kimg, ori_kimg_len);
    if (!sync_start) sync_start = map_start;
    int sync_size = map_max_size * 2;
    if (sync_start + sync_size > ori_kimg_len) {
        sync_size = ori_kimg_len - sync_start;
    }
    if (sync_size > 0) {
        memcpy(out_kernel_file.kimg + sync_start, kallsym_kimg + sync_start, sync_size);
        tools_logi("Synced NOP modifications from kallsym_kimg to output file (offset: 0x%x, size: 0x%x)\n",
                   sync_start, sync_size);
    }

    setup->kallsyms_lookup_name_offset = get_symbol_offset_exit(&kallsym, kallsym_kimg, "kallsyms_lookup_name");

    setup->printk_offset = get_symbol_offset_zero(&kallsym, kallsym_kimg, "printk");
    if (!setup->printk_offset) setup->printk_offset = get_symbol_offset_zero(&kallsym, kallsym_kimg, "_printk");
    if (!setup->printk_offset) tools_loge_exit("no symbol printk\n");

    int paging_init_offset = get_symbol_offset_exit(&kallsym, kallsym_kimg, "paging_init");
    setup->paging_init_offset = relo_branch_func(kallsym_kimg, paging_init_offset);

    if ((is_be() ^ kinfo->is_be)) {
        setup->kimg_size = i64swp(setup->kimg_size);
        setup->kernel_size = i64swp(setup->kernel_size);
        setup->page_shift = i64swp(setup->page_shift);
        setup->setup_offset = i64swp(setup->setup_offset);
        setup->start_offset = i64swp(setup->start_offset);
        setup->extra_size = i64swp(setup->extra_size);
        setup->map_offset = i64swp(setup->map_offset);
        setup->map_max_size = i64swp(setup->map_max_size);
        setup->kallsyms_lookup_name_offset = i64swp(setup->kallsyms_lookup_name_offset);
        setup->paging_init_offset = i64swp(setup->paging_init_offset);
        setup->printk_offset = i64swp(setup->printk_offset);
    }

    fillin_map_symbol(&kallsym, kallsym_kimg, &setup->map_symbol, kinfo->is_be);

    memcpy(setup->header_backup, kallsym_kimg, sizeof(setup->header_backup));

    strncpy((char *)setup->superkey, superkey, SUPER_KEY_LEN - 1);

    int text_offset = align_kimg_len + SZ_4K;
    b((uint32_t *)(out_kernel_file.kimg + kinfo->b_stext_insn_offset), kinfo->b_stext_insn_offset, text_offset);

    write_kernel_file(&out_kernel_file, out_path);

    free(kallsym_kimg);
    free(kiteimg);
    free_kernel_file(&out_kernel_file);
    free_kernel_file(&kernel_file);

    tools_logi("patch done: %s\n", out_path);

    set_log_enable(false);
    return 0;
}

int unpatch_kernel(const char *kimg_path, const char *out_path)
{
    if (!kimg_path) tools_loge_exit("empty kernel image\n");
    if (!out_path) tools_loge_exit("empty out image path\n");

    kernel_file_t kernel_file;
    read_kernel_file(kimg_path, &kernel_file);

    kite_preset_t *preset = get_preset(kernel_file.kimg, kernel_file.kimg_len);
    if (!preset) tools_loge_exit("not patched kernel image\n");

    memcpy(kernel_file.kimg, preset->setup.header_backup, sizeof(preset->setup.header_backup));
    int kimg_size = preset->setup.kimg_size ?: ((char *)preset - kernel_file.kimg);
    update_kernel_file_img_len(&kernel_file, kimg_size, false);

    write_kernel_file(&kernel_file, out_path);
    free_kernel_file(&kernel_file);
    return 0;
}

int dump_kallsym(const char *kimg_path)
{
    if (!kimg_path) tools_loge_exit("empty kernel image\n");
    set_log_enable(true);

    kernel_file_t kernel_file;
    read_kernel_file(kimg_path, &kernel_file);

    kallsym_t kallsym;
    if (analyze_kallsym_info(&kallsym, kernel_file.kimg, kernel_file.kimg_len, ARM64, 1)) {
        fprintf(stdout, "analyze_kallsym_info error\n");
        return -1;
    }
    dump_all_symbols(&kallsym, kernel_file.kimg);
    set_log_enable(false);
    free_kernel_file(&kernel_file);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        fprintf(stderr, "Commands:\n");
        fprintf(stderr, "  patch <kimg> <kiteimg> <out> <superkey>  Patch kernel with KITE\n");
        fprintf(stderr, "  unpatch <kimg> <out>                        Remove KITE patch\n");
        fprintf(stderr, "  dump <kimg>                               Dump kallsyms\n");
        fprintf(stderr, "  extract <bootimg>                         Extract kernel from boot.img\n");
        fprintf(stderr, "  repack <bootimg> <newkimg> <out>          Repack boot.img\n");
        fprintf(stderr, "  patch-boot <bootimg> <kiteimg> <out> <superkey>  Extract, patch and repack boot.img\n");
        return 1;
    }

    const char *cmd = argv[1];

    if (!strcmp(cmd, "patch")) {
        if (argc < 6) {
            fprintf(stderr, "Usage: %s patch <kimg> <kiteimg> <out> <superkey>\n", argv[0]);
            return 1;
        }
        return patch_kernel(argv[2], argv[3], argv[4], argv[5]);
    } else if (!strcmp(cmd, "unpatch")) {
        if (argc < 4) {
            fprintf(stderr, "Usage: %s unpatch <kimg> <out>\n", argv[0]);
            return 1;
        }
        return unpatch_kernel(argv[2], argv[3]);
    } else if (!strcmp(cmd, "dump")) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s dump <kimg>\n", argv[0]);
            return 1;
        }
        return dump_kallsym(argv[2]);
    } else if (!strcmp(cmd, "extract")) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s extract <bootimg>\n", argv[0]);
            return 1;
        }
        return extract_kernel(argv[2]);
    } else if (!strcmp(cmd, "repack")) {
        if (argc < 5) {
            fprintf(stderr, "Usage: %s repack <bootimg> <newkimg> <out>\n", argv[0]);
            return 1;
        }
        return repack_bootimg(argv[2], argv[3], argv[4]);
    } else if (!strcmp(cmd, "patch-boot")) {
        if (argc < 6) {
            fprintf(stderr, "Usage: %s patch-boot <bootimg> <kiteimg> <out> <superkey>\n", argv[0]);
            return 1;
        }
        const char *bootimg_path = argv[2];
        const char *kiteimg_path = argv[3];
        const char *out_path = argv[4];
        const char *superkey = argv[5];

        tools_logi("=== KITE Patch-Boot: %s -> %s ===\n", bootimg_path, out_path);

        // Step 1: Extract kernel from boot.img
        tools_logi("[Step 1/3] Extracting kernel from boot.img...\n");
        if (extract_kernel(bootimg_path) != 0) {
            tools_loge_exit("Failed to extract kernel from boot.img\n");
        }

        // Step 2: Patch kernel with KITE
        tools_logi("[Step 2/3] Patching kernel with KITE...\n");
        if (patch_kernel("kernel", kiteimg_path, "patched_kernel", superkey) != 0) {
            tools_loge_exit("Failed to patch kernel\n");
        }

        // Step 3: Repack boot.img
        tools_logi("[Step 3/3] Repacking boot.img...\n");
        if (repack_bootimg(bootimg_path, "patched_kernel", out_path) != 0) {
            tools_loge_exit("Failed to repack boot.img\n");
        }

        tools_logi("=== Patch-Boot Complete: %s ===\n", out_path);
        return 0;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        return 1;
    }
}

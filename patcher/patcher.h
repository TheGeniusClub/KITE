/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch tools
 */

#ifndef _KITE_PATCHER_H_
#define _KITE_PATCHER_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define KITE_MAGIC "KITEv6"
#define KITE_MAGIC_LEN 6
#define KITE_HEADER_SIZE 0x40
#define SUPER_KEY_LEN 0x20
#define MAP_ALIGN 0x10
#define MAP_SYMBOL_NUM 7
#define MAP_SYMBOL_SIZE (MAP_SYMBOL_NUM * 8)
#define HOOK_ALLOC_SIZE (1 << 20)
#define MEMORY_ROX_SIZE (4 << 20)
#define MEMORY_RW_SIZE (2 << 20)

#define MAP_SYM_MEMBLOCK_PHYS_ALLOC_TRY_NID 1
#define MAP_SYM_MEMBLOCK_ALLOC_TRY_NID 2
#define MAP_SYM_MEMBLOCK_VIRT_ALLOC_TRY_NID 1
#define MAP_SYM_MEMBLOCK_VIRT_ALLOC_FROM_ALLOC_TRY_NID 2

#define SZ_4K 0x1000
#define align_floor(x, align) ((uint64_t)(x) & ~((uint64_t)(align)-1))
#define align_ceil(x, align) (((uint64_t)(x) + (uint64_t)(align)-1) & ~((uint64_t)(align)-1))

#define is_be() (*(unsigned char *)&(uint16_t){ 1 } ? 0 : 1)

#define INSN_IS_B(inst) (((inst) & 0xFC000000) == 0x14000000)
#define bits32(n, high, low) ((uint32_t)((n) << (31u - (high))) >> (31u - (high) + (low)))
#define sign64_extend(n, len) \
    (((uint64_t)((n) << (63u - (len - 1))) >> 63u) ? ((n) | (0xFFFFFFFFFFFFFFFF << (len))) : n)

typedef struct
{
    union
    {
        struct
        {
            uint64_t memblock_reserve_relo;
            uint64_t memblock_free_relo;
            uint64_t memblock_phys_alloc_relo;
            uint64_t memblock_virt_alloc_relo;
            uint64_t memblock_mark_nomap_relo;
            uint64_t memblock_phys_alloc_type;
            uint64_t memblock_virt_alloc_type;
        };
        char _cap[MAP_SYMBOL_SIZE];
    };
} map_symbol_t;

typedef struct
{
    uint32_t kernel_version;
    uint32_t _;
    uint64_t kimg_size;
    uint64_t kiteimg_size;
    uint64_t kernel_size;
    uint64_t page_shift;
    uint64_t setup_offset;
    uint64_t start_offset;
    uint64_t extra_size;
    uint64_t map_offset;
    uint64_t map_max_size;
    uint64_t kallsyms_lookup_name_offset;
    uint64_t paging_init_offset;
    uint64_t printk_offset;
    map_symbol_t map_symbol;
    uint8_t header_backup[8];
    uint8_t superkey[SUPER_KEY_LEN];
} setup_preset_t;

typedef struct
{
    char magic[KITE_MAGIC_LEN];
    uint8_t _[KITE_HEADER_SIZE - KITE_MAGIC_LEN];
} kite_header_t;

typedef struct
{
    kite_header_t header;
    setup_preset_t setup;
} kite_preset_t;

/* kallsyms */
#define KSYM_TOKEN_NUMS 256
#define KSYM_SYMBOL_LEN 512
#define KSYM_MIN_NEQ_SYMS 25600
#define KSYM_MIN_MARKER (KSYM_MIN_NEQ_SYMS / 256)
#define KSYM_FIND_NAMES_USED_MARKER 5
#define ARM64_RELO_MIN_NUM 4000

#define ELF64_KERNEL_MIN_VA 0xffffff8008080000
#define ELF64_KERNEL_MAX_VA 0xffffffffffffffff

enum arch_type
{
    ARM64 = 1,
    X86_64,
    ARM_BE,
    ARM_LE,
    X86
};

enum current_type
{
    SP_EL0,
    SP
};

typedef struct
{
    enum arch_type arch;
    int32_t is_64;
    int32_t is_be;

    struct
    {
        uint8_t _;
        uint8_t patch;
        uint8_t minor;
        uint8_t major;
    } version;

    int32_t banner_num;
    int32_t linux_banner_offset[4];
    int32_t symbol_banner_idx;

    char *kallsyms_token_table[KSYM_TOKEN_NUMS];
    int32_t asm_long_size;
    int32_t asm_PTR_size;
    int32_t kallsyms_markers_elem_size;
    int32_t kallsyms_num_syms;

    int32_t has_relative_base;
    int32_t kallsyms_addresses_offset;
    int32_t kallsyms_offsets_offset;
    int32_t kallsyms_num_syms_offset;
    int32_t kallsyms_names_offset;
    int32_t kallsyms_markers_offset;
    int32_t kallsyms_token_table_offset;
    int32_t kallsyms_token_index_offset;

    int32_t _approx_addresses_or_offsets_offset;
    int32_t _approx_addresses_or_offsets_end;
    int32_t _approx_addresses_or_offsets_num;
    int32_t _marker_num;

    int32_t try_relo;
    int32_t relo_applied;
    uint64_t kernel_base;

    int32_t is_kallsysms_all_yes;
    enum current_type current_type;
} kallsym_t;

/* kernel image info */
typedef struct
{
    int8_t is_be;
    int8_t uefi;
    int32_t load_offset;
    int32_t kernel_size;
    int32_t page_shift;
    int32_t b_stext_insn_offset;
    int32_t primary_entry_offset;
} kernel_info_t;

/* boot image */
#define ALIGN(x, a) (((x) + (a) - 1) & ~((a) - 1))
#define PAGE_SIZE_DEFAULT 4096
#define LZ4_MAGIC 0x184c2102
#define LZ4_BLOCK_SIZE 0x800000
#define LZ4HC_CLEVEL 12
#define AVB_FOOTER_SIZE 64

struct boot_img_hdr {
    uint8_t magic[8];
    uint32_t kernel_size;
    uint32_t kernel_addr;
    uint32_t ramdisk_size;
    uint32_t ramdisk_addr;
    uint32_t second_size;
    uint32_t second_addr;
    uint32_t tags_addr;
    uint32_t page_size;
    uint32_t unused[2];
    uint8_t name[16];
    uint8_t cmdline[512];
    uint32_t id[8];
    uint8_t extra_cmdline[1024];
    uint32_t recovery_dtbo_size;
    uint64_t recovery_dtbo_offset;
    uint32_t header_size;
    uint32_t dtb_size;
    uint64_t dtb_addr;
};

struct kernel_hdr {
    uint32_t code0;
    uint32_t code1;
    uint64_t text_offset;
    uint64_t image_size;
    uint64_t flags;
    uint64_t res2;
    uint64_t res3;
    uint64_t res4;
    uint32_t magic;
    uint32_t res5;
};

typedef struct {
    uint8_t magic[8];
} compress_head;

#define DTB_MAGIC "\xd0\x0d\xfe\xed"

struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

struct avb_footer {
    uint32_t reverse[16];
    uint32_t magic;
    uint32_t version;
    uint64_t reserved1;
    uint32_t data_size1;
    uint32_t data_size_1;
    uint32_t data_size2;
    uint32_t data_size_2;
    uint64_t unknown_field;
    uint8_t padding[24];
} __attribute__((packed));

/* logging */
extern bool log_enable;

#define tools_logi(fmt, ...) \
    if (log_enable) fprintf(stdout, "[+] " fmt, ##__VA_ARGS__);

#define tools_logw(fmt, ...) \
    if (log_enable) fprintf(stdout, "[?] " fmt, ##__VA_ARGS__);

#define tools_loge(fmt, ...) \
    if (log_enable) fprintf(stdout, "[-] %s:%d/%s(); " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__);

#define tools_loge_exit(fmt, ...)                                                             \
    do {                                                                                      \
        fprintf(stderr, "[-] %s:%d/%s(); " fmt, __FILE__, __LINE__, __func__, ##__VA_ARGS__); \
        exit(EXIT_FAILURE);                                                                   \
    } while (0)

#define tools_log_errno_exit(fmt, ...)                                                                 \
    do {                                                                                               \
        fprintf(stderr, "[-] %s:%d/%s(); " fmt " - %s\n", __FILE__, __LINE__, __func__, ##__VA_ARGS__, \
                strerror(errno));                                                                      \
        exit(errno);                                                                                   \
    } while (0)

/* order.c */
int16_t i16swp(int16_t val);
int16_t i16le(int16_t val);
int16_t i16be(int16_t val);
uint16_t u16swp(uint16_t val);
uint16_t u16le(uint16_t val);
uint16_t u16be(uint16_t val);
int32_t i32swp(int32_t val);
int32_t i32le(int32_t val);
int32_t i32be(int32_t val);
uint32_t u32swp(uint32_t val);
uint32_t u32le(uint32_t val);
uint32_t u32be(uint32_t val);
int64_t i64swp(int64_t val);
int64_t i64le(int64_t val);
int64_t i64be(int64_t val);
uint64_t u64swp(uint64_t val);
uint64_t u64le(uint64_t val);
uint64_t u64be(uint64_t val);

/* common.c */
int can_b_imm(uint64_t from, uint64_t to);
int b(uint32_t *buf, uint64_t from, uint64_t to);
int32_t relo_branch_func(const char *img, int32_t func_offset);
void write_file(const char *path, const char *con, int len, bool append);
void read_file_align(const char *path, char **con, int *len, int align);
int64_t int_unpack(void *ptr, int32_t size, bool is_be);
uint64_t uint_unpack(void *ptr, int32_t size, bool is_be);

static inline void read_file(const char *path, char **con, int *len)
{
    return read_file_align(path, con, len, 1);
}

static inline void set_log_enable(bool enable)
{
    log_enable = enable;
}

/* image.c */
int32_t get_kernel_info(kernel_info_t *kinfo, const char *img, int32_t imglen);
int32_t kernel_resize(kernel_info_t *kinfo, char *img, int32_t size);

/* kallsym.c */
int find_linux_banner(kallsym_t *info, char *img, int32_t imglen, void *opt);
int analyze_kallsym_info(kallsym_t *info, char *img, int32_t imglen, enum arch_type arch, int32_t is_64);
int dump_all_symbols(kallsym_t *info, char *img);
int get_symbol_offset(kallsym_t *info, char *img, char *symbol);
int get_symbol_offset_and_size(kallsym_t *info, char *img, char *symbol, int32_t *size);
int32_t get_symbol_index_offset(kallsym_t *info, char *img, int32_t index);
int on_each_symbol(kallsym_t *info, char *img, void *userdata,
                   int32_t (*fn)(int32_t index, char type, const char *symbol, int32_t offset, void *userdata));

/* symbol.c */
int32_t get_symbol_offset_zero(kallsym_t *info, char *img, char *symbol);
int32_t get_symbol_offset_exit(kallsym_t *info, char *img, char *symbol);
int32_t find_suffixed_symbol(kallsym_t *kallsym, char *img_buf, const char *symbol);
void select_map_area(kallsym_t *kallsym, char *image_buf, int imglen, int32_t *map_start, int32_t *max_size, bool is_gki);
int32_t get_map_anchor_addr(kallsym_t *kallsym, char *image_buf, int imglen);
int fillin_map_symbol(kallsym_t *kallsym, char *img_buf, map_symbol_t *symbol, int32_t target_is_be);

/* bootimg.c */
int repack_bootimg(const char *orig_boot_path,
                   const char *new_kernel_path,
                   const char *out_boot_path);
int extract_kernel(const char *bootimg_path);
int detect_compress_method(compress_head data);
void *kite_memmem(const void *haystack, size_t haystacklen, const void *needle, size_t needlelen);

/* patcher.c */
typedef struct
{
    char *kfile, *kimg;
    int32_t kfile_len, kimg_len;
    bool is_uncompressed_img;
} kernel_file_t;

void read_kernel_file(const char *path, kernel_file_t *kernel_file);
void new_kernel_file(kernel_file_t *kernel_file, kernel_file_t *old, int32_t kimg_len, bool is_different_endian);
void update_kernel_file_img_len(kernel_file_t *kernel_file, int32_t kimg_len, bool is_different_endian);
void write_kernel_file(kernel_file_t *kernel_file, const char *path);
void free_kernel_file(kernel_file_t *kernel_file);
kite_preset_t *get_preset(const char *kimg, int kimg_len);
int patch_kernel(const char *kimg_path, const char *kiteimg_path, const char *out_path, const char *superkey);
int unpatch_kernel(const char *kimg_path, const char *out_path);
int dump_kallsym(const char *kimg_path);

#endif // _KITE_PATCHER_H_

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch tools/bootimg.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <zlib.h>
#include <sys/stat.h>
#include <lz4.h>
#include <lz4hc.h>
#include <lz4frame.h>
#include "patcher.h"
#include "sha1.h"
#include "sha256.h"
#include "lib/xz/xz.h"

void *kite_memmem(const void *haystack, size_t haystacklen,
                  const void *needle, size_t needlelen)
{
    if (needlelen == 0) return (void *)haystack;
    if (haystacklen < needlelen) return NULL;

    const char *h = (const char *)haystack;
    const char *n = (const char *)needle;
    size_t i;

    for (i = 0; i <= haystacklen - needlelen; i++) {
        if (h[i] == n[0] && memcmp(&h[i], n, needlelen) == 0) {
            return (void *)&h[i];
        }
    }
    return NULL;
}

static uint64_t XXH_swap64(uint64_t x)
{
    return ((x << 56) & 0xff00000000000000ULL) |
           ((x << 40) & 0x00ff000000000000ULL) |
           ((x << 24) & 0x0000ff0000000000ULL) |
           ((x << 8)  & 0x000000ff00000000ULL) |
           ((x >> 8)  & 0x00000000ff000000ULL) |
           ((x >> 24) & 0x0000000000ff0000ULL) |
           ((x >> 40) & 0x000000000000ff00ULL) |
           ((x >> 56) & 0x00000000000000ffULL);
}

static uint32_t XXH_swap32(uint32_t x)
{
    return ((x >> 24) & 0x000000FF) |
           ((x >>  8) & 0x0000FF00) |
           ((x <<  8) & 0x00FF0000) |
           ((x << 24) & 0xFF000000);
}

static uint32_t fdt32_to_cpu(uint32_t val)
{
    return ((val << 24) & 0xff000000) |
           ((val << 8)  & 0x00ff0000) |
           ((val >> 8)  & 0x0000ff00) |
           ((val >> 24) & 0x000000ff);
}

int is_sha256(uint32_t id[8])
{
    if ((id[0] | id[1] | id[2] | id[3] | id[4] | id[5]) == 0) {
        return 1;
    }
    if (id[6] != 0 || id[7] != 0) {
        return 2;
    }
    return 0;
}

static int find_dtb_offset(const uint8_t *buf, unsigned int sz)
{
    if (!buf || sz < sizeof(struct fdt_header)) return -1;
    const uint8_t *curr = buf;
    const uint8_t *end = buf + sz;

    while (curr < end - sizeof(struct fdt_header)) {
        curr = kite_memmem(curr, end - curr, DTB_MAGIC, 4);
        if (curr == NULL) return -1;

        struct fdt_header *fdt_hdr = (struct fdt_header *)curr;
        uint32_t totalsize = fdt32_to_cpu(fdt_hdr->totalsize);
        uint32_t off_dt_struct = fdt32_to_cpu(fdt_hdr->off_dt_struct);
        if (totalsize > (uint32_t)(end - curr) || totalsize <= 0x48) {
            curr += 4;
            continue;
        }

        if (curr + off_dt_struct + 4 <= end) {
            uint32_t *tag = (uint32_t *)(curr + off_dt_struct);
            if (fdt32_to_cpu(*tag) == 0x00000001) {
                return (int)(curr - buf);
            }
        }
        curr += 4;
    }
    return -1;
}

static int write_data_to_file(const char *path, const void *data, size_t size)
{
    FILE *fp = fopen(path, "wb");
    if (!fp) return -1;
    fwrite(data, 1, size, fp);
    fclose(fp);
    chmod(path, 0644);
    return 0;
}

static int compress_gzip(const uint8_t *in_data, size_t in_size, uint8_t **out_data, uint32_t *out_size)
{
    z_stream strm = {0};
    if (deflateInit2(&strm, 9, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return -1;

    uint32_t max_out_size = deflateBound(&strm, in_size);
    *out_data = malloc(max_out_size);
    if (!*out_data) { deflateEnd(&strm); return -1; }

    strm.next_in = (Bytef *)in_data;
    strm.avail_in = in_size;
    strm.next_out = *out_data;
    strm.avail_out = max_out_size;

    int ret = deflate(&strm, Z_FINISH);
    if (ret != Z_STREAM_END) {
        free(*out_data);
        deflateEnd(&strm);
        return -2;
    }

    *out_size = strm.total_out;
    deflateEnd(&strm);
    return 0;
}

static int decompress_gzip(const uint8_t *in_data, size_t in_size, const char *out_path)
{
    z_stream strm = {0};
    strm.next_in = (Bytef *)in_data;
    strm.avail_in = in_size;

    if (inflateInit2(&strm, 16 + MAX_WBITS) != Z_OK) return -1;

    FILE *out = fopen(out_path, "wb");
    if (!out) { inflateEnd(&strm); return -1; }

    uint8_t out_buf[40960];
    int ret;
    do {
        strm.next_out = out_buf;
        strm.avail_out = sizeof(out_buf);
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret < 0 && ret != Z_STREAM_END) {
            fclose(out);
            inflateEnd(&strm);
            return -2;
        }
        size_t have = sizeof(out_buf) - strm.avail_out;
        fwrite(out_buf, 1, have, out);
    } while (ret != Z_STREAM_END);

    fclose(out);
    chmod(out_path, 0644);
    inflateEnd(&strm);
    return 0;
}

static int compress_lz4_le(const uint8_t *in_data, size_t in_size,
                           uint8_t **out_data, uint32_t *out_size)
{
    size_t max_blocks = (in_size + LZ4_BLOCK_SIZE - 1) / LZ4_BLOCK_SIZE;
    size_t max_out = sizeof(uint32_t) +
        max_blocks * (sizeof(uint32_t) + LZ4_compressBound(LZ4_BLOCK_SIZE));

    uint8_t *out = malloc(max_out);
    if (!out) return -2;

    size_t out_off = 0;
    uint32_t magic = LZ4_MAGIC;
    memcpy(out + out_off, &magic, sizeof(magic));
    out_off += sizeof(magic);

    size_t in_off = 0;

    while (in_off < in_size) {
        size_t chunk_size = in_size - in_off;
        if (chunk_size > LZ4_BLOCK_SIZE)
            chunk_size = LZ4_BLOCK_SIZE;

        int max_dst = LZ4_compressBound((int)chunk_size);

        int compressed = LZ4_compress_HC(
            (const char *)(in_data + in_off),
            (char *)(out + out_off + sizeof(uint32_t)),
            (int)chunk_size,
            max_dst,
            LZ4HC_CLEVEL
        );

        if (compressed <= 0) {
            free(out);
            return -3;
        }

        uint32_t csz = (uint32_t)compressed;
        memcpy(out + out_off, &csz, sizeof(csz));
        out_off += sizeof(uint32_t);
        out_off += compressed;
        in_off += chunk_size;
    }

    *out_data = out;
    *out_size = (uint32_t)out_off;
    return 0;
}

static int decompress_xz(const uint8_t *src, size_t srcSize, uint8_t **dst, uint32_t *dstSize)
{
    xz_crc32_init();

    struct xz_dec *s = xz_dec_init(XZ_SINGLE, 0);
    if (s == NULL) return -1;

    uint32_t dstCapacity = 128 * 1024 * 1024;
    *dst = (uint8_t *)malloc(dstCapacity);
    if (!*dst) {
        xz_dec_end(s);
        return -1;
    }

    struct xz_buf b;
    b.in = src;
    b.in_pos = 0;
    b.in_size = srcSize;
    b.out = *dst;
    b.out_pos = 0;
    b.out_size = dstCapacity;
    enum xz_ret ret = xz_dec_run(s, &b);

    if (ret != XZ_STREAM_END) {
        tools_loge("XZ Decompression failed: %d\n", ret);
        free(*dst);
        xz_dec_end(s);
        return -1;
    }

    *dstSize = (uint32_t)b.out_pos;
    xz_dec_end(s);
    return 0;
}

static int decompress_lzma(const uint8_t *src, size_t srcSize, uint8_t **dst, uint32_t *dstSize)
{
    xz_crc32_init();

    struct xz_dec *s = xz_dec_init(XZ_SINGLE, 0);
    if (!s) return -1;

    uint32_t dstCapacity = 128 * 1024 * 1024;
    *dst = (uint8_t *)malloc(dstCapacity);
    if (!*dst) {
        xz_dec_end(s);
        return -1;
    }

    struct xz_buf b;
    b.in = src;
    b.in_pos = 0;
    b.in_size = srcSize;
    b.out = *dst;
    b.out_pos = 0;
    b.out_size = dstCapacity;

    enum xz_ret ret = xz_dec_run(s, &b);

    if (ret != XZ_STREAM_END) {
        tools_loge("LZMA Decompression failed: %d\n", ret);
        free(*dst);
        xz_dec_end(s);
        return -1;
    }

    *dstSize = (uint32_t)b.out_pos;
    xz_dec_end(s);
    return 0;
}

static int parse_lz4_header(const compress_head *head, LZ4F_preferences_t *prefs)
{
    uint32_t magic = (head->magic[0] << 0) | (head->magic[1] << 8) |
                     (head->magic[2] << 16) | (head->magic[3] << 24);
    if (magic != 0x184D2204) {
        return -1;
    }
    LZ4F_preferences_t tmp = LZ4F_INIT_PREFERENCES;
    *prefs = tmp;

    uint8_t flg = head->magic[4];
    prefs->frameInfo.blockMode = (flg & 0x20) ? LZ4F_blockIndependent : LZ4F_blockLinked;
    prefs->frameInfo.blockChecksumFlag = (flg & 0x10) ?
                                         LZ4F_blockChecksumEnabled : LZ4F_noBlockChecksum;
    prefs->frameInfo.contentChecksumFlag = (flg & 0x08) ?
                                           LZ4F_contentChecksumEnabled : LZ4F_noContentChecksum;

    uint8_t bd = head->magic[5];
    uint8_t block_size_id = (bd >> 4) & 0x07;
    switch (block_size_id) {
        case 4: prefs->frameInfo.blockSizeID = LZ4F_max64KB; break;
        case 5: prefs->frameInfo.blockSizeID = LZ4F_max256KB; break;
        case 6: prefs->frameInfo.blockSizeID = LZ4F_max1MB; break;
        case 7: prefs->frameInfo.blockSizeID = LZ4F_max4MB; break;
        default: return -2;
    }

    return 0;
}

static int compress_lz4(const uint8_t *in_data, size_t in_size,
                        uint8_t **out_data, uint32_t *out_size,
                        compress_head k_head)
{
    LZ4F_preferences_t prefs;
    int ret = parse_lz4_header(&k_head, &prefs);
    if (ret < 0) {
        return -3;
    }

    size_t max_out_size = LZ4F_compressFrameBound(in_size, &prefs);

    *out_data = (uint8_t *)malloc(max_out_size);
    if (!*out_data) {
        return -1;
    }

    size_t compressed_size = LZ4F_compressFrame(*out_data, max_out_size,
                                                in_data, in_size, &prefs);

    if (LZ4F_isError(compressed_size)) {
        free(*out_data);
        return -2;
    }

    *out_size = (uint32_t)compressed_size;
    return 0;
}

static int auto_depress(const uint8_t *data, size_t size, const char *out_path)
{
    if (size < 4) return -1;
    compress_head k_head;
    memcpy(&k_head, data, sizeof(k_head));
    int method = detect_compress_method(k_head);
    tools_logi("Auto-detect compression method: %d\n", method);

    if (method == 1) {
        tools_logi("Detected GZIP compressed kernel.\n");
        if (decompress_gzip(data, size, out_path) == 0) {
            tools_logi(" Decompressed to %s\n", out_path);
            return 0;
        } else {
            tools_logi(" Gzip decompression failed.\n");
            return -1;
        }
    }

    if (method == 2) {
        tools_logi("Detected LZ4 Frame format. Decompressing...\n");
        size_t out_cap = 128 * 1024 * 1024;
        uint8_t *out = malloc(out_cap);
        if (!out) {
            tools_loge(" LZ4 Frame: failed to allocate output buffer\n");
            return -1;
        }
        size_t dstSize = out_cap;
        size_t srcSize = size;
        size_t const decSize = LZ4F_decompress(NULL, out, &dstSize, data, &srcSize, NULL);
        if (LZ4F_isError(decSize)) {
            tools_loge(" LZ4 Frame: decompression error: %s\n", LZ4F_getErrorName(decSize));
            free(out);
            return -1;
        }
        LZ4F_dctx *dctx = NULL;
        LZ4F_errorCode_t const err = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
        if (LZ4F_isError(err)) {
            tools_loge(" LZ4 Frame: context creation error: %s\n", LZ4F_getErrorName(err));
            free(out);
            return -1;
        }
        dstSize = out_cap;
        srcSize = size;
        size_t const result = LZ4F_decompress(dctx, out, &dstSize, data, &srcSize, NULL);
        LZ4F_freeDecompressionContext(dctx);
        if (LZ4F_isError(result)) {
            tools_loge(" LZ4 Frame: decompression error: %s\n", LZ4F_getErrorName(result));
            free(out);
            return -1;
        }
        write_data_to_file(out_path, out, (uint32_t)dstSize);
        free(out);
        tools_logi(" LZ4 Frame Decompressed: %zu bytes\n", dstSize);
        return 0;
    }

    if (method == 3) {
        tools_logi("Probing LZ4 Legacy (block-based)...\n");

        const uint8_t *p = (const uint8_t *)data;
        const uint8_t *end = p + size;

        if (size < 4)
            goto not_lz4;

        if (*(uint32_t *)p != LZ4_MAGIC)
            goto not_lz4;

        p += 4;

        size_t out_cap = 64 * 1024 * 1024;
        uint8_t *out = malloc(out_cap);
        if (!out)
            goto not_lz4;

        size_t out_off = 0;

        uint8_t *block_out = malloc(LZ4_BLOCK_SIZE);
        if (!block_out) {
            free(out);
            goto not_lz4;
        }

        int decoded_any = 0;
        while (1) {
            uint32_t block_size;

            if (p + 4 > end)
                break;

            memcpy(&block_size, p, 4);
            p += 4;

            if (block_size == 0)
                break;

            if (block_size > LZ4_compressBound(LZ4_BLOCK_SIZE))
                goto fail;

            if (p + block_size > end)
                goto fail;

            int decoded = LZ4_decompress_safe(
                (const char *)p,
                (char *)block_out,
                (int)block_size,
                LZ4_BLOCK_SIZE
            );

            if (decoded < 0)
                goto fail;

            decoded_any = 1;

            if (out_off + (size_t)decoded > out_cap) {
                size_t new_cap = out_cap * 2;
                while (new_cap < out_off + (size_t)decoded)
                    new_cap *= 2;

                uint8_t *tmp = realloc(out, new_cap);
                if (!tmp)
                    goto fail;

                out = tmp;
                out_cap = new_cap;
            }

            memcpy(out + out_off, block_out, (size_t)decoded);
            out_off += (size_t)decoded;

            p += block_size;
        }

        if (!decoded_any)
            goto fail;

        tools_logi("LZ4 block decompressed: %zu bytes\n", out_off);
        write_data_to_file(out_path, out, (uint32_t)out_off);

        free(block_out);
        free(out);
        return 0;

    fail:
        free(block_out);
        free(out);

    not_lz4:
        tools_logi("Not LZ4 block format, fallback.\n");
    }

    if (method == 5) {
        tools_logi("Detected BZIP2. Decompressing...\n");
        tools_logi(" BZIP2 not supported in this build.\n");
        return -1;
    }

    if (method == 6) {
        tools_logi(" Detected XZ format. Decompressing...\n");
        uint8_t *xz_dst = NULL;
        uint32_t xz_size = 0;

        if (decompress_xz(data, size, &xz_dst, &xz_size) == 0) {
            tools_logi("XZ Decompressed: %u bytes\n", xz_size);
            write_data_to_file(out_path, xz_dst, xz_size);
            free(xz_dst);
            return 0;
        } else {
            tools_loge(" XZ Decompression failed.\n");
            return -1;
        }
    }

    if (method == 7) {
        tools_logi("Detected Legacy LZMA format. Decompressing...\n");
        uint8_t *lzma_dst = NULL;
        uint32_t lzma_size = 0;

        if (decompress_lzma(data, size, &lzma_dst, &lzma_size) == 0) {
            tools_logi(" LZMA Decompressed: %u bytes\n", lzma_size);
            write_data_to_file(out_path, lzma_dst, lzma_size);
            free(lzma_dst);
            return 0;
        } else {
            tools_loge(" LZMA Decompression failed.\n");
            return -1;
        }
    }

    tools_logi("Treating as Raw Kernel (or unknown format).\n");
    if (write_data_to_file(out_path, data, size) == 0) {
        tools_logi(" Saved raw kernel to %s\n", out_path);
        return 0;
    }

    return -1;
}

int extract_kernel(const char *bootimg_path)
{
    FILE *fp = fopen(bootimg_path, "rb");
    if (!fp) {
        tools_logi("Error: Cannot open %s\n", bootimg_path);
        return -1;
    }

    struct boot_img_hdr hdr;
    fread(&hdr, sizeof(hdr), 1, fp);

    if (memcmp(hdr.magic, "ANDROID!", 8) != 0) {
        tools_logi("Error: Invalid boot image magic.\n");
        fclose(fp);
        return -2;
    }

    uint32_t page_size = hdr.page_size;
    uint32_t kernel_offset = page_size;
    if (hdr.unused[0] >= 3) {
        kernel_offset = 4096;
    }
    if (hdr.unused[0] > 10) {
        kernel_offset = page_size;
    }

    tools_logi("Kernel size: %d,Header Version: %d, Offset: %d\n", hdr.kernel_size, hdr.unused[0], kernel_offset);

    uint8_t *kernel_data = malloc(hdr.kernel_size);
    if (!kernel_data) {
        fclose(fp);
        return -3;
    }

    fseek(fp, kernel_offset, SEEK_SET);
    fread(kernel_data, 1, hdr.kernel_size, fp);
    fclose(fp);

    int res = auto_depress(kernel_data, hdr.kernel_size, "kernel");

    free(kernel_data);
    return res;
}

int detect_compress_method(compress_head data)
{
    if (data.magic[0] == 0x1F && data.magic[1] == 0x8B) return 1;
    if (data.magic[0] == 0x1F && data.magic[1] == 0x9E) return 1;
    if (data.magic[0] == 0x04 && data.magic[1] == 0x22 &&
        data.magic[2] == 0x4D && data.magic[3] == 0x18) return 2;
    if (data.magic[0] == 0x03 && data.magic[1] == 0x21 &&
        data.magic[2] == 0x4C && data.magic[3] == 0x18) return 2;
    if (data.magic[0] == 0x02 && data.magic[1] == 0x21 &&
        data.magic[2] == 0x4C && data.magic[3] == 0x18) return 3;
    if (data.magic[0] == 0x28 && data.magic[1] == 0xB5 &&
        data.magic[2] == 0x2F && data.magic[3] == 0xFD) return 4;
    if (data.magic[0] == 0x42 && data.magic[1] == 0x5A &&
        data.magic[2] == 0x68) return 5;
    if (data.magic[0] == 0xFD && data.magic[1] == 0x37 &&
        data.magic[2] == 0x7A && data.magic[3] == 0x58) return 6;
    if (data.magic[0] == 0x5D && data.magic[1] == 0x00 &&
        data.magic[2] == 0x00) return 7;

    return 0;
}

int repack_bootimg(const char *orig_boot_path,
                   const char *new_kernel_path,
                   const char *out_boot_path)
{
    tools_logi(" Starting automatic repack...\n");

    FILE *f_orig = fopen(orig_boot_path, "rb");
    if (!f_orig) return -1;

    struct boot_img_hdr hdr;
    struct avb_footer avb;
    uint32_t extracted_size = 0;
    fread(&hdr, sizeof(hdr), 1, f_orig);

    if (memcmp(hdr.magic, "ANDROID!", 8) != 0) {
        tools_logi("Not a valid Android Boot Image.\n");
        fclose(f_orig);
        return -2;
    }

    fseek(f_orig, 0, SEEK_END);
    long total_size = ftell(f_orig);

    uint32_t avb_size = 0;
    fseek(f_orig, total_size - sizeof(avb), SEEK_SET);
    fread(&avb, sizeof(avb), 1, f_orig);

    uint32_t header_ver = hdr.unused[0];
    if (header_ver > 10) { header_ver = 0; extracted_size = hdr.unused[0]; }
    uint32_t page_size = (header_ver >= 3) ? 4096 : hdr.page_size;
    uint32_t fmt_size = (header_ver >= 3) ? hdr.kernel_addr : hdr.ramdisk_size;

    tools_logi("Header Version: %u, Page Size: %u, fmt_size: %u\n", header_ver, page_size, fmt_size);

    uint8_t *old_k_full = malloc(hdr.kernel_size);
    fseek(f_orig, page_size, SEEK_SET);
    fread(old_k_full, 1, hdr.kernel_size, f_orig);

    compress_head k_head;
    memcpy(&k_head, old_k_full, sizeof(k_head));
    int method = detect_compress_method(k_head);

    uint8_t *extracted_dtb = NULL;
    uint32_t dtb_size = 0;
    if (header_ver < 3) {
        int dtb_off = find_dtb_offset(old_k_full, hdr.kernel_size);
        if (dtb_off > 0) {
            dtb_size = hdr.kernel_size - dtb_off;
            extracted_dtb = malloc(dtb_size);
            memcpy(extracted_dtb, old_k_full + dtb_off, dtb_size);
            tools_logi("Detected DTB appended to kernel. Size: %u\n", dtb_size);
        }
    }
    free(old_k_full);

    FILE *f_new_k = fopen(new_kernel_path, "rb");
    if (!f_new_k) { fclose(f_orig); if(extracted_dtb) free(extracted_dtb); return -3; }
    fseek(f_new_k, 0, SEEK_END);
    uint32_t raw_k_size = ftell(f_new_k);
    fseek(f_new_k, 0, SEEK_SET);
    uint8_t *raw_k_buf = malloc(raw_k_size);
    fread(raw_k_buf, 1, raw_k_size, f_new_k);
    fclose(f_new_k);

    uint8_t *final_k_buf = raw_k_buf;
    uint32_t final_k_size = raw_k_size;
    uint8_t *compressed_buf = NULL;

    if (method == 1) {
        tools_logi("Compressing new kernel with GZIP...\n");
        if (compress_gzip(raw_k_buf, raw_k_size, &compressed_buf, &final_k_size) == 0) {
            final_k_buf = compressed_buf;
        }
    }
    if (method == 2) {
        tools_logi("Compressing new kernel with LZ4 Frame...\n");
        if (compress_lz4(raw_k_buf, raw_k_size, &compressed_buf, &final_k_size, k_head) == 0) {
            final_k_buf = compressed_buf;
        }
    }
    if (method == 3) {
        tools_logi("Compressing new kernel with LZ4 Legacy...\n");
        if (compress_lz4_le(raw_k_buf, raw_k_size, &compressed_buf, &final_k_size) == 0) {
            final_k_buf = compressed_buf;
        }
    }
    if (method == 4) {
        tools_logi(" Kernel uses zstd, not supported yet\n");
        return -1;
    }
    if (method == 5) {
        tools_logi(" BZIP2 not supported in this build\n");
        return -1;
    }
    if (method == 6 || method == 7) {
        tools_logi(" Original was XZ/LZMA. Repacking as GZIP for compatibility...\n");
        if (compress_gzip(raw_k_buf, raw_k_size, &compressed_buf, &final_k_size) == 0) {
            final_k_buf = compressed_buf;
            method = 1;
            tools_logi("Repacked as GZIP. New Size: %u bytes\n", final_k_size);
        } else {
            tools_loge("GZIP compression failed during XZ-to-GZIP conversion.\n");
            return -1;
        }
    }
    tools_logi("Final kernel size after compression (if applied): %u bytes\n", final_k_size);

    uint32_t old_k_aligned = ALIGN(hdr.kernel_size, page_size);
    uint32_t rest_data_offset = page_size + old_k_aligned;
    uint32_t rest_data_size = (total_size > rest_data_offset) ? (total_size - rest_data_offset) : 0;
    hdr.kernel_size = final_k_size + dtb_size;
    uint32_t checksum_aligned = ALIGN(fmt_size, page_size);
    uint8_t *rest_buf_tmp = NULL;
    uint8_t *rest_buf = NULL;
    uint32_t rest_buf_offset = 0;
    if (rest_data_size > 0) {
        rest_buf_tmp = malloc(rest_data_size);
        fseek(f_orig, rest_data_offset, SEEK_SET);
        fread(rest_buf_tmp, 1, rest_data_size - sizeof(avb), f_orig);
        for (int32_t i = (int32_t)rest_data_size - 1; i >= 0; i--) {
            if (rest_buf_tmp[i] != 0) {
                rest_buf_offset = (uint32_t)(i + 1);
                break;
            }
        }
        if (rest_buf_offset > rest_data_size / 3 * 2) {
            tools_logi("warning: overload size of rest data\n");
            rest_buf = rest_buf_tmp;
            rest_data_size = rest_buf_offset + sizeof(avb);
        } else {
            rest_buf = malloc(rest_buf_offset);
            memcpy(rest_buf, rest_buf_tmp, rest_buf_offset);
            tools_logi("Rest data size: %u bytes, Actual used size: %u bytes\n", rest_data_size, rest_buf_offset);
            rest_data_size = rest_buf_offset;
            free(rest_buf_tmp);
        }
    }
    fclose(f_orig);

    // Recalculate boot image hash (id field)
    uint32_t use_sha256 = is_sha256(hdr.id);
    int hash_len = use_sha256 ? SHA256_BLOCK_SIZE : SHA1_DIGEST_SIZE;
    uint8_t hash_buf[hash_len];
    
    // Only recalculate hash for v0-v3 or SHA1.
    // For v4+ with SHA256, keep original hash to avoid incorrect hash
    // due to fmt_size being a load address rather than data size.
    if (use_sha256 != 1 || header_ver <= 3) {
        if (use_sha256) {
            SHA256_CTX ctx;
            sha256_init(&ctx);
            sha256_update(&ctx, (const uint8_t *)final_k_buf, hdr.kernel_size);
            sha256_update(&ctx, (const uint8_t *)&hdr.kernel_size, 4);
            
            uint32_t checksum_aligned = ALIGN(fmt_size, page_size);
            if (fmt_size > 0) {
                sha256_update(&ctx, (const uint8_t *)rest_buf, fmt_size);
                sha256_update(&ctx, (const uint8_t *)&fmt_size, sizeof(fmt_size));
            }
            
            sha256_update(&ctx, (const uint8_t *)rest_buf + checksum_aligned, hdr.second_size);
            sha256_update(&ctx, (const uint8_t *)&hdr.second_size, 4);
            if (hdr.second_size > 0) {
                checksum_aligned += ALIGN(hdr.second_size, page_size);
            }
            
            if (extracted_size) {
                sha256_update(&ctx, (const uint8_t *)rest_buf + checksum_aligned, page_size);
                sha256_update(&ctx, (const uint8_t *)&extracted_size, 4);
                checksum_aligned += ALIGN(extracted_size, page_size);
            }
            
            if (header_ver == 1 || header_ver == 2) {
                sha256_update(&ctx, (const uint8_t *)rest_buf + checksum_aligned, hdr.recovery_dtbo_size);
                sha256_update(&ctx, (const uint8_t *)&hdr.recovery_dtbo_size, 4);
                checksum_aligned += ALIGN(hdr.recovery_dtbo_size, page_size);
            }
            
            if (header_ver == 2) {
                sha256_update(&ctx, (const uint8_t *)rest_buf + checksum_aligned, hdr.dtb_size);
                sha256_update(&ctx, (const uint8_t *)&hdr.dtb_size, 4);
            }
            
            sha256_final(&ctx, hash_buf);
            memcpy(hdr.id, hash_buf, SHA256_BLOCK_SIZE);
            tools_logi("Recalculated SHA256 hash for boot image\n");
        } else {
            SHA1_CTX ctx;
            sha1_init(&ctx);
            sha1_update(&ctx, (const uint8_t *)final_k_buf, hdr.kernel_size);
            sha1_update(&ctx, (const uint8_t *)&hdr.kernel_size, 4);
            
            uint32_t checksum_aligned = ALIGN(fmt_size, page_size);
            if (fmt_size > 0) {
                sha1_update(&ctx, (const uint8_t *)rest_buf, fmt_size);
                sha1_update(&ctx, (const uint8_t *)&fmt_size, sizeof(fmt_size));
            }
            
            sha1_update(&ctx, (const uint8_t *)rest_buf + checksum_aligned, hdr.second_size);
            sha1_update(&ctx, (const uint8_t *)&hdr.second_size, 4);
            if (hdr.second_size > 0) {
                checksum_aligned += ALIGN(hdr.second_size, page_size);
            }
            
            if (extracted_size) {
                sha1_update(&ctx, (const uint8_t *)rest_buf + checksum_aligned, page_size);
                sha1_update(&ctx, (const uint8_t *)&extracted_size, 4);
                checksum_aligned += ALIGN(extracted_size, page_size);
            }
            
            if (header_ver == 1 || header_ver == 2) {
                sha1_update(&ctx, (const uint8_t *)rest_buf + checksum_aligned, hdr.recovery_dtbo_size);
                sha1_update(&ctx, (const uint8_t *)&hdr.recovery_dtbo_size, 4);
                checksum_aligned += ALIGN(hdr.recovery_dtbo_size, page_size);
            }
            
            if (header_ver == 2) {
                sha1_update(&ctx, (const uint8_t *)rest_buf + checksum_aligned, hdr.dtb_size);
                sha1_update(&ctx, (const uint8_t *)&hdr.dtb_size, 4);
            }
            
            sha1_final(&ctx, hash_buf);
            memcpy(hdr.id, hash_buf, SHA1_DIGEST_SIZE);
            tools_logi("Recalculated SHA1 hash for boot image\n");
        }
    } else {
        tools_logi("Skipped hash recalculation for v%u with SHA256 (keeping original)\n", header_ver);
    }

    FILE *f_out = fopen(out_boot_path, "wb");
    if (!f_out) { return -4; }

    fwrite(&hdr, sizeof(hdr), 1, f_out);
    fseek(f_out, page_size, SEEK_SET);

    fwrite(final_k_buf, 1, final_k_size, f_out);
    if (extracted_dtb) {
        fwrite(extracted_dtb, 1, dtb_size, f_out);
    }
    tools_logi("dtb_size=%d\n", dtb_size);

    uint32_t new_k_total_aligned = ALIGN(hdr.kernel_size, page_size);
    fseek(f_out, page_size + new_k_total_aligned, SEEK_SET);

    uint8_t avb_sig[] = {
        0x41,0x56,0x42,0x30,
        0x00,0x00,0x00,0x01,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,
        0x00,0x00,0x00
    };

    if (rest_buf) {
        uint8_t *avb_ptr = kite_memmem(rest_buf, rest_data_size, avb_sig, sizeof(avb_sig));

        if (!avb_ptr) {
            avb_sig[18] = 0x01;
            avb_ptr = kite_memmem(rest_buf, rest_data_size, avb_sig, sizeof(avb_sig));
        }
        if (!avb_ptr) {
            avb_sig[18] = 0x02;
            avb_ptr = kite_memmem(rest_buf, rest_data_size, avb_sig, sizeof(avb_sig));
        }
        if (avb_ptr) {
            uint8_t *last_avb = NULL;
            uint8_t *search_ptr = avb_ptr;
            while (search_ptr) {
                last_avb = search_ptr;
                tools_logi("Found AVB footer in rest data.%p\n", search_ptr);
                uint32_t offset = (uint32_t)(search_ptr - rest_buf) + sizeof(avb_sig);
                if (offset >= rest_data_size)
                    break;

                search_ptr = kite_memmem(
                    rest_buf + offset,
                    rest_data_size - offset,
                    avb_sig,
                    sizeof(avb_sig)
                );
            }
            avb_ptr = last_avb;
        }

        if (avb_ptr) {
            size_t avb_offset = avb_ptr - rest_buf;
            tools_logi("avb_offset=%zu\n", avb_offset);
            uint32_t avb_size = page_size + avb_offset + new_k_total_aligned;
            avb.data_size1 = XXH_swap32(avb_size);
            avb.data_size2 = XXH_swap32(avb_size);
        }
        if (rest_data_size > total_size - page_size - new_k_total_aligned) {
            total_size = ALIGN(page_size + new_k_total_aligned + rest_data_size, page_size);
            fwrite(rest_buf, 1, total_size - page_size - new_k_total_aligned - sizeof(avb), f_out);
            fwrite(&avb, sizeof(avb), 1, f_out);
        } else {
            fwrite(rest_buf, 1, rest_data_size, f_out);
        }
    }

    long current_pos = ftell(f_out);

    if (current_pos < total_size - sizeof(avb)) {
        uint32_t padding = total_size - current_pos - sizeof(avb);
        uint8_t *zero_pad = calloc(1, padding);
        fwrite(zero_pad, 1, padding, f_out);
        free(zero_pad);
        fwrite(&avb, sizeof(avb), 1, f_out);
    }

    fclose(f_out);
    if (compressed_buf) free(compressed_buf);
    if (extracted_dtb) free(extracted_dtb);
    free(raw_k_buf);
    if (rest_buf) free(rest_buf);

    tools_logi("Repack completed: %s\n", out_boot_path);
    return 0;
}

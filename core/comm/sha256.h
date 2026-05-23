/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_SHA256_H
#define _KITE_SHA256_H

#include <kite/ktypes.h>

#define SHA256_BLOCK_SIZE 32
#define SHA256_DIGEST_SIZE 32

struct sha256_ctx {
    u32 state[8];
    u64 count;
    u8 buf[64];
};

void sha256_init(struct sha256_ctx *ctx);
void sha256_update(struct sha256_ctx *ctx, const u8 *data, size_t len);
void sha256_final(struct sha256_ctx *ctx, u8 *digest);

#endif // _KITE_SHA256_H

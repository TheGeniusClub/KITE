/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_KPM_H
#define _KITE_KPM_H

#include <kite/ktypes.h>

// KPM = KITE Package Module
#define KPM_MAGIC "KPM\0"
#define KPM_VERSION 1

struct kpm_header {
    char magic[4];
    u32 version;
    u32 header_size;
    u32 load_size;
    u32 name_size;
    u32 author_size;
    u32 desc_size;
    u32 target_size;
    u32 license_size;
    u32 depends_size;
    u32 reserved[4];
};

// module api for kpm
struct kpm_ops {
    int (*init)(const char *args);
    void (*exit)(void);
    int (*ctl0)(void *data, size_t len);
    int (*ctl1)(void *data, size_t len);
};

#define KPM_INIT(ops) \
    struct kpm_ops *kpm_init(void) { return (ops); }

#endif // _KITE_KPM_H

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_USERSPACE_H
#define _KITE_USERSPACE_H

#include <kite/ktypes.h>

// userspace control interface
#define KITE_DEV_PATH "/dev/kite"
#define KITE_IOCTL_MAGIC 'K'

#define KITE_IOCTL_LOAD_MODULE _IOW(KITE_IOCTL_MAGIC, 0, char*)
#define KITE_IOCTL_UNLOAD_MODULE _IOW(KITE_IOCTL_MAGIC, 1, char*)
#define KITE_IOCTL_CALL_CTL0 _IOWR(KITE_IOCTL_MAGIC, 2, void*)
#define KITE_IOCTL_CALL_CTL1 _IOWR(KITE_IOCTL_MAGIC, 3, void*)
#define KITE_IOCTL_GET_VERSION _IOR(KITE_IOCTL_MAGIC, 4, u32)

// kite_ctl tool
int kite_ctl_init(void);
int kite_ctl_load_module(const char *path);
int kite_ctl_unload_module(const char *name);
int kite_ctl_call_ctl0(const char *name, void *data, size_t len);
int kite_ctl_call_ctl1(const char *name, void *data, size_t len);
u32 kite_ctl_get_version(void);

#endif // _KITE_USERSPACE_H

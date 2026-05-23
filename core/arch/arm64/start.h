/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Reference from KernelPatch start.h
 */

#ifndef _KITE_START_H
#define _KITE_START_H

#include <kite/ktypes.h>
#include "arch/arm64/setup.h"
#include "memory/tlsf.h"

// preset data
extern setup_preset_t setup_preset;
extern start_preset_t start_preset;
extern map_data_t map_data;
extern uint8_t header[KITE_HEADER_SIZE];

// memory pools
extern tlsf_t kite_rw_mem;
extern tlsf_t kite_rox_mem;

// framework init
extern int kite_late_init(void *preset);

#endif // _KITE_START_H

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KITE_CHALLENGE_AUTH_H
#define _KITE_CHALLENGE_AUTH_H

#include <kite/ktypes.h>

// challenge-response authentication
int challenge_auth_init(void);
u64 challenge_auth_generate(void);
int challenge_auth_verify(u64 challenge, u64 response);

#endif // _KITE_CHALLENGE_AUTH_H

/* KITE - Kernel Injection and Trampoline Engine */
/* Copyright (C) 2026 Dere3046 */
/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is part of KITE project
 * Runtime stubs for kernel context (no libc)
 */

#include "stubs.h"

static uint64_t kite_printk_va = 0;

void kite_printk_init(uint64_t printk_va)
{
    kite_printk_va = printk_va;
}

int kite_printk(const char *fmt, ...)
{
    if (!kite_printk_va) return 0;
    char buf[512];
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int ret = kite_vsnprintf(buf, sizeof(buf), fmt, args);
    __builtin_va_end(args);
    ((int (*)(const char *, ...))kite_printk_va)("%s", buf);
    return ret;
}

void *kite_memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}

void *kite_memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

int kite_memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *p1 = s1;
    const unsigned char *p2 = s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

void *kite_memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) {
        while (n--) {
            *d++ = *s++;
        }
    } else if (d > s) {
        d += n;
        s += n;
        while (n--) {
            *--d = *--s;
        }
    }
    return dest;
}

size_t kite_strlen(const char *s)
{
    size_t len = 0;
    while (s[len]) {
        len++;
    }
    return len;
}

int kite_strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

char *kite_strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

char *kite_strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (n && (*d++ = *src++)) {
        n--;
    }
    while (n--) {
        *d++ = '\0';
    }
    return dest;
}

static char *num_to_str(char *buf, uint64_t num, int base, int upper, int pad, char padc)
{
    char tmp[32];
    int i = 0;
    if (num == 0) {
        tmp[i++] = '0';
    } else {
        while (num) {
            int digit = num % base;
            tmp[i++] = digit < 10 ? '0' + digit : (upper ? 'A' : 'a') + digit - 10;
            num /= base;
        }
    }
    int len = i;
    while (pad > len) {
        *buf++ = padc;
        pad--;
    }
    while (i--) {
        *buf++ = tmp[i];
    }
    return buf;
}

static char *snum_to_str(char *buf, int64_t num, int pad, char padc)
{
    if (num < 0) {
        *buf++ = '-';
        num = -num;
    }
    return num_to_str(buf, (uint64_t)num, 10, 0, pad, padc);
}

int kite_vsnprintf(char *buf, size_t size, const char *fmt, __builtin_va_list args)
{
    char *out = buf;
    char *end = buf + size - 1;
    if (size == 0) return 0;

    while (*fmt && out < end) {
        if (*fmt != '%') {
            *out++ = *fmt++;
            continue;
        }
        fmt++;

        int pad = 0;
        char padc = ' ';
        if (*fmt == '0') {
            padc = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            pad = pad * 10 + (*fmt++ - '0');
        }

        int longlong = 0;
        int longlen = 0;
        if (*fmt == 'l') {
            longlen = 1;
            fmt++;
            if (*fmt == 'l') {
                longlong = 1;
                longlen = 0;
                fmt++;
            }
        } else if (*fmt == 'z') {
            longlen = 1;
            fmt++;
        }

        switch (*fmt) {
        case '%':
            *out++ = '%';
            break;
        case 'c': {
            char c = (char)__builtin_va_arg(args, int);
            *out++ = c;
            break;
        }
        case 's': {
            const char *s = __builtin_va_arg(args, const char *);
            if (!s) s = "(null)";
            while (*s && out < end) {
                *out++ = *s++;
            }
            break;
        }
        case 'd':
        case 'i': {
            int64_t val;
            if (longlong) val = __builtin_va_arg(args, long long);
            else if (longlen) val = __builtin_va_arg(args, long);
            else val = __builtin_va_arg(args, int);
            out = snum_to_str(out, val, pad, padc);
            break;
        }
        case 'u': {
            uint64_t val;
            if (longlong) val = __builtin_va_arg(args, unsigned long long);
            else if (longlen) val = __builtin_va_arg(args, unsigned long);
            else val = __builtin_va_arg(args, unsigned int);
            out = num_to_str(out, val, 10, 0, pad, padc);
            break;
        }
        case 'x':
        case 'X': {
            uint64_t val;
            if (longlong) val = __builtin_va_arg(args, unsigned long long);
            else if (longlen) val = __builtin_va_arg(args, unsigned long);
            else val = __builtin_va_arg(args, unsigned int);
            out = num_to_str(out, val, 16, *fmt == 'X', pad, padc);
            break;
        }
        case 'p': {
            void *p = __builtin_va_arg(args, void *);
            if (out + 2 < end) {
                *out++ = '0';
                *out++ = 'x';
            }
            out = num_to_str(out, (uint64_t)p, 16, 0, 16, '0');
            break;
        }
        default:
            *out++ = '%';
            if (out < end) *out++ = *fmt;
            break;
        }
        fmt++;
    }
    *out = '\0';
    return out - buf;
}

int kite_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int ret = kite_vsnprintf(buf, size, fmt, args);
    __builtin_va_end(args);
    return ret;
}

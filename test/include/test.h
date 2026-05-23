/* KITE Test Framework */
/* Copyright (C) 2026 Dere3046 */

#ifndef _KITE_TEST_H
#define _KITE_TEST_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int tests_run = 0;
static int tests_pass = 0;
static int tests_fail = 0;
static const char *current_test = NULL;

#define TEST_ASSERT(cond) do { \
    tests_run++; \
    if (cond) { \
        tests_pass++; \
    } else { \
        tests_fail++; \
        printf("  FAIL: %s:%d %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

#define TEST_ASSERT_EQ(a, b) TEST_ASSERT((a) == (b))
#define TEST_ASSERT_NE(a, b) TEST_ASSERT((a) != (b))
#define TEST_ASSERT_NULL(p) TEST_ASSERT((p) == NULL)
#define TEST_ASSERT_NOT_NULL(p) TEST_ASSERT((p) != NULL)

#define TEST_BEGIN(name) do { \
    current_test = name; \
    printf("\nTEST: %s\n", name); \
} while(0)

#define TEST_END() do { \
    printf("  run=%d pass=%d fail=%d\n", tests_run, tests_pass, tests_fail); \
} while(0)

#define TEST_SUMMARY() do { \
    printf("\n=== TEST SUMMARY ===\n"); \
    printf("run=%d pass=%d fail=%d\n", tests_run, tests_pass, tests_fail); \
    return tests_fail > 0 ? 1 : 0; \
} while(0)

#endif

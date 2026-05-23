/* KITE Test - Module */
#include "test.h"
#include "module/module.h"
#include "module/loader.h"
#include "comm/kstorage.h"
#include "comm/task_local.h"
#include "memory/tlsf.h"

tlsf_t kite_rox_mem = 0;
tlsf_t kite_rw_mem = 0;

uint64_t symbol_lookup_name(const char *name)
{
    (void)name;
    return 0;
}

// stubs.c already provides kite_printk

int main(void)
{
    TEST_BEGIN("module");
    
    // init pools
    char pool_rw[4096] __attribute__((aligned(16)));
    char pool_rox[4096] __attribute__((aligned(16)));
    kite_rw_mem = tlsf_create(pool_rw, sizeof(pool_rw));
    kite_rox_mem = tlsf_create(pool_rox, sizeof(pool_rox));
    TEST_ASSERT_NOT_NULL(kite_rw_mem);
    TEST_ASSERT_NOT_NULL(kite_rox_mem);
    
    // loader init
    int ret = loader_init();
    TEST_ASSERT_EQ(ret, 0);
    
    // module count
    TEST_ASSERT_EQ(module_get_count(), 0);
    
    // kstorage test
    char key[] = "test_key";
    char val[] = "test_value";
    ret = kstorage_set(key, val, sizeof(val));
    TEST_ASSERT_EQ(ret, 0);
    
    size_t outlen;
    void *out = kstorage_get(key, &outlen);
    TEST_ASSERT_NOT_NULL(out);
    TEST_ASSERT_EQ(outlen, sizeof(val));
    TEST_ASSERT_EQ(memcmp(out, val, sizeof(val)), 0);
    
    // task_local test
    ret = task_local_set(1234, "pid_key", "pid_val", 6);
    TEST_ASSERT_EQ(ret, 0);
    
    size_t tl_len;
    void *tl_val = task_local_get(1234, "pid_key", &tl_len);
    TEST_ASSERT_NOT_NULL(tl_val);
    TEST_ASSERT_EQ(tl_len, 6);
    TEST_ASSERT_EQ(memcmp(tl_val, "pid_val", 6), 0);
    
    task_local_clear(1234);
    tl_val = task_local_get(1234, "pid_key", &tl_len);
    TEST_ASSERT_NULL(tl_val);
    
    TEST_END();
    TEST_SUMMARY();
}

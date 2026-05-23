/* KITE Test - Memory */
#include "test.h"
#include "memory/memory.h"
#include "memory/tlsf.h"

tlsf_t kite_rox_mem = 0;
tlsf_t kite_rw_mem = 0;

int kite_printk(const char *fmt, ...)
{
    (void)fmt;
    return 0;
}

int main(void)
{
    TEST_BEGIN("memory");
    
    // simulate pool init
    
    char pool_rw[4096] __attribute__((aligned(16)));
    char pool_rox[4096] __attribute__((aligned(16)));
    
    kite_rw_mem = tlsf_create(pool_rw, sizeof(pool_rw));
    kite_rox_mem = tlsf_create(pool_rox, sizeof(pool_rox));
    
    TEST_ASSERT_NOT_NULL(kite_rw_mem);
    TEST_ASSERT_NOT_NULL(kite_rox_mem);
    
    // execmem_alloc test
    void *p1 = execmem_alloc(64);
    TEST_ASSERT_NOT_NULL(p1);
    
    void *p2 = execmem_alloc(128);
    TEST_ASSERT_NOT_NULL(p2);
    
    execmem_free(p1);
    execmem_free(p2);
    
    TEST_END();
    TEST_SUMMARY();
}

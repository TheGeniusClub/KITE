/* KITE Test - TLSF */
#include "test.h"
#include "memory/tlsf.h"

int main(void)
{
    TEST_BEGIN("tlsf");
    
    char pool[4096] __attribute__((aligned(16)));
    
    tlsf_t t = tlsf_create(pool, sizeof(pool));
    TEST_ASSERT_NOT_NULL(t);
    
    void *p1 = tlsf_malloc(t, 64);
    TEST_ASSERT_NOT_NULL(p1);
    
    void *p2 = tlsf_malloc(t, 128);
    TEST_ASSERT_NOT_NULL(p2);
    
    void *p3 = tlsf_malloc(t, 256);
    TEST_ASSERT_NOT_NULL(p3);
    
    // write pattern
    memset(p1, 0x11, 64);
    memset(p2, 0x22, 128);
    memset(p3, 0x33, 256);
    
    TEST_ASSERT_EQ(((unsigned char*)p1)[0], 0x11);
    TEST_ASSERT_EQ(((unsigned char*)p2)[0], 0x22);
    TEST_ASSERT_EQ(((unsigned char*)p3)[0], 0x33);
    
    tlsf_free(t, p2);
    
    void *p4 = tlsf_malloc(t, 128);
    TEST_ASSERT_NOT_NULL(p4);
    
    tlsf_free(t, p1);
    tlsf_free(t, p3);
    tlsf_free(t, p4);
    
    TEST_END();
    TEST_SUMMARY();
}

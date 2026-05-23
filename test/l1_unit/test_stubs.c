/* KITE Test - Stubs */
#include "test.h"
#include "stubs.h"

int main(void)
{
    TEST_BEGIN("stubs");
    
    // memset
    char buf[16];
    kite_memset(buf, 0xAA, 16);
    TEST_ASSERT_EQ(buf[0], (char)0xAA);
    TEST_ASSERT_EQ(buf[15], (char)0xAA);
    
    // memcpy
    char src[] = "hello";
    char dst[16] = {0};
    kite_memcpy(dst, src, 6);
    TEST_ASSERT_EQ(kite_strcmp(dst, "hello"), 0);
    
    // memcmp
    TEST_ASSERT_EQ(kite_memcmp("abc", "abc", 3), 0);
    TEST_ASSERT(kite_memcmp("abc", "abd", 3) != 0);
    
    // strlen
    TEST_ASSERT_EQ(kite_strlen("hello"), 5);
    TEST_ASSERT_EQ(kite_strlen(""), 0);
    
    // strcpy
    char s1[16];
    kite_strcpy(s1, "world");
    TEST_ASSERT_EQ(kite_strcmp(s1, "world"), 0);
    
    // strncpy
    char s2[16] = {0};
    kite_strncpy(s2, "test", 3);
    TEST_ASSERT_EQ(s2[0], 't');
    TEST_ASSERT_EQ(s2[2], 's');
    
    // snprintf
    char fmt[64];
    int n = kite_snprintf(fmt, sizeof(fmt), "val=%d hex=%x ptr=%p", 42, 0xABCD, (void*)0x1234);
    TEST_ASSERT(n > 0);
    
    TEST_END();
    TEST_SUMMARY();
}

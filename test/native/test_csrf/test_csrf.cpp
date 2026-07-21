#include <unity.h>
#include "csrf.h"
void test_empty_origin_allowed() { TEST_ASSERT_TRUE(originAllowed("", "dev.local")); }
void test_same_origin_allowed() { TEST_ASSERT_TRUE(originAllowed("http://dev.local", "dev.local")); }
void test_same_origin_with_port() { TEST_ASSERT_TRUE(originAllowed("http://dev.local:80", "dev.local:80")); }
void test_cross_origin_rejected() { TEST_ASSERT_FALSE(originAllowed("http://evil.com", "dev.local")); }
void setUp() {} void tearDown() {}
int main(int, char**) { UNITY_BEGIN();
  RUN_TEST(test_empty_origin_allowed); RUN_TEST(test_same_origin_allowed);
  RUN_TEST(test_same_origin_with_port); RUN_TEST(test_cross_origin_rejected); return UNITY_END(); }

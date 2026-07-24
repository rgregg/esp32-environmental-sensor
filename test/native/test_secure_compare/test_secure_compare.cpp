#include <unity.h>
#include "secure_compare.h"

void test_equal() { TEST_ASSERT_TRUE(constantTimeEquals("s3cr3t-token", "s3cr3t-token")); }
void test_differ_same_length() { TEST_ASSERT_FALSE(constantTimeEquals("abcdef", "abcXef")); }
void test_differ_length() { TEST_ASSERT_FALSE(constantTimeEquals("abc", "abcd")); }
void test_empty_equal() { TEST_ASSERT_TRUE(constantTimeEquals("", "")); }
void test_empty_vs_nonempty() { TEST_ASSERT_FALSE(constantTimeEquals("", "x")); }

void setUp() {} void tearDown() {}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_equal);
  RUN_TEST(test_differ_same_length);
  RUN_TEST(test_differ_length);
  RUN_TEST(test_empty_equal);
  RUN_TEST(test_empty_vs_nonempty);
  return UNITY_END();
}

#include <unity.h>
#include "html_escape.h"
void test_escapes_all() {
  TEST_ASSERT_EQUAL_STRING("a&lt;b&gt;&amp;&quot;&#39;", htmlEscape("a<b>&\"'").c_str());
}
void test_plain_unchanged() {
  TEST_ASSERT_EQUAL_STRING("plain text 42", htmlEscape("plain text 42").c_str());
}
void setUp() {} void tearDown() {}
int main(int, char**) { UNITY_BEGIN(); RUN_TEST(test_escapes_all); RUN_TEST(test_plain_unchanged); return UNITY_END(); }

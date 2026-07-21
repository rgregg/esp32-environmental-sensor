#include <unity.h>
#include "reading.h"

void test_add_and_find_reading() {
  ReadingSet set;
  addReading(set, "temperature", 21.5, "C");
  addReading(set, "humidity", 48.0, "%");

  TEST_ASSERT_EQUAL_size_t(2, set.size());
  const Reading* t = findReading(set, "temperature");
  TEST_ASSERT_NOT_NULL(t);
  TEST_ASSERT_EQUAL_DOUBLE(21.5, t->value);
  TEST_ASSERT_EQUAL_STRING("C", t->unit.c_str());
}

void test_find_missing_returns_null() {
  ReadingSet set;
  addReading(set, "temperature", 21.5, "C");
  TEST_ASSERT_NULL(findReading(set, "pressure"));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_add_and_find_reading);
  RUN_TEST(test_find_missing_returns_null);
  return UNITY_END();
}

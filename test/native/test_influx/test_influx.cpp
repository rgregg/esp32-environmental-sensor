#include <unity.h>
#include "influx_format.h"

void test_basic_line() {
  ReadingSet s;
  addReading(s, "temperature", 21.5, "C");
  addReading(s, "humidity", 48.0, "%");
  std::string line = formatLineProtocol("environment", "attic", s);
  TEST_ASSERT_EQUAL_STRING(
    "environment,device=attic temperature=21.50,humidity=48.00",
    line.c_str());
}

void test_empty_readings_is_empty() {
  ReadingSet s;
  TEST_ASSERT_EQUAL_STRING("", formatLineProtocol("environment", "attic", s).c_str());
}

void test_device_tag_escaped() {
  ReadingSet s;
  addReading(s, "temperature", 20.0, "C");
  std::string line = formatLineProtocol("environment", "living room", s);
  TEST_ASSERT_EQUAL_STRING(
    "environment,device=living\\ room temperature=20.00",
    line.c_str());
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_basic_line);
  RUN_TEST(test_empty_readings_is_empty);
  RUN_TEST(test_device_tag_escaped);
  return UNITY_END();
}

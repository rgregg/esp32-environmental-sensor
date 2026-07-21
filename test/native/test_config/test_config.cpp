#include <unity.h>
#include "config.h"

void test_defaults_round_trip() {
  Config c;                       // defaults
  std::string json = configToJson(c);
  Config back = configFromJson(json);
  TEST_ASSERT_EQUAL_UINT32(30, back.publishIntervalSec);
  TEST_ASSERT_TRUE(back.mqttEnabled);
  TEST_ASSERT_FALSE(back.httpEnabled);
  TEST_ASSERT_EQUAL_STRING("env", back.mqttBaseTopic.c_str());
  TEST_ASSERT_EQUAL_STRING("homeassistant", back.mqttDiscoveryPrefix.c_str());
}

void test_overrides_persist() {
  Config c;
  c.deviceName = "attic";
  c.publishIntervalSec = 60;
  c.mqttHost = "10.0.0.5";
  c.httpEnabled = true;
  c.httpUrl = "http://influx:8086/write?db=env";
  std::string json = configToJson(c);
  Config back = configFromJson(json);
  TEST_ASSERT_EQUAL_STRING("attic", back.deviceName.c_str());
  TEST_ASSERT_EQUAL_UINT32(60, back.publishIntervalSec);
  TEST_ASSERT_EQUAL_STRING("10.0.0.5", back.mqttHost.c_str());
  TEST_ASSERT_TRUE(back.httpEnabled);
  TEST_ASSERT_EQUAL_STRING("http://influx:8086/write?db=env", back.httpUrl.c_str());
}

void test_malformed_json_yields_defaults() {
  Config back = configFromJson("not json {{{");
  TEST_ASSERT_EQUAL_UINT32(30, back.publishIntervalSec);
  TEST_ASSERT_TRUE(back.mqttEnabled);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_defaults_round_trip);
  RUN_TEST(test_overrides_persist);
  RUN_TEST(test_malformed_json_yields_defaults);
  return UNITY_END();
}

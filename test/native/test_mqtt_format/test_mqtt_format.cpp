#include <unity.h>
#include <ArduinoJson.h>
#include "mqtt_format.h"

void test_state_topic() {
  TEST_ASSERT_EQUAL_STRING("env/attic/temperature",
    stateTopic("env", "attic", "temperature").c_str());
}

void test_discovery_topic() {
  TEST_ASSERT_EQUAL_STRING("homeassistant/sensor/attic_temperature/config",
    discoveryTopic("homeassistant", "attic", "temperature").c_str());
}

void test_discovery_payload_temperature() {
  Reading r{"temperature", 21.5, "C"};
  std::string json = discoveryPayload("attic", r, "env/attic/temperature");
  JsonDocument d;
  deserializeJson(d, json);
  TEST_ASSERT_EQUAL_STRING("attic_temperature", d["unique_id"]);
  TEST_ASSERT_EQUAL_STRING("env/attic/temperature", d["state_topic"]);
  TEST_ASSERT_EQUAL_STRING("temperature", d["device_class"]);
  TEST_ASSERT_EQUAL_STRING("measurement", d["state_class"]);
  TEST_ASSERT_EQUAL_STRING("C", d["unit_of_measurement"]);
}

void test_discovery_payload_unknown_has_no_device_class() {
  Reading r{"weirdmetric", 1.0, "x"};
  std::string json = discoveryPayload("attic", r, "env/attic/weirdmetric");
  JsonDocument d;
  deserializeJson(d, json);
  TEST_ASSERT_FALSE(d["device_class"].is<const char*>());
  TEST_ASSERT_EQUAL_STRING("measurement", d["state_class"]);
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_state_topic);
  RUN_TEST(test_discovery_topic);
  RUN_TEST(test_discovery_payload_temperature);
  RUN_TEST(test_discovery_payload_unknown_has_no_device_class);
  return UNITY_END();
}

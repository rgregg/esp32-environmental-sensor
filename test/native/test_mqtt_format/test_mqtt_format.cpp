#include <unity.h>
#include <ArduinoJson.h>
#include "mqtt_format.h"
#include "config.h"

void test_state_topic() {
  TEST_ASSERT_EQUAL_STRING("env/attic/temperature",
    stateTopic("env", "attic", "temperature").c_str());
}

void test_discovery_topic() {
  TEST_ASSERT_EQUAL_STRING("homeassistant/sensor/attic_temperature/config",
    discoveryTopic("homeassistant", "attic", "temperature").c_str());
}

void test_discovery_payload_temperature() {
  Reading r{"temperature", 21.5, "°C"};
  std::string json = discoveryPayload("attic", r, "env/attic/temperature");
  JsonDocument d;
  deserializeJson(d, json);
  TEST_ASSERT_EQUAL_STRING("attic_temperature", d["unique_id"]);
  TEST_ASSERT_EQUAL_STRING("env/attic/temperature", d["state_topic"]);
  TEST_ASSERT_EQUAL_STRING("temperature", d["device_class"]);
  TEST_ASSERT_EQUAL_STRING("measurement", d["state_class"]);
  TEST_ASSERT_EQUAL_STRING("°C", d["unit_of_measurement"]);
}

void test_discovery_payload_unknown_has_no_device_class() {
  Reading r{"weirdmetric", 1.0, "x"};
  std::string json = discoveryPayload("attic", r, "env/attic/weirdmetric");
  JsonDocument d;
  deserializeJson(d, json);
  TEST_ASSERT_FALSE(d["device_class"].is<const char*>());
  TEST_ASSERT_EQUAL_STRING("measurement", d["state_class"]);
}

// Home Assistant's volatile_organic_compounds class is for ug/m3; ppb readings
// need the _parts variant or HA rejects the unit.
void test_discovery_payload_tvoc_ppb_uses_parts_class() {
  Reading r{"tvoc", 3.0, "ppb"};
  std::string json = discoveryPayload("attic", r, "env/attic/tvoc");
  JsonDocument d;
  deserializeJson(d, json);
  TEST_ASSERT_EQUAL_STRING("volatile_organic_compounds_parts", d["device_class"]);
  TEST_ASSERT_EQUAL_STRING("ppb", d["unit_of_measurement"]);
}

void test_discovery_payload_eco2() {
  Reading r{"eco2", 400.0, "ppm"};
  std::string json = discoveryPayload("attic", r, "env/attic/eco2");
  JsonDocument d;
  deserializeJson(d, json);
  TEST_ASSERT_EQUAL_STRING("carbon_dioxide", d["device_class"]);
}

void test_broker_unchanged_for_unrelated_fields() {
  Config a, b;
  a.mqttHost = b.mqttHost = "10.0.0.5";
  b.publishIntervalSec = 99;
  b.mqttBaseTopic = "other";
  b.deviceName = "renamed";
  TEST_ASSERT_FALSE(mqttConnectionSettingsChanged(a, b));
}

void test_broker_changed_for_connection_fields() {
  Config base;
  base.mqttHost = "10.0.0.5";
  Config c = base; c.mqttHost = "";
  TEST_ASSERT_TRUE(mqttConnectionSettingsChanged(base, c));
  c = base; c.mqttPort = 1884;
  TEST_ASSERT_TRUE(mqttConnectionSettingsChanged(base, c));
  c = base; c.mqttUser = "u";
  TEST_ASSERT_TRUE(mqttConnectionSettingsChanged(base, c));
  c = base; c.mqttPassword = "p";
  TEST_ASSERT_TRUE(mqttConnectionSettingsChanged(base, c));
  c = base; c.mqttEnabled = !base.mqttEnabled;
  TEST_ASSERT_TRUE(mqttConnectionSettingsChanged(base, c));
}

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_state_topic);
  RUN_TEST(test_discovery_topic);
  RUN_TEST(test_discovery_payload_temperature);
  RUN_TEST(test_discovery_payload_unknown_has_no_device_class);
  RUN_TEST(test_discovery_payload_tvoc_ppb_uses_parts_class);
  RUN_TEST(test_discovery_payload_eco2);
  RUN_TEST(test_broker_unchanged_for_unrelated_fields);
  RUN_TEST(test_broker_changed_for_connection_fields);
  return UNITY_END();
}

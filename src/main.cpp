#include <Arduino.h>
#include "net.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("esp32-environmental-sensor: boot");
  if (netBegin("esp32-env-test")) {
    Serial.printf("Ethernet up, IP=%s\n", netIp().c_str());
    Serial.println("mDNS: esp32-env-test.local");
  } else {
    Serial.println("Ethernet FAILED to get IP");
  }
}

void loop() {
  delay(5000);
  Serial.printf("link=%d ip=%s\n", netConnected(), netIp().c_str());
}

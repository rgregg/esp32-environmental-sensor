#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("esp32-environmental-sensor: boot");
}

void loop() {
  delay(1000);
}

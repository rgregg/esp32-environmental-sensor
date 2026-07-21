#include "bme280_sensor.h"

bool Bme280Sensor::begin() {
  present_ = dev_.begin(0x76) || dev_.begin(0x77);
  return present_;
}

bool Bme280Sensor::read() {
  if (!present_) return false;
  lastTemp_ = dev_.readTemperature();
  lastHum_ = dev_.readHumidity();
  float pressure = dev_.readPressure() / 100.0f;  // Pa -> hPa
  haveEnv_ = true;
  readings_.clear();
  addReading(readings_, "temperature", lastTemp_, "C");
  addReading(readings_, "humidity", lastHum_, "%");
  addReading(readings_, "pressure", pressure, "hPa");
  return true;
}

bool Bme280Sensor::hasEnv(float& tempC, float& humidity) const {
  if (!haveEnv_) return false;
  tempC = lastTemp_; humidity = lastHum_;
  return true;
}

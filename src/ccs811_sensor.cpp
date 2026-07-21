#include "ccs811_sensor.h"

bool Ccs811Sensor::begin() {
  present_ = dev_.begin(0x5A);
  return present_;
}

void Ccs811Sensor::setEnvironmentalData(float tempC, float humidity) {
  if (present_) dev_.setEnvironmentalData(humidity, tempC);
}

bool Ccs811Sensor::read() {
  if (!present_ || !dev_.available()) return false;
  if (dev_.readData() != 0) return false;   // nonzero = error
  readings_.clear();
  addReading(readings_, "eco2", dev_.geteCO2(), "ppm");
  addReading(readings_, "tvoc", dev_.getTVOC(), "ppb");
  return true;
}

#include "ccs811_sensor.h"

bool Ccs811Sensor::begin() {
  present_ = dev_.begin(0x5A);
  // Pulse the hotplate once a minute (drive mode 3) instead of continuously
  // (mode 1): the constant heater warmed the BME280 on the same module by ~1.8 °C
  // inside the enclosure. eCO2/TVOC then update once a minute.
  if (present_) dev_.setDriveMode(CCS811_DRIVE_MODE_60SEC);
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

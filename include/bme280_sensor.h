#pragma once
#include "sensor.h"
#include <Adafruit_BME280.h>

class Bme280Sensor : public Sensor {
public:
  bool begin() override;
  bool present() const override { return present_; }
  bool read() override;
  const ReadingSet& readings() const override { return readings_; }
  const char* name() const override { return "BME280"; }
  bool hasEnv(float& tempC, float& humidity) const;

private:
  Adafruit_BME280 dev_;
  bool present_ = false;
  float lastTemp_ = 0, lastHum_ = 0;
  bool haveEnv_ = false;
  ReadingSet readings_;
};

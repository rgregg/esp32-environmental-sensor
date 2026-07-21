#pragma once
#include "sensor.h"
#include <Adafruit_CCS811.h>

class Ccs811Sensor : public Sensor {
public:
  bool begin() override;
  bool present() const override { return present_; }
  bool read() override;
  const ReadingSet& readings() const override { return readings_; }
  const char* name() const override { return "CCS811"; }
  void setEnvironmentalData(float tempC, float humidity);

private:
  Adafruit_CCS811 dev_;
  bool present_ = false;
  ReadingSet readings_;
};

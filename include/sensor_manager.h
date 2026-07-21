#pragma once
#include <memory>
#include <vector>
#include <string>
#include "sensor.h"
#include "bme280_sensor.h"
#include "ccs811_sensor.h"

class SensorManager {
public:
  void begin();
  void poll();
  const ReadingSet& snapshot() const { return snapshot_; }
  std::vector<std::string> detected() const;

private:
  void probe();
  std::unique_ptr<Bme280Sensor> bme_;
  std::unique_ptr<Ccs811Sensor> ccs_;
  ReadingSet snapshot_;
  uint32_t lastProbeMs_ = 0;
};

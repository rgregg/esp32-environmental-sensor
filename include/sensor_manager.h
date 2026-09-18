#pragma once
#include <memory>
#include <mutex>
#include <vector>
#include <string>
#include "sensor.h"
#include "bme280_sensor.h"
#include "ccs811_sensor.h"

// poll() runs on the Arduino loop task while the async web server reads
// snapshot()/detected() from the AsyncTCP task, so both return copies taken
// under mu_ rather than references into state poll() is mutating.
class SensorManager {
public:
  void begin();
  void poll();
  ReadingSet snapshot() const;
  std::vector<std::string> detected() const;

private:
  void probe();
  std::unique_ptr<Bme280Sensor> bme_;
  std::unique_ptr<Ccs811Sensor> ccs_;
  ReadingSet snapshot_;
  mutable std::mutex mu_;
  uint32_t lastProbeMs_ = 0;
  uint32_t lastReadMs_ = 0;
  bool readOnce_ = false;
};

#include "sensor_manager.h"
#include <Arduino.h>
#include <Wire.h>

void SensorManager::begin() {
  Wire.begin(13, 16);   // UEXT SDA=13, SCL=16
  probe();
}

void SensorManager::probe() {
  if (!bme_) {
    auto b = std::make_unique<Bme280Sensor>();
    if (b->begin()) bme_ = std::move(b);
  }
  lastProbeMs_ = millis();
}

void SensorManager::poll() {
  if (millis() - lastProbeMs_ > 30000) probe();
  snapshot_.clear();
  if (bme_ && bme_->read()) {
    for (const auto& r : bme_->readings()) snapshot_.push_back(r);
  }
}

std::vector<std::string> SensorManager::detected() const {
  std::vector<std::string> out;
  if (bme_) out.push_back(bme_->name());
  return out;
}

#include "sensor_manager.h"
#include <Arduino.h>
#include <Wire.h>

// The CCS811 produces a new sample once per second (drive mode 1), so there is
// no point touching the bus more often than that.
static constexpr uint32_t kReadIntervalMs = 1000;

void SensorManager::begin() {
  Wire.begin(13, 16);   // UEXT SDA=13, SCL=16
  probe();
}

void SensorManager::probe() {
  std::unique_ptr<Bme280Sensor> newBme;
  std::unique_ptr<Ccs811Sensor> newCcs;
  if (!bme_) {
    auto b = std::make_unique<Bme280Sensor>();
    if (b->begin()) newBme = std::move(b);
  }
  if (!ccs_) {
    auto c = std::make_unique<Ccs811Sensor>();
    if (c->begin()) newCcs = std::move(c);
  }
  if (newBme || newCcs) {
    std::lock_guard<std::mutex> lock(mu_);
    if (newBme) bme_ = std::move(newBme);
    if (newCcs) ccs_ = std::move(newCcs);
  }
  lastProbeMs_ = millis();
}

void SensorManager::poll() {
  if (millis() - lastProbeMs_ > 30000) probe();
  if (readOnce_ && millis() - lastReadMs_ < kReadIntervalMs) return;
  readOnce_ = true;
  lastReadMs_ = millis();

  // Each sensor keeps its last good values; a sensor with no fresh sample this
  // round (the CCS811 between measurements) still contributes its previous one.
  if (bme_) bme_->read();
  if (ccs_) {
    float t, h;
    if (bme_ && bme_->hasEnv(t, h)) ccs_->setEnvironmentalData(t, h);
    ccs_->read();
  }

  ReadingSet next;
  if (bme_) for (const auto& r : bme_->readings()) next.push_back(r);
  if (ccs_) for (const auto& r : ccs_->readings()) next.push_back(r);
  std::lock_guard<std::mutex> lock(mu_);
  snapshot_ = std::move(next);
}

ReadingSet SensorManager::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  return snapshot_;
}

std::vector<std::string> SensorManager::detected() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::string> out;
  if (bme_) out.push_back(bme_->name());
  if (ccs_) out.push_back(ccs_->name());
  return out;
}

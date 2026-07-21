#pragma once
#include "publisher.h"

class HttpPublisher : public Publisher {
public:
  void configure(const Config& cfg) override { cfg_ = cfg; }
  void loop() override {}
  void publish(const std::string& device, const ReadingSet& readings) override;
  bool connected() const override { return lastOk_; }

private:
  Config cfg_;
  bool lastOk_ = false;
};

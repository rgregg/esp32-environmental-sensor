#pragma once
#include <string>
#include "reading.h"
#include "config.h"

class Publisher {
public:
  virtual ~Publisher() = default;
  virtual void configure(const Config& cfg) = 0;
  virtual void loop() = 0;
  virtual void publish(const std::string& device, const ReadingSet& readings) = 0;
  virtual bool connected() const = 0;
};

#pragma once
#include "reading.h"

class Sensor {
public:
  virtual ~Sensor() = default;
  virtual bool begin() = 0;
  virtual bool present() const = 0;
  virtual bool read() = 0;
  virtual const ReadingSet& readings() const = 0;
  virtual const char* name() const = 0;
};

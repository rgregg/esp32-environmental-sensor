#pragma once
#include <string>
#include <vector>

struct Reading {
  std::string name;
  double value;
  std::string unit;
};

using ReadingSet = std::vector<Reading>;

inline void addReading(ReadingSet& set, const std::string& name,
                       double value, const std::string& unit) {
  set.push_back(Reading{name, value, unit});
}

inline const Reading* findReading(const ReadingSet& set, const std::string& name) {
  for (const auto& r : set) {
    if (r.name == name) return &r;
  }
  return nullptr;
}

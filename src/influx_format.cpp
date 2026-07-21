#include "influx_format.h"
#include <cstdio>

static std::string escapeTag(const std::string& in) {
  std::string out;
  for (char ch : in) {
    if (ch == ' ' || ch == ',' || ch == '=') out += '\\';
    out += ch;
  }
  return out;
}

static std::string fmtValue(double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.2f", v);
  return std::string(buf);
}

std::string formatLineProtocol(const std::string& measurement,
                               const std::string& device,
                               const ReadingSet& readings) {
  if (readings.empty()) return "";
  std::string line = measurement + ",device=" + escapeTag(device) + " ";
  bool first = true;
  for (const auto& r : readings) {
    if (!first) line += ",";
    line += r.name + "=" + fmtValue(r.value);
    first = false;
  }
  return line;
}

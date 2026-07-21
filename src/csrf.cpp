#include "csrf.h"
static std::string originHost(const std::string& origin) {
  auto p = origin.find("://");
  if (p == std::string::npos) return origin;
  std::string rest = origin.substr(p + 3);
  auto slash = rest.find('/');
  if (slash != std::string::npos) rest = rest.substr(0, slash);
  return rest;
}
bool originAllowed(const std::string& origin, const std::string& host) {
  if (origin.empty()) return true;
  return originHost(origin) == host;
}

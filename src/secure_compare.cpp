#include "secure_compare.h"

bool constantTimeEquals(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  unsigned char diff = 0;
  for (size_t i = 0; i < a.size(); i++)
    diff |= (unsigned char)a[i] ^ (unsigned char)b[i];
  return diff == 0;
}

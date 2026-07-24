#pragma once
#include <cstdint>

class RateLimiter {
public:
  static constexpr uint8_t kMaxFailures = 5;
  static constexpr uint32_t kCooldownMs = 60000;
  static constexpr int kSlots = 8;

  bool allowed(uint32_t ip, uint32_t nowMs) const;
  void recordFailure(uint32_t ip, uint32_t nowMs);
  void recordSuccess(uint32_t ip);

private:
  struct Slot {
    uint32_t ip = 0;
    uint8_t fails = 0;
    uint32_t lockUntilMs = 0;
    uint32_t lastMs = 0;
    bool used = false;
  };
  Slot slots_[kSlots];

  const Slot* find(uint32_t ip) const;
  Slot* find(uint32_t ip);
  Slot* allocate(uint32_t ip, uint32_t nowMs);
};

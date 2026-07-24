#include "rate_limiter.h"

const RateLimiter::Slot* RateLimiter::find(uint32_t ip) const {
  for (const auto& s : slots_) if (s.used && s.ip == ip) return &s;
  return nullptr;
}

RateLimiter::Slot* RateLimiter::find(uint32_t ip) {
  for (auto& s : slots_) if (s.used && s.ip == ip) return &s;
  return nullptr;
}

RateLimiter::Slot* RateLimiter::allocate(uint32_t ip, uint32_t nowMs) {
  Slot* victim = nullptr;
  for (auto& s : slots_) {
    if (!s.used) { victim = &s; break; }
    if (!victim || s.lastMs < victim->lastMs) victim = &s;  // least-recently-used
  }
  victim->ip = ip;
  victim->fails = 0;
  victim->lockUntilMs = 0;
  victim->lastMs = nowMs;
  victim->used = true;
  return victim;
}

bool RateLimiter::allowed(uint32_t ip, uint32_t nowMs) const {
  const Slot* s = find(ip);
  if (!s) return true;
  return nowMs >= s->lockUntilMs;   // lockUntilMs==0 for un-locked slots
}

void RateLimiter::recordFailure(uint32_t ip, uint32_t nowMs) {
  Slot* s = find(ip);
  if (!s) s = allocate(ip, nowMs);
  s->lastMs = nowMs;
  if (nowMs < s->lockUntilMs) return;   // already locked
  s->fails++;
  if (s->fails >= kMaxFailures) {
    s->lockUntilMs = nowMs + kCooldownMs;
    s->fails = 0;
  }
}

void RateLimiter::recordSuccess(uint32_t ip) {
  Slot* s = find(ip);
  if (s) { s->used = false; s->fails = 0; s->lockUntilMs = 0; }
}

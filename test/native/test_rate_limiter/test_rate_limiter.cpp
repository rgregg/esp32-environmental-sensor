#include <unity.h>
#include "rate_limiter.h"

void test_fresh_ip_allowed() {
  RateLimiter rl;
  TEST_ASSERT_TRUE(rl.allowed(0x0A000001, 1000));
}

void test_locks_after_max_failures() {
  RateLimiter rl;
  uint32_t ip = 0x0A000001;
  for (int i = 0; i < RateLimiter::kMaxFailures - 1; i++) rl.recordFailure(ip, 1000);
  TEST_ASSERT_TRUE(rl.allowed(ip, 1000));         // still allowed at 4 fails
  rl.recordFailure(ip, 1000);                     // 5th fail -> locked
  TEST_ASSERT_FALSE(rl.allowed(ip, 1000));
}

void test_unlocks_after_cooldown() {
  RateLimiter rl;
  uint32_t ip = 0x0A000001;
  for (int i = 0; i < RateLimiter::kMaxFailures; i++) rl.recordFailure(ip, 1000);
  TEST_ASSERT_FALSE(rl.allowed(ip, 1000));
  TEST_ASSERT_FALSE(rl.allowed(ip, 1000 + RateLimiter::kCooldownMs - 1));
  TEST_ASSERT_TRUE(rl.allowed(ip, 1000 + RateLimiter::kCooldownMs + 1));
}

void test_success_resets_failures() {
  RateLimiter rl;
  uint32_t ip = 0x0A000001;
  for (int i = 0; i < RateLimiter::kMaxFailures - 1; i++) rl.recordFailure(ip, 1000);
  rl.recordSuccess(ip);
  for (int i = 0; i < RateLimiter::kMaxFailures - 1; i++) rl.recordFailure(ip, 2000);
  TEST_ASSERT_TRUE(rl.allowed(ip, 2000));         // not locked: counter was reset
}

void test_per_ip_isolation() {
  RateLimiter rl;
  uint32_t a = 0x0A000001, b = 0x0A000002;
  for (int i = 0; i < RateLimiter::kMaxFailures; i++) rl.recordFailure(a, 1000);
  TEST_ASSERT_FALSE(rl.allowed(a, 1000));
  TEST_ASSERT_TRUE(rl.allowed(b, 1000));          // b unaffected
}

void test_lru_eviction_no_overflow() {
  RateLimiter rl;
  // Touch more distinct IPs than there are slots; must not crash / must keep working.
  for (int i = 0; i < RateLimiter::kSlots + 4; i++)
    rl.recordFailure(0x0A000000 + i, 1000 + i);
  // A brand-new IP is still allowed (table handled overflow via eviction).
  TEST_ASSERT_TRUE(rl.allowed(0x0AFF00FF, 5000));
}

void setUp() {} void tearDown() {}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_fresh_ip_allowed);
  RUN_TEST(test_locks_after_max_failures);
  RUN_TEST(test_unlocks_after_cooldown);
  RUN_TEST(test_success_resets_failures);
  RUN_TEST(test_per_ip_isolation);
  RUN_TEST(test_lru_eviction_no_overflow);
  return UNITY_END();
}

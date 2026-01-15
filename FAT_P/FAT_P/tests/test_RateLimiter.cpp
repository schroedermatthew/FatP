/**
 * @file test_RateLimiter.cpp
 * @brief Comprehensive unit tests for RateLimiter.h
 */
/*
FATP_META:
  meta_version: 1
  component: RateLimiter
  file_role: test
  path: tests/test_RateLimiter.cpp
  namespace: fat_p::testing::ratelimiter
  summary: "Unit tests for RateLimiter."
  related:
    docs_search: "RateLimiter"
    headers:
      - fat_p/RateLimiter.h
      - fat_p/FatPTest.h
  hygiene:
    pragma_once: false
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <iostream>
#include <thread>

#include "FatPTest.h"
#include "RateLimiter.h"

namespace fat_p::testing::ratelimiter
{

FATP_TEST_CASE(token_bucket_basic)
{
    TokenBucketRateLimiter limiter(10.0, 10.0); // 10 tokens/sec, capacity 10

    // Should have initial tokens
    for (int i = 0; i < 10; ++i)
    {
        FATP_ASSERT_TRUE(limiter.try_acquire(), "Should acquire initial tokens");
    }

    // Should be exhausted
    FATP_ASSERT_TRUE(!limiter.try_acquire(), "Should be rate limited");

    return true;
}

FATP_TEST_CASE(token_bucket_refill)
{
    TokenBucketRateLimiter limiter(100.0, 10.0); // Fast refill

    // Exhaust tokens
    for (int i = 0; i < 10; ++i)
    {
        limiter.try_acquire();
    }

    // Wait for refill
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Should have refilled
    FATP_ASSERT_TRUE(limiter.try_acquire(), "Should refill over time");

    return true;
}

FATP_TEST_CASE(sliding_window)
{
    SlidingWindowRateLimiter limiter(5, std::chrono::seconds(1));

    // Should allow up to 5 requests
    for (int i = 0; i < 5; ++i)
    {
        FATP_ASSERT_TRUE(limiter.try_acquire(), "Should allow requests up to limit");
    }

    // Should deny 6th request
    FATP_ASSERT_TRUE(!limiter.try_acquire(), "Should deny beyond limit");

    // Wait for window to slide
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // Should allow new requests
    FATP_ASSERT_TRUE(limiter.try_acquire(), "Should allow after window slides");

    return true;
}

FATP_TEST_CASE(leaky_bucket)
{
    LeakyBucketRateLimiter limiter(10.0, 10); // 10 req/sec, capacity 10

    // Fill bucket
    for (int i = 0; i < 10; ++i)
    {
        FATP_ASSERT_TRUE(limiter.try_acquire(), "Should fill bucket");
    }

    // Should be full
    FATP_ASSERT_TRUE(!limiter.try_acquire(), "Bucket should be full");

    // Wait for leak
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Should have space now
    FATP_ASSERT_TRUE(limiter.try_acquire(), "Bucket should have leaked");

    return true;
}

void benchmark_rate_limiters()
{
    std::cout << "\n" << colors::cyan() << "RateLimiter Benchmarks:" << colors::reset() << "\n\n";

    // Token bucket
    {
        TokenBucketRateLimiter limiter(1000000.0, 1000.0);
        double time = measure_perf(
            [&]()
            {
                limiter.try_acquire();
            },
            10000,
            100);
        std::cout << "TokenBucket try_acquire: " << format_time(time) << "\n";
    }

    // Sliding window
    {
        SlidingWindowRateLimiter limiter(10000, std::chrono::seconds(1));
        double time = measure_perf(
            [&]()
            {
                limiter.try_acquire();
            },
            1000,
            10);
        std::cout << "SlidingWindow try_acquire: " << format_time(time) << "\n";
    }
}

} // namespace fat_p::testing::ratelimiter

namespace fat_p::testing
{

bool test_RateLimiter()
{
    FATP_PRINT_HEADER(RATE LIMITER)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, ratelimiter, token_bucket_basic);
    FATP_RUN_TEST_NS(runner, ratelimiter, token_bucket_refill);
    FATP_RUN_TEST_NS(runner, ratelimiter, sliding_window);
    FATP_RUN_TEST_NS(runner, ratelimiter, leaky_bucket);

    ratelimiter::benchmark_rate_limiters();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_RateLimiter() ? 0 : 1;
}
#endif

#include <iostream>
#include <thread>

#include "RateLimiter.h"
#include "test_RateLimiter.h"
#include "test_Utilities.h"

namespace cpp_utilities::testing
{

bool test_token_bucket_basic() {
    TokenBucketRateLimiter limiter(10.0, 10.0);  // 10 tokens/sec, capacity 10
    
    // Should have initial tokens
    for (int i = 0; i < 10; ++i) {
        SIMPLE_ASSERT(limiter.try_acquire(), "Should acquire initial tokens");
    }
    
    // Should be exhausted
    SIMPLE_ASSERT(!limiter.try_acquire(), "Should be rate limited");
    
    return true;
}

bool test_token_bucket_refill() {
    TokenBucketRateLimiter limiter(100.0, 10.0);  // Fast refill
    
    // Exhaust tokens
    for (int i = 0; i < 10; ++i) {
        limiter.try_acquire();
    }
    
    // Wait for refill
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Should have refilled
    SIMPLE_ASSERT(limiter.try_acquire(), "Should refill over time");
    
    return true;
}

bool test_sliding_window() {
    SlidingWindowRateLimiter limiter(5, std::chrono::seconds(1));
    
    // Should allow up to 5 requests
    for (int i = 0; i < 5; ++i) {
        SIMPLE_ASSERT(limiter.try_acquire(), "Should allow requests up to limit");
    }
    
    // Should deny 6th request
    SIMPLE_ASSERT(!limiter.try_acquire(), "Should deny beyond limit");
    
    // Wait for window to slide
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    // Should allow new requests
    SIMPLE_ASSERT(limiter.try_acquire(), "Should allow after window slides");
    
    return true;
}

bool test_leaky_bucket() {
    LeakyBucketRateLimiter limiter(10.0, 10);  // 10 req/sec, capacity 10
    
    // Fill bucket
    for (int i = 0; i < 10; ++i) {
        SIMPLE_ASSERT(limiter.try_acquire(), "Should fill bucket");
    }
    
    // Should be full
    SIMPLE_ASSERT(!limiter.try_acquire(), "Bucket should be full");
    
    // Wait for leak
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // Should have space now
    SIMPLE_ASSERT(limiter.try_acquire(), "Bucket should have leaked");
    
    return true;
}

void benchmark_rate_limiters() {
    std::cout << "\n" << colors::cyan() << "RateLimiter Benchmarks:" << colors::reset() << "\n\n";
    
    // Token bucket
    {
        TokenBucketRateLimiter limiter(1000000.0, 1000.0);
        double time = measure_perf([&]() {
            limiter.try_acquire();
        }, 10000, 100);
        std::cout << "TokenBucket try_acquire: " << format_time(time) << "\n";
    }
    
    // Sliding window
    {
        SlidingWindowRateLimiter limiter(10000, std::chrono::seconds(1));
        double time = measure_perf([&]() {
            limiter.try_acquire();
        }, 1000, 10);
        std::cout << "SlidingWindow try_acquire: " << format_time(time) << "\n";
    }
}

bool test_RateLimiter() {

    PRINT_HEADER(RATE LIMITER)

    TestRunner runner;

    RUN_TEST(runner, token_bucket_basic);
    RUN_TEST(runner, token_bucket_refill);
    RUN_TEST(runner, sliding_window);
    RUN_TEST(runner, leaky_bucket);

    benchmark_rate_limiters();

    return 0 == runner.print_summary();
}

} // namespace cpp_utilities::testing

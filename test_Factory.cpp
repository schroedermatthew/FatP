#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>

#include "Factory.h"
#include "test_Factory.h"
#include "test_Utilities.h"

/**
 * @file test_Factory.cpp (v3.0 - Corrected)
 * @brief Comprehensive test suite for cpp_utilities::Factory v3.0
 * 
 * Tests cover all v3.0 features:
 * - make() instead of create() (better naming)
 * - Lambda captures for runtime parameters
 * - Atomic statistics
 * - Exception handling
 * - All policy variations
 * - Concurrency validation
 * - Performance benchmarks
 */

using namespace cpp_utilities;
using namespace cpp_utilities::testing;

namespace cpp_utilities::testing {

// ============================================================================
// Test Fixtures and Helper Classes
// ============================================================================

/**
 * @brief Simple test class for factory
 */
class Widget {
public:
    int value_;
    
    Widget() : value_(0) {}
    explicit Widget(int v) : value_(v) {}
    
    bool operator==(const Widget& other) const {
        return value_ == other.value_;
    }
    
    friend std::ostream& operator<<(std::ostream& os, const Widget& w) {
        return os << "Widget(" << w.value_ << ")";
    }
};

/**
 * @brief Widget with constructor parameters (for lambda capture testing)
 */
class ConfiguredWidget {
public:
    std::string name_;
    int value_;
    
    ConfiguredWidget(const std::string& name, int value) 
        : name_(name), value_(value) {}
    
    bool operator==(const ConfiguredWidget& other) const {
        return name_ == other.name_ && value_ == other.value_;
    }
};

/**
 * @brief Database connection for lambda capture testing
 */
class DatabaseConnection {
public:
    std::string type_;
    std::string host_;
    int port_;
    
    DatabaseConnection(const std::string& type, const std::string& host, int port)
        : type_(type), host_(host), port_(port) {}
    
    bool connect() { return true; }
};

/**
 * @brief Class that tracks construction/destruction
 */
class TrackedObject {
public:
    static inline std::atomic<int> construction_count{ 0 };
    static inline std::atomic<int> destruction_count{ 0 };

    int id_;
    bool moved_from_ = false;  // Track if this object was moved from

    explicit TrackedObject(int id = 0) : id_(id) {
        ++construction_count;
    }

    ~TrackedObject() {
        // Only count destruction if not moved-from
        if (!moved_from_) {
            ++destruction_count;
        }
    }

    TrackedObject(const TrackedObject& other) : id_(other.id_) {
        ++construction_count;
    }

    // Move constructor - doesn't increment count (transferring ownership)
    TrackedObject(TrackedObject&& other) noexcept : id_(other.id_) {
        other.moved_from_ = true;  // Mark source as moved-from
        other.id_ = -1;
    }

    TrackedObject& operator=(const TrackedObject&) = default;

    TrackedObject& operator=(TrackedObject&& other) noexcept {
        if (this != &other) {
            id_ = other.id_;
            moved_from_ = false;
            other.moved_from_ = true;
            other.id_ = -1;
        }
        return *this;
    }

    static void reset_counts() {
        construction_count = 0;
        destruction_count = 0;
    }
};

/**
 * @brief Class that can throw during construction
 */
class ThrowingWidget {
public:
    static inline std::atomic<bool> should_throw{false};
    
    ThrowingWidget() {
        if (should_throw) {
            throw std::runtime_error("Construction failed");
        }
    }
};

/**
 * @brief Movable-only widget
 */
class MovableWidget {
public:
    int value_;
    
    explicit MovableWidget(int v) : value_(v) {}
    
    MovableWidget(const MovableWidget&) = delete;
    MovableWidget& operator=(const MovableWidget&) = delete;
    
    MovableWidget(MovableWidget&& other) noexcept : value_(other.value_) {
        other.value_ = -1;
    }
    
    MovableWidget& operator=(MovableWidget&& other) noexcept {
        value_ = other.value_;
        other.value_ = -1;
        return *this;
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

bool test_basic_registration() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Basic Registration Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    
    // Register a creator
    bool registered = factory.registerType("widget1", [] { return Widget(42); });
    SIMPLE_ASSERT(registered, "First registration should succeed");
    
    // Check it exists
    SIMPLE_ASSERT(factory.hasType("widget1"), "Registered type should exist");
    SIMPLE_ASSERT(!factory.hasType("nonexistent"), "Unregistered type should not exist");
    
    // Check size
    ASSERT_EQ(factory.size(), 1u, "Factory should have 1 registration");
    SIMPLE_ASSERT(!factory.empty(), "Factory should not be empty");
    
    out << colors::green() << "✓ All registration checks passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_basic_make() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Basic Make Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    (void)factory.registerType("widget1", [] { return Widget(42); });
    (void)factory.registerType("widget2", [] { return Widget(100); });
    
    // Make registered types
    auto result1 = factory.make("widget1");
    SIMPLE_ASSERT(result1.has_value(), "Make should succeed for registered type");
    ASSERT_EQ(result1->value_, 42, "Widget should have correct value");
    
    auto result2 = factory.make("widget2");
    SIMPLE_ASSERT(result2.has_value(), "Second make should succeed");
    ASSERT_EQ(result2->value_, 100, "Second widget should have correct value");
    
    // Try to make unregistered type
    auto result3 = factory.make("nonexistent");
    SIMPLE_ASSERT(!result3.has_value(), "Make should fail for unregistered type");
    ASSERT_EQ(result3.error().code, FactoryError::KeyNotFound, 
              "Error code should be KeyNotFound");
    
    out << colors::green() << "✓ All make checks passed" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_lambda_capture_parameters() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Lambda Capture Parameters Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, ConfiguredWidget> factory;
    
    // Register with captured parameters - different for each key
    std::string name1 = "widget1";
    int value1 = 42;
    [[maybe_unused]] bool reg1 = factory.registerType("basic", [name1, value1]() {
        return ConfiguredWidget(name1, value1);
    });
    
    std::string name2 = "widget2";
    int value2 = 100;
    [[maybe_unused]] bool reg2 = factory.registerType("advanced", [name2, value2]() {
        return ConfiguredWidget(name2 + "_advanced", value2 * 2);
    });
    
    // Make with captured parameters
    auto result1 = factory.make("basic");
    SIMPLE_ASSERT(result1.has_value(), "Should make with captured parameters");
    ASSERT_EQ(result1->name_, std::string("widget1"), "Name should match captured");
    ASSERT_EQ(result1->value_, 42, "Value should match captured");
    
    auto result2 = factory.make("advanced");
    SIMPLE_ASSERT(result2.has_value(), "Should make advanced");
    ASSERT_EQ(result2->name_, std::string("widget2_advanced"), "Name should be modified");
    ASSERT_EQ(result2->value_, 200, "Value should be doubled");
    
    out << colors::green() << "✓ Lambda capture parameters work correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_database_connection_lambda_capture() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Database Connection Lambda Capture Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, DatabaseConnection> db_factory;
    
    // Helper to create closures with captured parameters
    auto makeDbCreator = [](std::string type, std::string host, int port) {
        return [type, host, port]() {
            return DatabaseConnection(type, host, port);
        };
    };
    
    // Register different database types with captured parameters
    [[maybe_unused]] bool reg1 = db_factory.registerType("postgres-dev", 
        makeDbCreator("PostgreSQL", "localhost", 5432));
    
    [[maybe_unused]] bool reg2 = db_factory.registerType("postgres-prod",
        makeDbCreator("PostgreSQL", "prod-server", 5432));
    
    [[maybe_unused]] bool reg3 = db_factory.registerType("mysql-dev",
        makeDbCreator("MySQL", "localhost", 3306));
    
    // Make with different captured parameters
    auto pg_dev = db_factory.make("postgres-dev");
    SIMPLE_ASSERT(pg_dev.has_value(), "Should make postgres dev connection");
    ASSERT_EQ(pg_dev->type_, std::string("PostgreSQL"), "Type should be PostgreSQL");
    ASSERT_EQ(pg_dev->host_, std::string("localhost"), "Host should be localhost");
    ASSERT_EQ(pg_dev->port_, 5432, "Port should match");
    
    auto pg_prod = db_factory.make("postgres-prod");
    SIMPLE_ASSERT(pg_prod.has_value(), "Should make postgres prod connection");
    ASSERT_EQ(pg_prod->host_, std::string("prod-server"), "Prod host should differ");
    
    auto mysql = db_factory.make("mysql-dev");
    SIMPLE_ASSERT(mysql.has_value(), "Should make mysql connection");
    ASSERT_EQ(mysql->type_, std::string("MySQL"), "Type should be MySQL");
    ASSERT_EQ(mysql->port_, 3306, "MySQL port should match");
    
    out << colors::green() << "✓ Database connection with lambda captures works" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_duplicate_registration() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Duplicate Registration Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    
    // Register first time
    bool first = factory.registerType("widget", [] { return Widget(1); });
    SIMPLE_ASSERT(first, "First registration should succeed");
    
    // Try to register again (should fail with PreventOverwrite)
    bool second = factory.registerType("widget", [] { return Widget(2); });
    SIMPLE_ASSERT(!second, "Second registration should fail (prevent overwrite)");
    
    // Verify original creator is still used
    auto result = factory.make("widget");
    SIMPLE_ASSERT(result.has_value(), "Make should succeed");
    ASSERT_EQ(result->value_, 1, "Should use first creator (not overwritten)");
    
    out << colors::green() << "✓ Duplicate registration prevented correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_unregister() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Unregister Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    
    (void)factory.registerType("widget1", [] { return Widget(1); });
    (void)factory.registerType("widget2", [] { return Widget(2); });
    
    ASSERT_EQ(factory.size(), 2u, "Should have 2 registrations");
    
    // Unregister one
    bool removed = factory.unregisterType("widget1");
    SIMPLE_ASSERT(removed, "Unregister should succeed");
    ASSERT_EQ(factory.size(), 1u, "Should have 1 registration after unregister");
    SIMPLE_ASSERT(!factory.hasType("widget1"), "Unregistered type should not exist");
    SIMPLE_ASSERT(factory.hasType("widget2"), "Other type should still exist");
    
    // Try to unregister nonexistent
    bool removed2 = factory.unregisterType("nonexistent");
    SIMPLE_ASSERT(!removed2, "Unregister of nonexistent should fail");
    
    out << colors::green() << "✓ Unregister works correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_clear() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Clear Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    
    (void)factory.registerType("widget1", [] { return Widget(1); });
    (void)factory.registerType("widget2", [] { return Widget(2); });
    
    ASSERT_EQ(factory.size(), 2u, "Should have 2 registrations");
    
    factory.clear();
    
    ASSERT_EQ(factory.size(), 0u, "Should have 0 registrations after clear");
    SIMPLE_ASSERT(factory.empty(), "Factory should be empty");
    SIMPLE_ASSERT(!factory.hasType("widget1"), "Cleared type should not exist");
    
    // Stats should also be reset
    auto stats = factory.getStats();
    ASSERT_EQ(stats.registrations, 0u, "Stats should be reset");
    
    out << colors::green() << "✓ Clear works correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_get_registered_keys() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Get Registered Keys Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    
    (void)factory.registerType("widget1", [] { return Widget(1); });
    (void)factory.registerType("widget2", [] { return Widget(2); });
    (void)factory.registerType("widget3", [] { return Widget(3); });
    
    auto keys = factory.getRegisteredKeys();
    
    ASSERT_EQ(keys.size(), 3u, "Should return 3 keys");
    
    // Check all keys are present (order may vary)
    bool has_widget1 = std::find(keys.begin(), keys.end(), "widget1") != keys.end();
    bool has_widget2 = std::find(keys.begin(), keys.end(), "widget2") != keys.end();
    bool has_widget3 = std::find(keys.begin(), keys.end(), "widget3") != keys.end();
    
    SIMPLE_ASSERT(has_widget1 && has_widget2 && has_widget3, 
                  "All keys should be present");
    
    out << colors::green() << "✓ Get registered keys works correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// NEW: Exception Handling Tests (CRITICAL - was missing)
// ============================================================================

bool test_throwing_make() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Throwing Make Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, ThrowingWidget> factory;
    (void)factory.registerType("thrower", [] { return ThrowingWidget(); });
    
    // Test successful make
    ThrowingWidget::should_throw = false;
    auto ok_result = factory.make("thrower");
    SIMPLE_ASSERT(ok_result.has_value(), 
                  "Make should succeed when not throwing");
    
    // Test throwing make
    ThrowingWidget::should_throw = true;
    auto fail_result = factory.make("thrower");
    SIMPLE_ASSERT(!fail_result.has_value(), 
                  "Make should fail when throwing");
    ASSERT_EQ(fail_result.error().code, FactoryError::CreationFailed,
              "Error code should be CreationFailed");
    
    std::string msg = fail_result.error().full_message();
    out << "Error message: " << msg << std::endl;
    SIMPLE_ASSERT(msg.find("Construction failed") != std::string::npos,
                  "Error should contain exception message");
    
    out << colors::green() << "✓ Exception handling works correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// NEW: Statistics Tests (CRITICAL - was incomplete)
// ============================================================================

bool test_statistics() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Statistics Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    (void)factory.registerType("widget", [] { return Widget(42); });
    
    // Perform various operations
    auto result1 = factory.make("widget");        // 1 lookup, 1 resolution
    (void)factory.hasType("widget");              // 2 lookups
    auto result2 = factory.make("nonexistent");   // 3 lookups, 1 failure
    
    auto stats = factory.getStats();
    
    ASSERT_EQ(stats.registrations, 1u, "Should have 1 registration");
    ASSERT_EQ(stats.resolutions, 1u, "Should have 1 successful resolution");
    ASSERT_EQ(stats.resolution_failures, 1u, "Should have 1 failed resolution");
    ASSERT_EQ(stats.lookups, 3u, "Should have 3 lookups total");
    
    out << "Statistics:\n"
        << "  Registrations: " << stats.registrations << "\n"
        << "  Resolutions: " << stats.resolutions << "\n"
        << "  Failures: " << stats.resolution_failures << "\n"
        << "  Lookups: " << stats.lookups << "\n";
    
    out << colors::green() << "✓ Statistics tracking works correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// NEW: Policy Tests (CRITICAL - was missing)
// ============================================================================

bool test_overwrite_policy() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Overwrite Policy Test ===" 
        << colors::reset() << std::endl;
    
    using OverwriteFactory = Factory<std::string, Widget,
        SingleThreadedPolicy,
        ExpectedErrorPolicy<Widget, std::string>,
        AllowOverwritePolicy,
        MapStoragePolicy<std::string, std::function<Widget()>>,
        InstanceLifetimePolicy>;
    
    OverwriteFactory factory;
    
    // First registration
    bool first = factory.registerType("widget", [] { return Widget(1); });
    SIMPLE_ASSERT(first, "First registration should succeed");
    
    auto result1 = factory.make("widget");
    SIMPLE_ASSERT(result1.has_value(), "Should make widget");
    ASSERT_EQ(result1->value_, 1, "Should use first creator");
    
    // Overwrite (should succeed with AllowOverwritePolicy)
    bool second = factory.registerType("widget", [] { return Widget(2); });
    SIMPLE_ASSERT(!second, "Returns false for overwrite");
    
    auto result2 = factory.make("widget");
    SIMPLE_ASSERT(result2.has_value(), "Should still make widget");
    ASSERT_EQ(result2->value_, 2, "Should use overwritten creator");
    
    out << colors::green() << "✓ Overwrite policy works correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_unordered_map_storage() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== UnorderedMap Storage Test ===" 
        << colors::reset() << std::endl;
    
    FastFactory<std::string, Widget> factory;  // Uses UnorderedMapStoragePolicy
    
    // Register many types
    for (int i = 0; i < 100; ++i) {
        (void)factory.registerType("widget" + std::to_string(i), 
                             [i] { return Widget(i); });
    }
    
    ASSERT_EQ(factory.size(), 100u, "Should have 100 registrations");
    
    // Verify retrieval
    auto result = factory.make("widget42");
    SIMPLE_ASSERT(result.has_value(), "Should make widget42");
    ASSERT_EQ(result->value_, 42, "Should have correct value");
    
    // Get all keys (order not guaranteed)
    auto keys = factory.getRegisteredKeys();
    ASSERT_EQ(keys.size(), 100u, "Should return all keys");
    
    out << colors::green() << "✓ UnorderedMap storage works correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// NEW: Concurrency Tests (CRITICAL - was completely missing!)
// ============================================================================

bool test_concurrent_access() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Concurrent Access Test ===" 
        << colors::reset() << std::endl;
    
    ThreadSafeFactory<std::string, Widget> factory;
    
    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::thread> threads;
    
    constexpr int NUM_THREADS = 10;
    constexpr int OPS_PER_THREAD = 100;
    
    // Spawn threads that register and make concurrently
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&factory, t, &success_count, &failure_count] {
            for (int i = 0; i < OPS_PER_THREAD; ++i) {
                std::string key = "widget_" + std::to_string(t) + "_" + std::to_string(i);
                
                // Register
                bool registered = factory.registerType(key, [t, i] { 
                    return Widget(t * 1000 + i); 
                });
                
                if (registered) {
                    ++success_count;
                    
                    // Immediately make
                    auto result = factory.make(key);
                    if (result.has_value()) {
                        ++success_count;
                    } else {
                        ++failure_count;
                    }
                }
            }
        });
    }
    
    // Wait for completion
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify no data corruption
    size_t expected_registrations = NUM_THREADS * OPS_PER_THREAD;
    ASSERT_EQ(factory.size(), expected_registrations, 
              "All registrations should succeed without races");
    
    ASSERT_EQ(success_count.load(), static_cast<int>(expected_registrations * 2),
              "All operations should succeed (register + make)");
    
    ASSERT_EQ(failure_count.load(), 0,
              "No failures should occur");
    
    out << "Completed " << success_count << " operations across " 
        << NUM_THREADS << " threads\n";
    
    out << colors::green() << "✓ Concurrent access handled correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_concurrent_read_write() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Concurrent Read/Write Test ===" 
        << colors::reset() << std::endl;
    
    ThreadSafeFactory<int, Widget> factory;
    
    // Pre-register some types
    for (int i = 0; i < 100; ++i) {
        [[maybe_unused]] bool registered = factory.registerType(i, [i] { return Widget(i); });
    }
    
    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};
    
    // Reader threads (many)
    std::vector<std::thread> readers;
    for (int i = 0; i < 8; ++i) {
        readers.emplace_back([&factory, &stop, &read_count] {
            while (!stop) {
                int key = read_count.load() % 100;
                auto result = factory.make(key);
                if (result.has_value()) {
                    ++read_count;
                }
            }
        });
    }
    
    // Writer threads (few)
    std::vector<std::thread> writers;
    for (int i = 0; i < 2; ++i) {
        writers.emplace_back([&factory, &stop, &write_count, i] {
            int counter = 100;
            while (!stop && counter < 200) {
                (void)factory.registerType(counter++, [i] { return Widget(i); });
                ++write_count;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        });
    }
    
    // Run for a short time
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop = true;
    
    for (auto& t : readers) t.join();
    for (auto& t : writers) t.join();
    
    out << "Read operations: " << read_count << "\n";
    out << "Write operations: " << write_count << "\n";
    
    SIMPLE_ASSERT(read_count > 1000, "Should perform many reads");
    SIMPLE_ASSERT(write_count > 0, "Should perform some writes");
    
    // Verify no corruption
    SIMPLE_ASSERT(factory.size() >= 100, "Should have at least initial registrations");
    
    out << colors::green() << "✓ Concurrent read/write works" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// NEW: Batch Registration Test
// ============================================================================

bool test_batch_registration() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Batch Registration Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    
    // Register multiple types at once
    size_t registered = factory.registerTypes({
        {"widget1", [] { return Widget(1); }},
        {"widget2", [] { return Widget(2); }},
        {"widget3", [] { return Widget(3); }}
    });
    
    ASSERT_EQ(registered, 3u, "Should register 3 types");
    ASSERT_EQ(factory.size(), 3u, "Factory should have 3 registrations");
    
    // Verify all are makeable
    auto r1 = factory.make("widget1");
    auto r2 = factory.make("widget2");
    auto r3 = factory.make("widget3");
    
    SIMPLE_ASSERT(r1.has_value() && r2.has_value() && r3.has_value(),
                  "All should make successfully");
    
    ASSERT_EQ(r1->value_, 1, "Widget1 value correct");
    ASSERT_EQ(r2->value_, 2, "Widget2 value correct");
    ASSERT_EQ(r3->value_, 3, "Widget3 value correct");
    
    out << colors::green() << "✓ Batch registration works correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

// ============================================================================
// Advanced Features Tests
// ============================================================================

bool test_lambda_with_captures() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Lambda with Captures Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    
    int captured_value = 99;
    (void)factory.registerType("captured", [captured_value] { 
        return Widget(captured_value); 
    });
    
    auto result = factory.make("captured");
    SIMPLE_ASSERT(result.has_value(), "Should make captured lambda");
    ASSERT_EQ(result->value_, 99, "Should use captured value");
    
    out << colors::green() << "✓ Lambda captures work correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_movable_only_types() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Movable-Only Types Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, MovableWidget> factory;
    
    (void)factory.registerType("movable", [] { return MovableWidget(42); });
    
    auto result = factory.make("movable");
    SIMPLE_ASSERT(result.has_value(), "Should make movable-only type");
    ASSERT_EQ(result->value_, 42, "Value should be correct");
    
    // Move the result
    MovableWidget moved = std::move(*result);
    ASSERT_EQ(moved.value_, 42, "Moved value should be correct");
    ASSERT_EQ(result->value_, -1, "Original should be moved-from");
    
    out << colors::green() << "✓ Movable-only types work correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_unique_ptr_factory() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== unique_ptr Factory Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, std::unique_ptr<Widget>> factory;
    
    (void)factory.registerType("unique_widget", [] { 
        return std::make_unique<Widget>(42); 
    });
    
    auto result = factory.make("unique_widget");
    SIMPLE_ASSERT(result.has_value(), "Should make unique_ptr");
    SIMPLE_ASSERT(*result != nullptr, "unique_ptr should not be null");
    ASSERT_EQ((*result)->value_, 42, "Widget value should be correct");
    
    out << colors::green() << "✓ unique_ptr factory works correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_integer_keys() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Integer Keys Test ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<int, Widget> factory;
    
    (void)factory.registerType(1, [] { return Widget(10); });
    (void)factory.registerType(2, [] { return Widget(20); });
    
    auto result1 = factory.make(1);
    auto result2 = factory.make(2);
    
    SIMPLE_ASSERT(result1.has_value(), "Should make key 1");
    SIMPLE_ASSERT(result2.has_value(), "Should make key 2");
    ASSERT_EQ(result1->value_, 10, "Key 1 value correct");
    ASSERT_EQ(result2->value_, 20, "Key 2 value correct");
    
    out << colors::green() << "✓ Integer keys work correctly" 
        << colors::reset() << std::endl;
    
    return true;
}

bool test_tracked_object_lifecycle() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Tracked Object Lifecycle Test ==="
        << colors::reset() << std::endl;

    TrackedObject::reset_counts();

    {
        SimpleFactory<std::string, TrackedObject> factory;
        (void)factory.registerType("tracked", [] { return TrackedObject(1); });

        {
            auto result = factory.make("tracked");
            SIMPLE_ASSERT(result.has_value(), "Should make tracked object");

            // With move constructor (doesn't increment count) + RVO, expect 1 construction
            // Without RVO, might be 2 (original + copy if move is elided)
            int constructions = TrackedObject::construction_count.load();
            out << "  Constructions: " << constructions << "\n";

            SIMPLE_ASSERT(constructions >= 1 && constructions <= 2,
                "Should have 1-2 constructions (depending on copy elision)");
        }

        // Object(s) should be destroyed when result goes out of scope
        int destructions = TrackedObject::destruction_count.load();
        out << "  Destructions: " << destructions << "\n";

        // Construction and destruction counts should match
        ASSERT_EQ(TrackedObject::construction_count.load(),
            destructions,
            "Construction and destruction counts should match");
    }

    out << colors::green() << "✓ Object lifecycle tracked correctly"
        << colors::reset() << std::endl;

    return true;
}

// ============================================================================
// Performance Benchmarks
// ============================================================================

void benchmark_basic_make() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Basic Make Benchmark ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    (void)factory.registerType("widget", [] { return Widget(42); });
    
    benchmark("Factory make", [&factory] {
        auto result = factory.make("widget");
    }, 1000000);
}

void benchmark_lambda_capture_make() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Lambda Capture Make Benchmark ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, ConfiguredWidget> factory;
    
    std::string name = "test";
    int value = 42;
    (void)factory.registerType("widget", [name, value]() {
        return ConfiguredWidget(name, value);
    });
    
    benchmark("Lambda capture make", [&factory] {
        auto result = factory.make("widget");
    }, 1000000);
}

void benchmark_registration() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Registration Benchmark ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    int counter = 0;
    
    benchmark("Factory registration", [&factory, &counter] {
        (void)factory.registerType("widget" + std::to_string(counter++), [] {
            return Widget(42);
        });
    }, 10000);
}

void benchmark_lookup() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Lookup Benchmark ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    (void)factory.registerType("widget", [] { return Widget(42); });
    
    benchmark("Factory hasType", [&factory] {
        (void)factory.hasType("widget");
    }, 1000000);
}

void benchmark_vs_direct_creation() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Factory vs Direct Creation ===" 
        << colors::reset() << std::endl;
    
    SimpleFactory<std::string, Widget> factory;
    (void)factory.registerType("widget", [] { return Widget(42); });
    
    benchmark_compare(
        "Direct creation",
        [] { Widget w(42); },
        "Factory make",
        [&factory] { auto result = factory.make("widget"); },
        1000000
    );
}

void benchmark_map_vs_unordered() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Map vs UnorderedMap Benchmark ===" 
        << colors::reset() << std::endl;
    
    // Create both factory types
    SimpleFactory<std::string, Widget> map_factory;
    FastFactory<std::string, Widget> unordered_factory;
    
    // Register 1000 types in each
    for (int i = 0; i < 1000; ++i) {
        std::string key = "widget" + std::to_string(i);
        [[maybe_unused]] bool r1 = map_factory.registerType(key, [i] { return Widget(i); });
        [[maybe_unused]] bool r2 = unordered_factory.registerType(key, [i] { return Widget(i); });
    }
    
    // Compare make performance
    benchmark_compare(
        "Map storage (1000 items)",
        [&map_factory] { 
            auto r = map_factory.make("widget500"); 
        },
        "UnorderedMap storage (1000 items)",
        [&unordered_factory] { 
            auto r = unordered_factory.make("widget500"); 
        },
        100000
    );
}

void benchmark_map_size_impact() {
    auto& out = *get_test_config().output;
    out << colors::cyan() << "\n=== Map Size Impact Benchmark ===" 
        << colors::reset() << std::endl;
    
    // Small factory
    SimpleFactory<std::string, Widget> small_factory;
    for (int i = 0; i < 10; ++i) {
        (void)small_factory.registerType("widget" + std::to_string(i), 
                                   [i] { return Widget(i); });
    }
    
    // Large factory
    SimpleFactory<std::string, Widget> large_factory;
    for (int i = 0; i < 1000; ++i) {
        (void)large_factory.registerType("widget" + std::to_string(i), 
                                   [i] { return Widget(i); });
    }
    
    benchmark_compare(
        "Small factory (10 items)",
        [&small_factory] { auto r = small_factory.make("widget5"); },
        "Large factory (1000 items)",
        [&large_factory] { auto r = large_factory.make("widget500"); },
        100000
    );
}

void run_factory_benchmarks() {
    auto& out = *get_test_config().output;
    out << "\n" << colors::bold() << colors::cyan()
        << "=== Factory Performance Benchmarks ==="
        << colors::reset() << std::endl;
    
    benchmark_basic_make();
    benchmark_lambda_capture_make();
    benchmark_registration();
    benchmark_lookup();
    benchmark_vs_direct_creation();
    benchmark_map_vs_unordered();
    benchmark_map_size_impact();
}

// ============================================================================
// Main Test Runner
// ============================================================================

bool test_Factory() {
    TestRunner runner;
    
    auto& out = *get_test_config().output;
    out << colors::bold() << colors::cyan()
        << "\n================================================\n"
        << "  Factory Test Suite v3.0\n"
        << "  - make() for clear semantics\n"
        << "  - Lambda captures for runtime params\n"
        << "  - All bug fixes applied\n"
        << "================================================\n"
        << colors::reset() << std::endl;
        
    // Basic functionality
    out << "\n" << colors::bold() << "=== Basic Functionality Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Basic Registration", test_basic_registration);
    runner.run_test("Basic Make", test_basic_make);
    runner.run_test("Lambda Capture Parameters", test_lambda_capture_parameters);
    runner.run_test("Database Connection Lambda Capture", test_database_connection_lambda_capture);
    runner.run_test("Duplicate Registration", test_duplicate_registration);
    runner.run_test("Unregister", test_unregister);
    runner.run_test("Clear", test_clear);
    runner.run_test("Get Registered Keys", test_get_registered_keys);
    
    // Exception and statistics
    out << "\n" << colors::bold() << "=== Exception & Statistics Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Throwing Make", test_throwing_make);
    runner.run_test("Statistics Tracking", test_statistics);
    
    // Policy tests
    out << "\n" << colors::bold() << "=== Policy Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Overwrite Policy", test_overwrite_policy);
    runner.run_test("UnorderedMap Storage", test_unordered_map_storage);
    
    // Concurrency tests
    out << "\n" << colors::bold() << "=== Concurrency Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Concurrent Access", test_concurrent_access);
    runner.run_test("Concurrent Read/Write", test_concurrent_read_write);
    
    // Batch operations
    out << "\n" << colors::bold() << "=== Batch Operations ===" 
        << colors::reset() << std::endl;
    runner.run_test("Batch Registration", test_batch_registration);
    
    // Advanced features
    out << "\n" << colors::bold() << "=== Advanced Features Tests ===" 
        << colors::reset() << std::endl;
    runner.run_test("Lambda with Captures", test_lambda_with_captures);
    runner.run_test("Movable-Only Types", test_movable_only_types);
    runner.run_test("unique_ptr Factory", test_unique_ptr_factory);
    runner.run_test("Integer Keys", test_integer_keys);
    runner.run_test("Tracked Object Lifecycle", test_tracked_object_lifecycle);
    
    int failed = runner.print_summary();
    
    if (failed == 0) {
        run_factory_benchmarks();
    }
    
    return failed == 0;
}

} // namespace cpp_utilities::testing

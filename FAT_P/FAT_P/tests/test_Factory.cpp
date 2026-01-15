/**
 * @file test_Factory.cpp
 * @brief Comprehensive test suite for fat_p::Factory
 *
 * Tests cover all features:
 * - Snapshot pattern for re-entrant safety
 * - All error handling policies (Expected, Throwing, Default)
 * - All storage policies (Map, UnorderedMap with transparent comparators)
 * - All lifetime policies (Instance, Singleton)
 * - Variadic parameters
 * - Statistics policies
 * - Concurrency validation
 * - Performance benchmarks
 */
/*
FATP_META:
  meta_version: 1
  component: Factory
  file_role: test
  path: tests/test_Factory.cpp
  namespace: fat_p
  summary: "Unit tests for Factory."
  related:
    docs_search: "Factory"
    headers:
      - fat_p/Factory.h
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

#include <atomic>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "Factory.h"
#include "FatPTest.h"

namespace fat_p::testing::factory
{

// Import types from fat_p namespace for convenience
using fat_p::AllowOverwritePolicy;
using fat_p::AtomicStatisticsPolicy;
using fat_p::DefaultFallbackPolicy;
using fat_p::Factory;
using fat_p::FactoryError;
using fat_p::FastFactory;
using fat_p::HPCFactory;
using fat_p::InstanceLifetimePolicy;
using fat_p::MapStoragePolicy;
using fat_p::MutexSynchronizationPolicy;
using fat_p::NoStatisticsPolicy;
using fat_p::PreventOverwritePolicy;
using fat_p::SharedMutexPolicy;
using fat_p::SimpleFactory;
using fat_p::SimpleVariadicFactory;
using fat_p::SingleThreadedPolicy;
using fat_p::SingletonLifetimePolicy;
using fat_p::ThreadSafeFactory;
using fat_p::ThrowingFallbackPolicy;
using fat_p::UnorderedMapStoragePolicy;
using fat_p::factory::DefaultErrorPolicy;
using fat_p::factory::ExpectedErrorPolicy;
using fat_p::factory::ThrowingErrorPolicy;

// ============================================================================
// Test Fixtures and Helper Classes
// ============================================================================

/**
 * @brief Simple test class for factory
 */
class Widget final
{
public:
    int mValue;

    Widget()
        : mValue(0)
    {
    }
    explicit Widget(int v)
        : mValue(v)
    {
    }

    bool operator==(const Widget& other) const
    {
        return mValue == other.mValue;
    }

    friend std::ostream& operator<<(std::ostream& os, const Widget& w)
    {
        return os << "Widget(" << w.mValue << ")";
    }
};

/**
 * @brief Widget with constructor parameters (for lambda capture testing)
 */
class ConfiguredWidget final
{
public:
    std::string mName;
    int mValue;

    ConfiguredWidget(const std::string& name, int value)
        : mName(name)
        , mValue(value)
    {
    }

    bool operator==(const ConfiguredWidget& other) const
    {
        return mName == other.mName && mValue == other.mValue;
    }
};

/**
 * @brief Database connection for lambda capture testing
 */
class DatabaseConnection final
{
public:
    std::string mType;
    std::string mHost;
    int mPort;

    DatabaseConnection(const std::string& type, const std::string& host, int port)
        : mType(type)
        , mHost(host)
        , mPort(port)
    {
    }

    bool connect()
    {
        return true;
    }
};

/**
 * @brief Class that tracks construction/destruction
 */
class TrackedObject final
{
public:
    static inline std::atomic<int> construction_count{0};
    static inline std::atomic<int> destruction_count{0};

    int mId;
    bool moved_from_ = false;

    explicit TrackedObject(int id = 0)
        : mId(id)
    {
        ++construction_count;
    }

    ~TrackedObject()
    {
        if (!moved_from_)
        {
            ++destruction_count;
        }
    }

    TrackedObject(const TrackedObject& other)
        : mId(other.mId)
    {
        ++construction_count;
    }

    TrackedObject(TrackedObject&& other) noexcept
        : mId(other.mId)
    {
        other.moved_from_ = true;
        other.mId = -1;
    }

    TrackedObject& operator=(const TrackedObject&) = default;

    TrackedObject& operator=(TrackedObject&& other) noexcept
    {
        if (this != &other)
        {
            mId = other.mId;
            moved_from_ = false;
            other.moved_from_ = true;
            other.mId = -1;
        }
        return *this;
    }

    static void reset_counts()
    {
        construction_count = 0;
        destruction_count = 0;
    }
};

/**
 * @brief Class that can throw during construction
 */
class ThrowingWidget final
{
public:
    static inline std::atomic<bool> should_throw{false};

    ThrowingWidget()
    {
        if (should_throw)
        {
            throw std::runtime_error("Construction failed");
        }
    }
};

/**
 * @brief Movable-only widget
 */
class MovableWidget final
{
public:
    int mValue;

    explicit MovableWidget(int v)
        : mValue(v)
    {
    }

    MovableWidget(const MovableWidget&) = delete;
    MovableWidget& operator=(const MovableWidget&) = delete;

    MovableWidget(MovableWidget&& other) noexcept
        : mValue(other.mValue)
    {
        other.mValue = -1;
    }

    MovableWidget& operator=(MovableWidget&& other) noexcept
    {
        mValue = other.mValue;
        other.mValue = -1;
        return *this;
    }
};

// ============================================================================
// Basic Functionality Tests
// ============================================================================

FATP_TEST_CASE(basic_registration)
{
    SimpleFactory<std::string, Widget> factory;

    bool registered = factory.registerType("widget1",
                                           []
                                           {
                                               return Widget(42);
                                           });
    FATP_ASSERT_TRUE(registered, "First registration should succeed");

    FATP_ASSERT_TRUE(factory.hasType("widget1"), "Registered type should exist");
    FATP_ASSERT_TRUE(!factory.hasType("nonexistent"), "Unregistered type should not exist");

    FATP_ASSERT_EQ(factory.size(), 1u, "Factory should have 1 registration");
    FATP_ASSERT_TRUE(!factory.empty(), "Factory should not be empty");

    return true;
}

FATP_TEST_CASE(basic_make)
{
    SimpleFactory<std::string, Widget> factory;
    (void)factory.registerType("widget1",
                               []
                               {
                                   return Widget(42);
                               });
    (void)factory.registerType("widget2",
                               []
                               {
                                   return Widget(100);
                               });

    auto result1 = factory.make("widget1");
    FATP_ASSERT_TRUE(result1.has_value(), "Make should succeed for registered type");
    FATP_ASSERT_EQ(result1->mValue, 42, "Widget should have correct value");

    auto result2 = factory.make("widget2");
    FATP_ASSERT_TRUE(result2.has_value(), "Second make should succeed");
    FATP_ASSERT_EQ(result2->mValue, 100, "Second widget should have correct value");

    auto result3 = factory.make("nonexistent");
    FATP_ASSERT_TRUE(!result3.has_value(), "Make should fail for unregistered type");
    FATP_ASSERT_EQ(result3.error().code, FactoryError::KeyNotFound, "Error code should be KeyNotFound");

    return true;
}

FATP_TEST_CASE(lambda_capture_parameters)
{
    SimpleFactory<std::string, ConfiguredWidget> factory;

    std::string name1 = "widget1";
    int value1 = 42;
    (void)factory.registerType("basic",
                               [name1, value1]()
                               {
                                   return ConfiguredWidget(name1, value1);
                               });

    std::string name2 = "widget2";
    int value2 = 100;
    (void)factory.registerType("advanced",
                               [name2, value2]()
                               {
                                   return ConfiguredWidget(name2 + "_advanced", value2 * 2);
                               });

    auto result1 = factory.make("basic");
    FATP_ASSERT_TRUE(result1.has_value(), "Should make with captured parameters");
    FATP_ASSERT_EQ(result1->mName, std::string("widget1"), "Name should match captured");
    FATP_ASSERT_EQ(result1->mValue, 42, "Value should match captured");

    auto result2 = factory.make("advanced");
    FATP_ASSERT_TRUE(result2.has_value(), "Should make advanced");
    FATP_ASSERT_EQ(result2->mName, std::string("widget2_advanced"), "Name should be modified");
    FATP_ASSERT_EQ(result2->mValue, 200, "Value should be doubled");

    return true;
}

FATP_TEST_CASE(database_connection_lambda_capture)
{
    SimpleFactory<std::string, DatabaseConnection> db_factory;

    auto makeDbCreator = [](std::string type, std::string host, int port)
    {
        return [type, host, port]()
        {
            return DatabaseConnection(type, host, port);
        };
    };

    (void)db_factory.registerType("postgres-dev", makeDbCreator("PostgreSQL", "localhost", 5432));
    (void)db_factory.registerType("postgres-prod", makeDbCreator("PostgreSQL", "prod-server", 5432));
    (void)db_factory.registerType("mysql-dev", makeDbCreator("MySQL", "localhost", 3306));

    auto pg_dev = db_factory.make("postgres-dev");
    FATP_ASSERT_TRUE(pg_dev.has_value(), "Should make postgres dev connection");
    FATP_ASSERT_EQ(pg_dev->mType, std::string("PostgreSQL"), "Type should be PostgreSQL");
    FATP_ASSERT_EQ(pg_dev->mHost, std::string("localhost"), "Host should be localhost");
    FATP_ASSERT_EQ(pg_dev->mPort, 5432, "Port should match");

    auto pg_prod = db_factory.make("postgres-prod");
    FATP_ASSERT_TRUE(pg_prod.has_value(), "Should make postgres prod connection");
    FATP_ASSERT_EQ(pg_prod->mHost, std::string("prod-server"), "Prod host should differ");

    auto mysql = db_factory.make("mysql-dev");
    FATP_ASSERT_TRUE(mysql.has_value(), "Should make mysql connection");
    FATP_ASSERT_EQ(mysql->mType, std::string("MySQL"), "Type should be MySQL");
    FATP_ASSERT_EQ(mysql->mPort, 3306, "MySQL port should match");

    return true;
}

FATP_TEST_CASE(duplicate_registration)
{
    SimpleFactory<std::string, Widget> factory;

    bool first = factory.registerType("widget",
                                      []
                                      {
                                          return Widget(1);
                                      });
    FATP_ASSERT_TRUE(first, "First registration should succeed");

    bool second = factory.registerType("widget",
                                       []
                                       {
                                           return Widget(2);
                                       });
    FATP_ASSERT_TRUE(!second, "Second registration should fail (prevent overwrite)");

    auto result = factory.make("widget");
    FATP_ASSERT_TRUE(result.has_value(), "Make should succeed");
    FATP_ASSERT_EQ(result->mValue, 1, "Should use first creator (not overwritten)");

    return true;
}

FATP_TEST_CASE(unregister)
{
    SimpleFactory<std::string, Widget> factory;

    (void)factory.registerType("widget1",
                               []
                               {
                                   return Widget(1);
                               });
    (void)factory.registerType("widget2",
                               []
                               {
                                   return Widget(2);
                               });

    FATP_ASSERT_EQ(factory.size(), 2u, "Should have 2 registrations");

    bool removed = factory.unregisterType("widget1");
    FATP_ASSERT_TRUE(removed, "Unregister should succeed");
    FATP_ASSERT_EQ(factory.size(), 1u, "Should have 1 registration after unregister");
    FATP_ASSERT_TRUE(!factory.hasType("widget1"), "Unregistered type should not exist");
    FATP_ASSERT_TRUE(factory.hasType("widget2"), "Other type should still exist");

    bool removed2 = factory.unregisterType("nonexistent");
    FATP_ASSERT_TRUE(!removed2, "Unregister of nonexistent should fail");

    return true;
}

FATP_TEST_CASE(clear)
{
    SimpleFactory<std::string, Widget> factory;

    (void)factory.registerType("widget1",
                               []
                               {
                                   return Widget(1);
                               });
    (void)factory.registerType("widget2",
                               []
                               {
                                   return Widget(2);
                               });

    FATP_ASSERT_EQ(factory.size(), 2u, "Should have 2 registrations");

    factory.clear();

    FATP_ASSERT_EQ(factory.size(), 0u, "Should have 0 registrations after clear");
    FATP_ASSERT_TRUE(factory.empty(), "Factory should be empty");
    FATP_ASSERT_TRUE(!factory.hasType("widget1"), "Cleared type should not exist");

    auto stats = factory.getStats();
    FATP_ASSERT_EQ(stats.registrations, 0u, "Stats should be reset");

    return true;
}

FATP_TEST_CASE(get_registered_keys)
{
    SimpleFactory<std::string, Widget> factory;

    (void)factory.registerType("widget1",
                               []
                               {
                                   return Widget(1);
                               });
    (void)factory.registerType("widget2",
                               []
                               {
                                   return Widget(2);
                               });
    (void)factory.registerType("widget3",
                               []
                               {
                                   return Widget(3);
                               });

    auto keys = factory.getRegisteredKeys();

    FATP_ASSERT_EQ(keys.size(), 3u, "Should return 3 keys");

    bool has_widget1 = std::find(keys.begin(), keys.end(), "widget1") != keys.end();
    bool has_widget2 = std::find(keys.begin(), keys.end(), "widget2") != keys.end();
    bool has_widget3 = std::find(keys.begin(), keys.end(), "widget3") != keys.end();

    FATP_ASSERT_TRUE(has_widget1 && has_widget2 && has_widget3, "All keys should be present");

    return true;
}

// ============================================================================
// Exception and Error Handling Tests
// ============================================================================

FATP_TEST_CASE(throwing_make)
{
    SimpleFactory<std::string, ThrowingWidget> factory;
    (void)factory.registerType("thrower",
                               []
                               {
                                   return ThrowingWidget();
                               });

    ThrowingWidget::should_throw = false;
    auto ok_result = factory.make("thrower");
    FATP_ASSERT_TRUE(ok_result.has_value(), "Make should succeed when not throwing");

    ThrowingWidget::should_throw = true;
    auto fail_result = factory.make("thrower");
    FATP_ASSERT_TRUE(!fail_result.has_value(), "Make should fail when throwing");
    FATP_ASSERT_EQ(fail_result.error().code, FactoryError::CreationFailed, "Error code should be CreationFailed");

    std::string msg = fail_result.error().full_message();
    FATP_ASSERT_TRUE(msg.find("Construction failed") != std::string::npos, "Error should contain exception message");

    // Reset for other tests
    ThrowingWidget::should_throw = false;

    return true;
}

FATP_TEST_CASE(throwing_error_policy)
{
    using ThrowingFactory = Factory<std::string,
                                    Widget,
                                    SingleThreadedPolicy,
                                    ThrowingErrorPolicy<Widget, std::string>,
                                    PreventOverwritePolicy,
                                    MapStoragePolicy<std::string, std::function<Widget()>>,
                                    InstanceLifetimePolicy,
                                    AtomicStatisticsPolicy>;

    ThrowingFactory factory;
    (void)factory.registerType("widget",
                               []
                               {
                                   return Widget(42);
                               });

    // Success case
    Widget w = factory.make("widget");
    FATP_ASSERT_EQ(w.mValue, 42, "Should create widget");

    // Failure case - should throw
    bool threw = false;
    try
    {
        (void)factory.make("nonexistent");
    }
    catch (const std::runtime_error& e)
    {
        threw = true;
        FATP_ASSERT_TRUE(std::string(e.what()).find("not found") != std::string::npos,
                         "Exception message should mention 'not found'");
    }
    FATP_ASSERT_TRUE(threw, "Should throw on missing key");

    return true;
}

FATP_TEST_CASE(default_error_policy)
{
    using DefaultFactory = Factory<std::string,
                                   Widget,
                                   SingleThreadedPolicy,
                                   DefaultErrorPolicy<Widget, std::string>,
                                   PreventOverwritePolicy,
                                   MapStoragePolicy<std::string, std::function<Widget()>>,
                                   InstanceLifetimePolicy,
                                   AtomicStatisticsPolicy>;

    DefaultFactory factory;
    (void)factory.registerType("widget",
                               []
                               {
                                   return Widget(42);
                               });

    // Success case
    Widget w1 = factory.make("widget");
    FATP_ASSERT_EQ(w1.mValue, 42, "Should create widget");

    // Missing key returns default-constructed Widget
    Widget w2 = factory.make("nonexistent");
    FATP_ASSERT_EQ(w2.mValue, 0, "Should return default Widget");

    return true;
}

// ============================================================================
// Statistics Tests
// ============================================================================

FATP_TEST_CASE(statistics)
{
    SimpleFactory<std::string, Widget> factory;
    (void)factory.registerType("widget",
                               []
                               {
                                   return Widget(42);
                               });

    auto result1 = factory.make("widget");      // 1 lookup, 1 resolution
    (void)factory.hasType("widget");            // 2 lookups
    auto result2 = factory.make("nonexistent"); // 3 lookups, 1 failure

    auto stats = factory.getStats();

    FATP_ASSERT_EQ(stats.registrations, 1u, "Should have 1 registration");
    FATP_ASSERT_EQ(stats.resolutions, 1u, "Should have 1 successful resolution");
    FATP_ASSERT_EQ(stats.resolution_failures, 1u, "Should have 1 failed resolution");
    FATP_ASSERT_EQ(stats.lookups, 3u, "Should have 3 lookups total");

    return true;
}

FATP_TEST_CASE(const_methods_update_stats)
{
    SimpleFactory<std::string, Widget> factory;
    (void)factory.registerType("widget",
                               []
                               {
                                   return Widget(42);
                               });
    factory.resetStats();

    // Access via const reference
    const auto& const_factory = factory;
    (void)const_factory.hasType("widget");

    auto stats = factory.getStats();
    FATP_ASSERT_EQ(stats.lookups, 1u, "Const hasType should increment lookups (mutable stats by design)");

    return true;
}

FATP_TEST_CASE(no_statistics_policy)
{
    using NoStatsFactory = Factory<std::string,
                                   Widget,
                                   SingleThreadedPolicy,
                                   ExpectedErrorPolicy<Widget, std::string>,
                                   PreventOverwritePolicy,
                                   MapStoragePolicy<std::string, std::function<Widget()>>,
                                   InstanceLifetimePolicy,
                                   NoStatisticsPolicy>;

    NoStatsFactory factory;
    (void)factory.registerType("widget",
                               []
                               {
                                   return Widget(42);
                               });
    (void)factory.make("widget");
    (void)factory.hasType("widget");
    (void)factory.make("nonexistent");

    auto stats = factory.getStats();

    // All stats should remain zero with NoStatisticsPolicy
    FATP_ASSERT_EQ(stats.registrations, 0u, "NoStats should not track registrations");
    FATP_ASSERT_EQ(stats.resolutions, 0u, "NoStats should not track resolutions");
    FATP_ASSERT_EQ(stats.lookups, 0u, "NoStats should not track lookups");
    FATP_ASSERT_EQ(stats.resolution_failures, 0u, "NoStats should not track failures");

    return true;
}

FATP_TEST_CASE(hpc_factory)
{
    // HPCFactory combines: NoStatisticsPolicy + ThrowingErrorPolicy + UnorderedMapStoragePolicy
    HPCFactory<std::string, Widget> factory;

    (void)factory.registerType("widget",
                               []
                               {
                                   return Widget(42);
                               });
    (void)factory.registerType("another",
                               []
                               {
                                   return Widget(99);
                               });

    // Success case - returns Widget directly (ThrowingErrorPolicy)
    Widget w = factory.make("widget");
    FATP_ASSERT_EQ(w.mValue, 42, "Should create widget");

    Widget w2 = factory.make("another");
    FATP_ASSERT_EQ(w2.mValue, 99, "Should create another widget");

    // Verify size works
    FATP_ASSERT_EQ(factory.size(), 2u, "Should have 2 registrations");

    // Failure case - should throw
    bool threw = false;
    try
    {
        (void)factory.make("nonexistent");
    }
    catch (const std::runtime_error& e)
    {
        threw = true;
        FATP_ASSERT_TRUE(std::string(e.what()).find("not found") != std::string::npos,
                         "Exception should mention 'not found'");
    }
    FATP_ASSERT_TRUE(threw, "HPCFactory should throw on missing key");

    return true;
}

// ============================================================================
// Policy Tests
// ============================================================================

FATP_TEST_CASE(overwrite_policy)
{
    using OverwriteFactory = Factory<std::string,
                                     Widget,
                                     SingleThreadedPolicy,
                                     ExpectedErrorPolicy<Widget, std::string>,
                                     AllowOverwritePolicy,
                                     MapStoragePolicy<std::string, std::function<Widget()>>,
                                     InstanceLifetimePolicy,
                                     AtomicStatisticsPolicy>;

    OverwriteFactory factory;

    bool first = factory.registerType("widget",
                                      []
                                      {
                                          return Widget(1);
                                      });
    FATP_ASSERT_TRUE(first, "First registration should succeed");

    auto result1 = factory.make("widget");
    FATP_ASSERT_TRUE(result1.has_value(), "Should make widget");
    FATP_ASSERT_EQ(result1->mValue, 1, "Should use first creator");

    bool second = factory.registerType("widget",
                                       []
                                       {
                                           return Widget(2);
                                       });
    FATP_ASSERT_TRUE(!second, "Returns false for overwrite");

    auto result2 = factory.make("widget");
    FATP_ASSERT_TRUE(result2.has_value(), "Should still make widget");
    FATP_ASSERT_EQ(result2->mValue, 2, "Should use overwritten creator");

    return true;
}

FATP_TEST_CASE(unordered_map_storage)
{
    FastFactory<std::string, Widget> factory;

    for (int i = 0; i < 100; ++i)
    {
        (void)factory.registerType("widget" + std::to_string(i),
                                   [i]
                                   {
                                       return Widget(i);
                                   });
    }

    FATP_ASSERT_EQ(factory.size(), 100u, "Should have 100 registrations");

    auto result = factory.make("widget42");
    FATP_ASSERT_TRUE(result.has_value(), "Should make widget42");
    FATP_ASSERT_EQ(result->mValue, 42, "Should have correct value");

    auto keys = factory.getRegisteredKeys();
    FATP_ASSERT_EQ(keys.size(), 100u, "Should return all keys");

    return true;
}

FATP_TEST_CASE(singleton_lifetime_policy)
{
    using SingletonFactory = Factory<std::string,
                                     Widget,
                                     SingleThreadedPolicy,
                                     ExpectedErrorPolicy<Widget, std::string>,
                                     PreventOverwritePolicy,
                                     MapStoragePolicy<std::string, std::function<Widget()>>,
                                     SingletonLifetimePolicy,
                                     AtomicStatisticsPolicy>;

    auto& factory1 = SingletonFactory::instance();
    auto& factory2 = SingletonFactory::instance();

    FATP_ASSERT_TRUE(&factory1 == &factory2, "Should return same instance");

    (void)factory1.registerType("singleton_test",
                                []
                                {
                                    return Widget(123);
                                });
    FATP_ASSERT_TRUE(factory2.hasType("singleton_test"), "Registration should be visible on both references");

    // Cleanup for other tests
    factory1.clear();

    return true;
}

FATP_TEST_CASE(variadic_parameters)
{
    using ParamFactory = Factory<std::string,
                                 ConfiguredWidget,
                                 SingleThreadedPolicy,
                                 ExpectedErrorPolicy<ConfiguredWidget, std::string>,
                                 PreventOverwritePolicy,
                                 MapStoragePolicy<std::string, std::function<ConfiguredWidget(std::string, int)>>,
                                 InstanceLifetimePolicy,
                                 AtomicStatisticsPolicy,
                                 std::string,
                                 int>;

    ParamFactory factory;

    (void)factory.registerType("configured",
                               [](std::string name, int value)
                               {
                                   return ConfiguredWidget(name, value);
                               });

    auto result = factory.make("configured", "test_name", 99);
    FATP_ASSERT_TRUE(result.has_value(), "Should create with parameters");
    FATP_ASSERT_EQ(result->mName, std::string("test_name"), "Name should match");
    FATP_ASSERT_EQ(result->mValue, 99, "Value should match");

    return true;
}

// ============================================================================
// Re-entrancy Test (validates Critical Issue #1 fix)
// ============================================================================

FATP_TEST_CASE(reentrant_factory_access)
{
    SimpleFactory<std::string, int> factory;

    (void)factory.registerType("child",
                               []()
                               {
                                   return 42;
                               });
    (void)factory.registerType("parent",
                               [&factory]()
                               {
                                   // This re-enters the factory from within a creator
                                   auto child = factory.make("child");
                                   return child.has_value() ? *child + 1 : -1;
                               });

    // This would deadlock/UB before the snapshot pattern fix
    auto result = factory.make("parent");
    FATP_ASSERT_TRUE(result.has_value(), "Re-entrant make should succeed");
    FATP_ASSERT_EQ(*result, 43, "Should use child value + 1");

    return true;
}

// ============================================================================
// SimpleVariadicFactory Tests (Legacy API)
// ============================================================================

FATP_TEST_CASE(simple_variadic_factory_basic)
{
    auto& factory = SimpleVariadicFactory<std::string, Widget, false>::instance();

    bool registered = factory.registerType("legacy_widget",
                                           []
                                           {
                                               return Widget(100);
                                           });
    FATP_ASSERT_TRUE(registered, "Should register in legacy factory");

    Widget w = factory.create("legacy_widget");
    FATP_ASSERT_EQ(w.mValue, 100, "Should create via legacy API");

    factory.clear();
    return true;
}

FATP_TEST_CASE(simple_variadic_factory_params)
{
    using ParamLegacyFactory = SimpleVariadicFactory<std::string,
                                                     ConfiguredWidget,
                                                     false,
                                                     ThrowingFallbackPolicy<ConfiguredWidget>,
                                                     std::string,
                                                     int>;

    auto& factory = ParamLegacyFactory::instance();

    (void)factory.registerType("param_widget",
                               [](std::string name, int value)
                               {
                                   return ConfiguredWidget(name, value);
                               });

    ConfiguredWidget w = factory.create("param_widget", "legacy_name", 55);
    FATP_ASSERT_EQ(w.mName, std::string("legacy_name"), "Name should match");
    FATP_ASSERT_EQ(w.mValue, 55, "Value should match");

    factory.clear();
    return true;
}

FATP_TEST_CASE(simple_variadic_factory_reentrant)
{
    auto& factory = SimpleVariadicFactory<std::string, int, false>::instance();

    (void)factory.registerType("child",
                               []()
                               {
                                   return 10;
                               });
    (void)factory.registerType("parent",
                               [&factory]()
                               {
                                   // Re-entrant access
                                   return factory.create("child") * 2;
                               });

    int result = factory.create("parent");
    FATP_ASSERT_EQ(result, 20, "Re-entrant create should work");

    factory.clear();
    return true;
}

// ============================================================================
// Concurrency Tests
// ============================================================================

FATP_TEST_CASE(concurrent_access)
{
    ThreadSafeFactory<std::string, Widget> factory;

    std::atomic<int> success_count{0};
    std::atomic<int> failure_count{0};
    std::vector<std::thread> threads;

    constexpr int NUM_THREADS = 10;
    constexpr int OPS_PER_THREAD = 100;

    for (int t = 0; t < NUM_THREADS; ++t)
    {
        threads.emplace_back(
            [&factory, t, &success_count, &failure_count]
            {
                for (int i = 0; i < OPS_PER_THREAD; ++i)
                {
                    std::string key = "widget_" + std::to_string(t) + "_" + std::to_string(i);

                    bool registered = factory.registerType(key,
                                                           [t, i]
                                                           {
                                                               return Widget(t * 1000 + i);
                                                           });

                    if (registered)
                    {
                        ++success_count;

                        auto result = factory.make(key);
                        if (result.has_value())
                        {
                            ++success_count;
                        }
                        else
                        {
                            ++failure_count;
                        }
                    }
                }
            });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    size_t expected_registrations = NUM_THREADS * OPS_PER_THREAD;
    FATP_ASSERT_EQ(factory.size(), expected_registrations, "All registrations should succeed without races");

    FATP_ASSERT_EQ(success_count.load(),
                   static_cast<int>(expected_registrations * 2),
                   "All operations should succeed (register + make)");

    FATP_ASSERT_EQ(failure_count.load(), 0, "No failures should occur");

    return true;
}

FATP_TEST_CASE(concurrent_read_write)
{
    ThreadSafeFactory<int, Widget> factory;

    // Pre-register some types
    for (int i = 0; i < 100; ++i)
    {
        (void)factory.registerType(i,
                                   [i]
                                   {
                                       return Widget(i);
                                   });
    }

    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};
    std::atomic<int> write_count{0};

    constexpr int NUM_READERS = 8;
    constexpr int NUM_WRITERS = 2;
    constexpr int WRITES_PER_WRITER = 100;

    std::vector<std::thread> readers;
    std::vector<std::thread> writers;

    // Spawn readers
    for (int i = 0; i < NUM_READERS; ++i)
    {
        readers.emplace_back(
            [&]
            {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                while (!stop.load(std::memory_order_acquire))
                {
                    int key = read_count.load(std::memory_order_relaxed) % 100;
                    auto result = factory.make(key);
                    if (result.has_value())
                    {
                        read_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
    }

    // Spawn writers
    for (int i = 0; i < NUM_WRITERS; ++i)
    {
        writers.emplace_back(
            [&, i]
            {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                for (int j = 0; j < WRITES_PER_WRITER; ++j)
                {
                    int key = 100 + i * WRITES_PER_WRITER + j;
                    (void)factory.registerType(key,
                                               [i]
                                               {
                                                   return Widget(i);
                                               });
                    write_count.fetch_add(1, std::memory_order_relaxed);
                }
            });
    }

    // Start all threads simultaneously
    start.store(true, std::memory_order_release);

    // Wait for writers to complete
    for (auto& w : writers)
    {
        w.join();
    }

    // Signal readers to stop
    stop.store(true, std::memory_order_release);
    for (auto& r : readers)
    {
        r.join();
    }

    FATP_ASSERT_EQ(write_count.load(), NUM_WRITERS * WRITES_PER_WRITER, "All writes should complete");
    FATP_ASSERT_TRUE(read_count.load() > 100, "Should perform many reads");
    FATP_ASSERT_TRUE(factory.size() >= 100 + NUM_WRITERS * WRITES_PER_WRITER, "Should have all registrations");

    return true;
}

FATP_TEST_CASE(shared_mutex_concurrency)
{
    // Use SharedMutexPolicy for better read concurrency
    using SharedFactory = Factory<int,
                                  Widget,
                                  SharedMutexPolicy,
                                  ExpectedErrorPolicy<Widget, int>,
                                  PreventOverwritePolicy,
                                  MapStoragePolicy<int, std::function<Widget()>>,
                                  InstanceLifetimePolicy,
                                  AtomicStatisticsPolicy>;

    SharedFactory factory;

    // Pre-register types
    for (int i = 0; i < 100; ++i)
    {
        (void)factory.registerType(i,
                                   [i]
                                   {
                                       return Widget(i);
                                   });
    }

    std::atomic<bool> start{false};
    std::atomic<bool> stop{false};
    std::atomic<int> read_count{0};

    constexpr int NUM_READERS = 8;

    std::vector<std::thread> readers;

    // Spawn readers - all should run concurrently with shared locks
    for (int i = 0; i < NUM_READERS; ++i)
    {
        readers.emplace_back(
            [&]
            {
                while (!start.load(std::memory_order_acquire))
                {
                    std::this_thread::yield();
                }
                while (!stop.load(std::memory_order_acquire))
                {
                    int key = read_count.load(std::memory_order_relaxed) % 100;
                    auto result = factory.make(key);
                    if (result.has_value())
                    {
                        read_count.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
    }

    start.store(true, std::memory_order_release);

    // Let readers run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    stop.store(true, std::memory_order_release);
    for (auto& r : readers)
    {
        r.join();
    }

    // With shared locks, concurrent reads should complete many operations
    FATP_ASSERT_TRUE(read_count.load() > 100, "SharedMutexPolicy should allow concurrent reads");

    return true;
}

FATP_TEST_CASE(transparent_lookup)
{
    FastFactory<std::string, Widget> factory;
    (void)factory.registerType("widget",
                               []
                               {
                                   return Widget(42);
                               });

    // Note: make() takes const K& (const std::string&), so string literals
    // undergo implicit conversion. The transparent comparator benefit is
    // in the internal find() call within the container, not at the API level.
    // A future enhancement could template make() on KeyArg for full benefit.

    // Test with string literal (const char* -> std::string conversion)
    auto lit_result = factory.make("widget");
    FATP_ASSERT_TRUE(lit_result.has_value(), "Should work with string literal");
    FATP_ASSERT_EQ(lit_result->mValue, 42, "Value should be correct");

    // Test hasType with literal
    FATP_ASSERT_TRUE(factory.hasType("widget"), "hasType should work with literal");
    FATP_ASSERT_TRUE(!factory.hasType("nonexistent"), "hasType should return false for missing");

    // Test with explicit std::string
    std::string key = "widget";
    auto str_result = factory.make(key);
    FATP_ASSERT_TRUE(str_result.has_value(), "Should work with std::string");

    return true;
}

// ============================================================================
// Batch Operations
// ============================================================================

FATP_TEST_CASE(batch_registration)
{
    SimpleFactory<std::string, Widget> factory;

    size_t registered = factory.registerTypes({{"widget1",
                                                []
                                                {
                                                    return Widget(1);
                                                }},
                                               {"widget2",
                                                []
                                                {
                                                    return Widget(2);
                                                }},
                                               {"widget3",
                                                []
                                                {
                                                    return Widget(3);
                                                }}});

    FATP_ASSERT_EQ(registered, 3u, "Should register 3 types");
    FATP_ASSERT_EQ(factory.size(), 3u, "Factory should have 3 registrations");

    auto r1 = factory.make("widget1");
    auto r2 = factory.make("widget2");
    auto r3 = factory.make("widget3");

    FATP_ASSERT_TRUE(r1.has_value() && r2.has_value() && r3.has_value(), "All should make successfully");

    FATP_ASSERT_EQ(r1->mValue, 1, "Widget1 value correct");
    FATP_ASSERT_EQ(r2->mValue, 2, "Widget2 value correct");
    FATP_ASSERT_EQ(r3->mValue, 3, "Widget3 value correct");

    return true;
}

// ============================================================================
// Advanced Features Tests
// ============================================================================

FATP_TEST_CASE(lambda_with_captures)
{
    SimpleFactory<std::string, Widget> factory;

    int captured_value = 99;
    (void)factory.registerType("captured",
                               [captured_value]
                               {
                                   return Widget(captured_value);
                               });

    auto result = factory.make("captured");
    FATP_ASSERT_TRUE(result.has_value(), "Should make captured lambda");
    FATP_ASSERT_EQ(result->mValue, 99, "Should use captured value");

    return true;
}

FATP_TEST_CASE(movable_only_types)
{
    SimpleFactory<std::string, MovableWidget> factory;

    (void)factory.registerType("movable",
                               []
                               {
                                   return MovableWidget(42);
                               });

    auto result = factory.make("movable");
    FATP_ASSERT_TRUE(result.has_value(), "Should make movable-only type");
    FATP_ASSERT_EQ(result->mValue, 42, "Value should be correct");

    MovableWidget moved = std::move(*result);
    FATP_ASSERT_EQ(moved.mValue, 42, "Moved value should be correct");
    FATP_ASSERT_EQ(result->mValue, -1, "Original should be moved-from");

    return true;
}

FATP_TEST_CASE(unique_ptr_factory)
{
    SimpleFactory<std::string, std::unique_ptr<Widget>> factory;

    (void)factory.registerType("unique_widget",
                               []
                               {
                                   return std::make_unique<Widget>(42);
                               });

    auto result = factory.make("unique_widget");
    FATP_ASSERT_TRUE(result.has_value(), "Should make unique_ptr");
    FATP_ASSERT_TRUE(*result != nullptr, "unique_ptr should not be null");
    FATP_ASSERT_EQ((*result)->mValue, 42, "Widget value should be correct");

    return true;
}

FATP_TEST_CASE(integer_keys)
{
    SimpleFactory<int, Widget> factory;

    (void)factory.registerType(1,
                               []
                               {
                                   return Widget(10);
                               });
    (void)factory.registerType(2,
                               []
                               {
                                   return Widget(20);
                               });

    auto result1 = factory.make(1);
    auto result2 = factory.make(2);

    FATP_ASSERT_TRUE(result1.has_value(), "Should make key 1");
    FATP_ASSERT_TRUE(result2.has_value(), "Should make key 2");
    FATP_ASSERT_EQ(result1->mValue, 10, "Key 1 value correct");
    FATP_ASSERT_EQ(result2->mValue, 20, "Key 2 value correct");

    return true;
}

FATP_TEST_CASE(tracked_object_lifecycle)
{
    TrackedObject::reset_counts();

    {
        SimpleFactory<std::string, TrackedObject> factory;
        (void)factory.registerType("tracked",
                                   []
                                   {
                                       return TrackedObject(1);
                                   });

        {
            auto result = factory.make("tracked");
            FATP_ASSERT_TRUE(result.has_value(), "Should make tracked object");

            int constructions = TrackedObject::construction_count.load();
            FATP_ASSERT_TRUE(constructions >= 1 && constructions <= 2,
                             "Should have 1-2 constructions (depending on copy elision)");
        }

        int destructions = TrackedObject::destruction_count.load();
        FATP_ASSERT_EQ(TrackedObject::construction_count.load(),
                       destructions,
                       "Construction and destruction counts should match");
    }

    return true;
}

// ============================================================================
// Edge Case Tests
// ============================================================================

FATP_TEST_CASE(empty_key)
{
    SimpleFactory<std::string, Widget> factory;

    // Empty string is valid key
    bool registered = factory.registerType("",
                                           []
                                           {
                                               return Widget(0);
                                           });
    FATP_ASSERT_TRUE(registered, "Empty key should be valid");

    auto result = factory.make("");
    FATP_ASSERT_TRUE(result.has_value(), "Should make with empty key");
    FATP_ASSERT_EQ(result->mValue, 0, "Value should be correct");

    return true;
}

// ============================================================================
// Benchmarks
// ============================================================================

void run_benchmarks()
{
    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Factory Benchmarks:" << colors::reset() << "\n\n";

    // Basic make
    {
        SimpleFactory<std::string, Widget> factory;
        (void)factory.registerType("widget",
                                   []
                                   {
                                       return Widget(42);
                                   });

        double time = measure_perf(
            [&factory]
            {
                auto result = factory.make("widget");
                DoNotOptimize(result);
            },
            1000000,
            10000);

        out << "Basic make: " << format_time(time) << "\n";
    }

    // Lambda capture make
    {
        SimpleFactory<std::string, ConfiguredWidget> factory;
        std::string name = "test";
        int value = 42;
        (void)factory.registerType("widget",
                                   [name, value]()
                                   {
                                       return ConfiguredWidget(name, value);
                                   });

        double time = measure_perf(
            [&factory]
            {
                auto result = factory.make("widget");
                DoNotOptimize(result);
            },
            1000000,
            10000);

        out << "Lambda capture make: " << format_time(time) << "\n";
    }

    // Registration
    {
        SimpleFactory<std::string, Widget> factory;
        int counter = 0;

        double time = measure_perf(
            [&factory, &counter]
            {
                (void)factory.registerType("widget" + std::to_string(counter++),
                                           []
                                           {
                                               return Widget(42);
                                           });
            },
            10000,
            100);

        out << "Registration: " << format_time(time) << "\n";
    }

    // Lookup (hasType)
    {
        SimpleFactory<std::string, Widget> factory;
        (void)factory.registerType("widget",
                                   []
                                   {
                                       return Widget(42);
                                   });

        double time = measure_perf(
            [&factory]
            {
                bool has = factory.hasType("widget");
                DoNotOptimize(has);
            },
            1000000,
            10000);

        out << "Lookup (hasType): " << format_time(time) << "\n";
    }

    // Direct creation vs Factory
    {
        SimpleFactory<std::string, Widget> factory;
        (void)factory.registerType("widget",
                                   []
                                   {
                                       return Widget(42);
                                   });

        out << "\nDirect creation vs Factory:\n";

        double direct_time = measure_perf(
            []
            {
                Widget w(42);
                DoNotOptimize(w);
            },
            1000000,
            10000);
        out << "  Direct creation: " << format_time(direct_time) << "\n";

        double factory_time = measure_perf(
            [&factory]
            {
                auto result = factory.make("widget");
                DoNotOptimize(result);
            },
            1000000,
            10000);
        out << "  Factory make: " << format_time(factory_time) << "\n";
        out << "  Overhead: " << std::fixed << std::setprecision(1) << (factory_time / direct_time) << "x\n";
    }

    // Map vs UnorderedMap with 1000 items
    {
        SimpleFactory<std::string, Widget> map_factory;
        FastFactory<std::string, Widget> unordered_factory;

        for (int i = 0; i < 1000; ++i)
        {
            std::string key = "widget" + std::to_string(i);
            (void)map_factory.registerType(key,
                                           [i]
                                           {
                                               return Widget(i);
                                           });
            (void)unordered_factory.registerType(key,
                                                 [i]
                                                 {
                                                     return Widget(i);
                                                 });
        }

        out << "\nMap vs UnorderedMap (1000 items):\n";

        double map_time = measure_perf(
            [&map_factory]
            {
                auto r = map_factory.make("widget500");
                DoNotOptimize(r);
            },
            100000,
            1000);
        out << "  Map storage: " << format_time(map_time) << "\n";

        double unordered_time = measure_perf(
            [&unordered_factory]
            {
                auto r = unordered_factory.make("widget500");
                DoNotOptimize(r);
            },
            100000,
            1000);
        out << "  UnorderedMap storage: " << format_time(unordered_time) << "\n";
        out << "  Speedup: " << std::fixed << std::setprecision(2) << (map_time / unordered_time) << "x\n";
    }

    // Size impact
    {
        SimpleFactory<std::string, Widget> small_factory;
        for (int i = 0; i < 10; ++i)
        {
            (void)small_factory.registerType("widget" + std::to_string(i),
                                             [i]
                                             {
                                                 return Widget(i);
                                             });
        }

        SimpleFactory<std::string, Widget> large_factory;
        for (int i = 0; i < 1000; ++i)
        {
            (void)large_factory.registerType("widget" + std::to_string(i),
                                             [i]
                                             {
                                                 return Widget(i);
                                             });
        }

        out << "\nSize impact (Map storage):\n";

        double small_time = measure_perf(
            [&small_factory]
            {
                auto r = small_factory.make("widget5");
                DoNotOptimize(r);
            },
            100000,
            1000);
        out << "  10 items: " << format_time(small_time) << "\n";

        double large_time = measure_perf(
            [&large_factory]
            {
                auto r = large_factory.make("widget500");
                DoNotOptimize(r);
            },
            100000,
            1000);
        out << "  1000 items: " << format_time(large_time) << "\n";
    }

    // Stats policy overhead comparison
    {
        out << "\nStatistics Policy Overhead:\n";

        SimpleFactory<std::string, Widget> atomic_factory;
        (void)atomic_factory.registerType("widget",
                                          []
                                          {
                                              return Widget(42);
                                          });

        HPCFactory<std::string, Widget> no_stats_factory;
        (void)no_stats_factory.registerType("widget",
                                            []
                                            {
                                                return Widget(42);
                                            });

        double atomic_time = measure_perf(
            [&atomic_factory]
            {
                auto r = atomic_factory.make("widget");
                DoNotOptimize(r);
            },
            1000000,
            10000);
        out << "  Atomic stats: " << format_time(atomic_time) << "\n";

        double no_stats_time = measure_perf(
            [&no_stats_factory]
            {
                try
                {
                    Widget w = no_stats_factory.make("widget");
                    DoNotOptimize(w);
                }
                catch (...)
                {
                }
            },
            1000000,
            10000);
        out << "  No stats (HPC): " << format_time(no_stats_time) << "\n";

        if (no_stats_time > 0)
        {
            out << "  Speedup: " << std::fixed << std::setprecision(2) << (atomic_time / no_stats_time) << "x\n";
        }
    }
}

} // namespace fat_p::testing::factory

// ============================================================================
// Main Test Runner
// ============================================================================

namespace fat_p::testing
{

bool test_Factory()
{
    FATP_PRINT_HEADER(FACTORY)

    TestRunner runner;

    auto& out = *get_test_config().output;

    // Basic functionality
    out << "\n" << colors::bold() << "=== Basic Functionality Tests ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, basic_registration);
    FATP_RUN_TEST_NS(runner, factory, basic_make);
    FATP_RUN_TEST_NS(runner, factory, lambda_capture_parameters);
    FATP_RUN_TEST_NS(runner, factory, database_connection_lambda_capture);
    FATP_RUN_TEST_NS(runner, factory, duplicate_registration);
    FATP_RUN_TEST_NS(runner, factory, unregister);
    FATP_RUN_TEST_NS(runner, factory, clear);
    FATP_RUN_TEST_NS(runner, factory, get_registered_keys);

    // Exception and error handling
    out << "\n" << colors::bold() << "=== Exception & Error Handling Tests ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, throwing_make);
    FATP_RUN_TEST_NS(runner, factory, throwing_error_policy);
    FATP_RUN_TEST_NS(runner, factory, default_error_policy);

    // Statistics
    out << "\n" << colors::bold() << "=== Statistics Tests ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, statistics);
    FATP_RUN_TEST_NS(runner, factory, const_methods_update_stats);
    FATP_RUN_TEST_NS(runner, factory, no_statistics_policy);
    FATP_RUN_TEST_NS(runner, factory, hpc_factory);

    // Policy tests
    out << "\n" << colors::bold() << "=== Policy Tests ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, overwrite_policy);
    FATP_RUN_TEST_NS(runner, factory, unordered_map_storage);
    FATP_RUN_TEST_NS(runner, factory, singleton_lifetime_policy);
    FATP_RUN_TEST_NS(runner, factory, variadic_parameters);

    // Re-entrancy test (Critical Issue #1)
    out << "\n" << colors::bold() << "=== Re-entrancy Tests ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, reentrant_factory_access);

    // SimpleVariadicFactory tests
    out << "\n" << colors::bold() << "=== SimpleVariadicFactory Tests ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, simple_variadic_factory_basic);
    FATP_RUN_TEST_NS(runner, factory, simple_variadic_factory_params);
    FATP_RUN_TEST_NS(runner, factory, simple_variadic_factory_reentrant);

    // Concurrency tests
    out << "\n" << colors::bold() << "=== Concurrency Tests ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, concurrent_access);
    FATP_RUN_TEST_NS(runner, factory, concurrent_read_write);
    FATP_RUN_TEST_NS(runner, factory, shared_mutex_concurrency);
    FATP_RUN_TEST_NS(runner, factory, transparent_lookup);

    // Batch operations
    out << "\n" << colors::bold() << "=== Batch Operations ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, batch_registration);

    // Advanced features
    out << "\n" << colors::bold() << "=== Advanced Features Tests ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, lambda_with_captures);
    FATP_RUN_TEST_NS(runner, factory, movable_only_types);
    FATP_RUN_TEST_NS(runner, factory, unique_ptr_factory);
    FATP_RUN_TEST_NS(runner, factory, integer_keys);
    FATP_RUN_TEST_NS(runner, factory, tracked_object_lifecycle);

    // Edge cases
    out << "\n" << colors::bold() << "=== Edge Case Tests ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, factory, empty_key);

    int failed = runner.print_summary();

    if (failed == 0)
    {
        factory::run_benchmarks();
    }

    return failed == 0;
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Factory() ? 0 : 1;
}
#endif

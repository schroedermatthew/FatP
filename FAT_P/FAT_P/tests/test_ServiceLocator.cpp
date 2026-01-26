/**
 * @file test_ServiceLocator.cpp
 * @brief Comprehensive test suite for fat_p::ServiceLocator
 */
/*
FATP_META:
  meta_version: 1
  component: ServiceLocator
  file_role: test
  path: tests/test_ServiceLocator.cpp
  namespace: fat_p
  summary: "Unit tests for ServiceLocator."
  related:
    docs_search: "ServiceLocator"
    headers:
      - fat_p/ServiceLocator.h
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
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "ServiceLocator.h"

namespace fat_p::testing::service_locator
{

using fat_p::DefaultServiceLocator;
using fat_p::ServiceAllowOverwritePolicy;
using fat_p::ServiceError;
using fat_p::ServiceLifetime;
using fat_p::ServiceLocator;
using fat_p::SingleThreadedPolicy;
using fat_p::ThreadSafeServiceLocator;

struct CounterService
{
    int mValue = 0;
};

struct Widget
{
    int mId = 0;

    explicit Widget(int id)
        : mId(id)
    {
    }
};

FATP_TEST_CASE(instance_registration_and_resolution)
{
    DefaultServiceLocator locator;

    CounterService svc;
    svc.mValue = 123;

    auto reg = locator.registerInstance<CounterService>(svc);
    FATP_ASSERT_TRUE(reg.has_value(), "registerInstance should succeed");

    CounterService* p = locator.tryResolve<CounterService>();
    FATP_ASSERT_TRUE(p != nullptr, "tryResolve should find registered instance");
    FATP_ASSERT_EQ(p, &svc, "tryResolve should return the same address");

    CounterService& r = locator.resolve<CounterService>();
    FATP_ASSERT_EQ(&r, &svc, "resolve should return the same address");
    FATP_ASSERT_EQ(r.mValue, 123, "resolved instance should have expected value");

    return true;
}

FATP_TEST_CASE(named_instances)
{
    DefaultServiceLocator locator;

    CounterService a;
    a.mValue = 1;

    CounterService b;
    b.mValue = 2;

    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(a, "primary").has_value(),
                     "register primary should succeed");
    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(b, "secondary").has_value(),
                     "register secondary should succeed");

    FATP_ASSERT_EQ(locator.resolve<CounterService>("primary").mValue, 1, "primary matches");
    FATP_ASSERT_EQ(locator.resolve<CounterService>("secondary").mValue, 2, "secondary matches");

    FATP_ASSERT_TRUE(locator.tryResolve<CounterService>("missing") == nullptr, "missing name should not resolve");

    return true;
}

FATP_TEST_CASE(prevent_overwrite_policy)
{
    DefaultServiceLocator locator;

    CounterService a;
    a.mValue = 10;

    CounterService b;
    b.mValue = 20;

    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(a).has_value(), "first registration should succeed");

    auto second = locator.registerInstance<CounterService>(b);
    FATP_ASSERT_TRUE(!second.has_value(), "duplicate registration should fail");

    CounterService& r = locator.resolve<CounterService>();
    FATP_ASSERT_EQ(&r, &a, "prevent overwrite keeps first instance");
    FATP_ASSERT_EQ(r.mValue, 10, "value stays from first instance");

    return true;
}

FATP_TEST_CASE(allow_overwrite_policy)
{
    ServiceLocator<SingleThreadedPolicy, ServiceAllowOverwritePolicy> locator;

    CounterService a;
    a.mValue = 10;

    CounterService b;
    b.mValue = 20;

    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(a).has_value(), "first registration should succeed");
    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(b).has_value(), "overwrite registration should succeed");

    CounterService& r = locator.resolve<CounterService>();
    FATP_ASSERT_EQ(&r, &b, "allow overwrite replaces instance");
    FATP_ASSERT_EQ(r.mValue, 20, "value from overwritten instance");

    return true;
}

FATP_TEST_CASE(shared_registration)
{
    DefaultServiceLocator locator;

    auto shared = std::make_shared<CounterService>();
    shared->mValue = 77;

    auto reg = locator.registerShared<CounterService>(shared);
    FATP_ASSERT_TRUE(reg.has_value(), "registerShared should succeed");

    CounterService& r = locator.resolve<CounterService>();
    FATP_ASSERT_EQ(r.mValue, 77, "resolved shared instance should match");

    return true;
}

FATP_TEST_CASE(named_shared_registration_and_resolve_shared_expected)
{
    DefaultServiceLocator locator;

    auto shared = std::make_shared<CounterService>();
    shared->mValue = 1234;

    FATP_ASSERT_TRUE(locator.registerShared<CounterService>(shared, "alpha").has_value(),
                     "named registerShared should succeed");

    auto result = locator.resolveSharedExpected<CounterService>("alpha");
    FATP_ASSERT_TRUE(result.has_value(), "resolveSharedExpected(name) should succeed");
    FATP_ASSERT_EQ(result.value().get(), shared.get(), "resolveSharedExpected should return same pointer");
    FATP_ASSERT_EQ(result.value()->mValue, 1234, "resolved shared instance should have expected value");

    auto unnamed = locator.resolveSharedExpected<CounterService>();
    FATP_ASSERT_FALSE(unnamed.has_value(), "unnamed resolveSharedExpected should fail when only named is registered");
    FATP_ASSERT_EQ(unnamed.error().mCode,
                   ServiceError::ServiceNotFound,
                   "unnamed resolve should report ServiceNotFound when only named is registered");

    return true;
}

FATP_TEST_CASE(shared_null_rejected)
{
    DefaultServiceLocator locator;

    std::shared_ptr<CounterService> empty;
    auto reg = locator.registerShared<CounterService>(empty);

    FATP_ASSERT_TRUE(!reg.has_value(), "registerShared should reject empty shared_ptr");
    FATP_ASSERT_EQ(reg.error().mCode,
                   ServiceError::NullSharedInstance,
                   "null shared registration should report NullSharedInstance");

    return true;
}

FATP_TEST_CASE(registration_raii_unregisters)
{
    DefaultServiceLocator locator;

    CounterService svc;
    svc.mValue = 5;

    {
        auto regExpected = DefaultServiceLocator::Registration::registerInstanceExpected(locator, svc);
        FATP_ASSERT_TRUE(regExpected.has_value(), "RAII instance registration should succeed");

        auto reg = std::move(regExpected.value());
        FATP_ASSERT_TRUE(locator.tryResolve<CounterService>() != nullptr,
                         "service should be resolvable while registration is alive");

        CounterService& r = locator.resolve<CounterService>();
        FATP_ASSERT_EQ(r.mValue, 5, "resolved value matches");
    }

    FATP_ASSERT_TRUE(locator.tryResolve<CounterService>() == nullptr,
                     "service should be unregistered when Registration is destroyed");

    return true;
}

FATP_TEST_CASE(singleton_factory_resolution)
{
    DefaultServiceLocator locator;

    std::atomic<int> created{0};

    auto reg = locator.registerFactory<Widget>(
        [&created]() -> std::unique_ptr<Widget> {
            created.fetch_add(1, std::memory_order_relaxed);
            return std::make_unique<Widget>(42);
        },
        ServiceLifetime::Singleton);

    FATP_ASSERT_TRUE(reg.has_value(), "singleton factory registration should succeed");

    Widget* p1 = locator.tryResolve<Widget>();
    FATP_ASSERT_TRUE(p1 != nullptr, "tryResolve should materialize singleton");
    Widget* p2 = locator.tryResolve<Widget>();
    FATP_ASSERT_TRUE(p2 != nullptr, "tryResolve should return singleton again");

    FATP_ASSERT_EQ(p1, p2, "singleton should be cached");
    FATP_ASSERT_EQ(p1->mId, 42, "singleton instance should have expected value");
    FATP_ASSERT_EQ(created.load(std::memory_order_relaxed), 1, "singleton created once");

    return true;
}

FATP_TEST_CASE(named_singleton_factory_caches)
{
    DefaultServiceLocator locator;

    std::atomic<int> created{0};

    auto reg = locator.registerFactory<Widget>(
        [&created]() -> std::unique_ptr<Widget> {
            created.fetch_add(1, std::memory_order_relaxed);
            return std::make_unique<Widget>(7);
        },
        ServiceLifetime::Singleton,
        "primary");

    FATP_ASSERT_TRUE(reg.has_value(), "named singleton factory registration should succeed");

    Widget* p1 = locator.tryResolve<Widget>("primary");
    Widget* p2 = locator.tryResolve<Widget>("primary");

    FATP_ASSERT_TRUE(p1 != nullptr, "tryResolve(name) should materialize singleton");
    FATP_ASSERT_TRUE(p2 != nullptr, "tryResolve(name) should return singleton again");
    FATP_ASSERT_EQ(p1, p2, "named singleton should be cached");
    FATP_ASSERT_EQ(p1->mId, 7, "named singleton instance should have expected value");
    FATP_ASSERT_EQ(created.load(std::memory_order_relaxed), 1, "named singleton factory should run once");

    return true;
}

FATP_TEST_CASE(named_singleton_factory_error_propagation)
{
    DefaultServiceLocator locator;

    std::atomic<int> invocations{0};

    auto reg = locator.registerFactory<Widget>(
        [&invocations]() -> std::unique_ptr<Widget> {
            invocations.fetch_add(1, std::memory_order_relaxed);
            throw std::runtime_error("boom");
        },
        ServiceLifetime::Singleton,
        "bad");

    FATP_ASSERT_TRUE(reg.has_value(), "named throwing singleton factory registration should succeed");

    auto result1 = locator.resolveExpected<Widget>("bad");
    FATP_ASSERT_FALSE(result1.has_value(), "resolveExpected(name) should fail when factory throws");
    FATP_ASSERT_EQ(result1.error().mCode, ServiceError::FactoryThrew, "should propagate FactoryThrew error code");
    FATP_ASSERT_EQ(result1.error().mName, "bad", "error info should include the failing service name");

    auto result2 = locator.resolveExpected<Widget>("bad");
    FATP_ASSERT_FALSE(result2.has_value(), "subsequent resolveExpected(name) should also fail");
    FATP_ASSERT_EQ(invocations.load(std::memory_order_relaxed),
                   2,
                   "singleton factory should be retried after failure (no caching of failures)");

    return true;
}

FATP_TEST_CASE(transient_factory_creation)
{
    DefaultServiceLocator locator;

    std::atomic<int> created{0};

    auto reg = locator.registerFactory<Widget>(
        [&created]() -> std::unique_ptr<Widget> {
            int id = created.fetch_add(1, std::memory_order_relaxed) + 1;
            return std::make_unique<Widget>(id);
        },
        ServiceLifetime::Transient);

    FATP_ASSERT_TRUE(reg.has_value(), "transient factory registration should succeed");

    FATP_ASSERT_TRUE(locator.tryResolve<Widget>() == nullptr, "transient factory is not resolvable via tryResolve");

    auto c1 = locator.createExpected<Widget>();
    FATP_ASSERT_TRUE(c1.has_value(), "createExpected should succeed");
    auto c2 = locator.createExpected<Widget>();
    FATP_ASSERT_TRUE(c2.has_value(), "createExpected should succeed");

    FATP_ASSERT_TRUE(c1.value() != nullptr, "created instance 1 not null");
    FATP_ASSERT_TRUE(c2.value() != nullptr, "created instance 2 not null");
    FATP_ASSERT_TRUE(c1.value().get() != c2.value().get(), "transient creates new instance");

    FATP_ASSERT_EQ(created.load(std::memory_order_relaxed), 2, "factory invoked twice");

    return true;
}

FATP_TEST_CASE(named_transient_factory_creation)
{
    DefaultServiceLocator locator;

    std::atomic<int> created{0};

    auto reg = locator.registerFactory<Widget>(
        [&created]() -> std::unique_ptr<Widget> {
            int id = created.fetch_add(1, std::memory_order_relaxed) + 1;
            return std::make_unique<Widget>(id);
        },
        ServiceLifetime::Transient,
        "transient");

    FATP_ASSERT_TRUE(reg.has_value(), "named transient factory registration should succeed");
    FATP_ASSERT_TRUE(locator.tryResolve<Widget>("transient") == nullptr,
                     "named transient factory is not resolvable via tryResolve");

    auto c1 = locator.createExpected<Widget>("transient");
    FATP_ASSERT_TRUE(c1.has_value(), "createExpected(name) should succeed");
    auto c2 = locator.createExpected<Widget>("transient");
    FATP_ASSERT_TRUE(c2.has_value(), "createExpected(name) should succeed");

    FATP_ASSERT_TRUE(c1.value() != nullptr, "created instance 1 not null");
    FATP_ASSERT_TRUE(c2.value() != nullptr, "created instance 2 not null");
    FATP_ASSERT_TRUE(c1.value().get() != c2.value().get(), "named transient creates new instance each time");
    FATP_ASSERT_EQ(created.load(std::memory_order_relaxed), 2, "named transient factory invoked twice");

    return true;
}

FATP_TEST_CASE(scoped_overrides)
{
    DefaultServiceLocator parent;

    CounterService base;
    base.mValue = 1;

    FATP_ASSERT_TRUE(parent.registerInstance<CounterService>(base).has_value(), "parent registration should succeed");

    auto scope = parent.makeScope();

    CounterService override;
    override.mValue = 2;

    FATP_ASSERT_TRUE(scope.locator().registerInstance<CounterService>(override).has_value(),
                     "child override registration should succeed");

    FATP_ASSERT_EQ(parent.resolve<CounterService>().mValue, 1, "parent resolves base");
    FATP_ASSERT_EQ(scope.locator().resolve<CounterService>().mValue, 2, "child resolves override");

    return true;
}

FATP_TEST_CASE(named_scoped_overrides)
{
    DefaultServiceLocator parent;

    CounterService base;
    base.mValue = 1;

    FATP_ASSERT_TRUE(parent.registerInstance<CounterService>(base, "primary").has_value(),
                     "parent named registration should succeed");

    auto scope = parent.makeScope();

    CounterService* inherited = scope.locator().tryResolve<CounterService>("primary");
    FATP_ASSERT_TRUE(inherited != nullptr, "child should resolve named service registered in parent");
    FATP_ASSERT_EQ(inherited, &base, "child should resolve to the parent's instance");

    CounterService override;
    override.mValue = 2;

    FATP_ASSERT_TRUE(scope.locator().registerInstance<CounterService>(override, "primary").has_value(),
                     "child named override registration should succeed");

    FATP_ASSERT_EQ(parent.resolve<CounterService>("primary").mValue, 1, "parent resolves base");
    FATP_ASSERT_EQ(scope.locator().resolve<CounterService>("primary").mValue, 2, "child resolves override");

    return true;
}

FATP_TEST_CASE(thread_safe_smoke)
{
    ThreadSafeServiceLocator locator;

    CounterService svc;
    svc.mValue = 99;

    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(svc).has_value(), "thread-safe register should succeed");

    constexpr int kThreads = 8;
    constexpr int kIters = 1000;

    std::atomic<int> ok{0};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&]() {
            for (int i = 0; i < kIters; ++i)
            {
                CounterService* p = locator.tryResolve<CounterService>();
                if (p != nullptr && p->mValue == 99)
                {
                    ok.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& th : threads)
    {
        th.join();
    }

    FATP_ASSERT_EQ(ok.load(std::memory_order_relaxed),
                   kThreads * kIters,
                   "all thread resolves should observe the registered service");

    return true;
}

FATP_TEST_CASE(concurrent_singleton_exactly_once)
{
    ThreadSafeServiceLocator locator;

    std::atomic<int> factoryInvocations{0};
    std::atomic<Widget*> firstObserved{nullptr};
    std::atomic<int> mismatchCount{0};

    auto reg = locator.registerFactory<Widget>(
        [&factoryInvocations]() -> std::unique_ptr<Widget> {
            factoryInvocations.fetch_add(1, std::memory_order_relaxed);
            // Sleep to widen the race window
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return std::make_unique<Widget>(42);
        },
        ServiceLifetime::Singleton);
    FATP_ASSERT_TRUE(reg.has_value(), "singleton factory registration should succeed");

    constexpr int kThreads = 16;
    std::vector<std::thread> threads;
    threads.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t)
    {
        threads.emplace_back([&]() {
            Widget* p = locator.tryResolve<Widget>();
            if (p == nullptr)
            {
                mismatchCount.fetch_add(1, std::memory_order_relaxed);
                return;
            }

            // Record first observed instance, check all threads see the same one
            Widget* expected = nullptr;
            if (!firstObserved.compare_exchange_strong(expected,
                                                       p,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_acquire))
            {
                // Someone else set it first; verify we got the same pointer
                if (expected != p)
                {
                    mismatchCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& th : threads)
    {
        th.join();
    }

    FATP_ASSERT_EQ(factoryInvocations.load(std::memory_order_relaxed),
                   1,
                   "singleton factory must be invoked exactly once");
    FATP_ASSERT_EQ(mismatchCount.load(std::memory_order_relaxed),
                   0,
                   "all threads must observe the same singleton instance");
    FATP_ASSERT_TRUE(firstObserved.load(std::memory_order_relaxed) != nullptr, "singleton should have been created");
    FATP_ASSERT_EQ(firstObserved.load(std::memory_order_relaxed)->mId, 42, "singleton should have correct value");

    return true;
}

FATP_TEST_CASE(cross_thread_singleton_factories_are_serialized)
{
    ThreadSafeServiceLocator locator;

    std::mutex enteredMutex;
    std::condition_variable enteredCv;
    std::atomic<int> entered{0};

    std::atomic<int> activeFactories{0};
    std::atomic<int> maxActiveFactories{0};

    std::atomic<bool> start{false};
    std::atomic<bool> allowExit{false};

    auto updateMax = [&maxActiveFactories](int value) {
        int current = maxActiveFactories.load(std::memory_order_relaxed);
        while (value > current &&
               !maxActiveFactories.compare_exchange_weak(current,
                                                        value,
                                                        std::memory_order_relaxed,
                                                        std::memory_order_relaxed))
        {
        }
    };

    auto waitUntilReleased = [&]() {
        entered.fetch_add(1, std::memory_order_release);
        enteredCv.notify_all();

        while (!allowExit.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    };

    auto regA = locator.registerFactory<Widget>(
        [&]() -> std::unique_ptr<Widget> {
            int now = activeFactories.fetch_add(1, std::memory_order_acq_rel) + 1;
            updateMax(now);

            waitUntilReleased();

            activeFactories.fetch_sub(1, std::memory_order_acq_rel);
            return std::make_unique<Widget>(1);
        },
        ServiceLifetime::Singleton);
    FATP_ASSERT_TRUE(regA.has_value(), "Widget singleton factory registration should succeed");

    auto regB = locator.registerFactory<CounterService>(
        [&]() -> std::unique_ptr<CounterService> {
            int now = activeFactories.fetch_add(1, std::memory_order_acq_rel) + 1;
            updateMax(now);

            waitUntilReleased();

            activeFactories.fetch_sub(1, std::memory_order_acq_rel);
            auto created = std::make_unique<CounterService>();
            created->mValue = 2;
            return created;
        },
        ServiceLifetime::Singleton);
    FATP_ASSERT_TRUE(regB.has_value(), "CounterService singleton factory registration should succeed");

    std::atomic<Widget*> widgetPtr{nullptr};
    std::atomic<CounterService*> counterPtr{nullptr};

    std::thread t1([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        widgetPtr.store(locator.tryResolve<Widget>(), std::memory_order_relaxed);
    });

    std::thread t2([&]() {
        while (!start.load(std::memory_order_acquire))
        {
            std::this_thread::yield();
        }
        counterPtr.store(locator.tryResolve<CounterService>(), std::memory_order_relaxed);
    });

    start.store(true, std::memory_order_release);

    {
        std::unique_lock<std::mutex> lock(enteredMutex);
        bool ok = enteredCv.wait_for(lock,
                                     std::chrono::seconds(1),
                                     [&]() { return entered.load(std::memory_order_acquire) >= 1; });
        FATP_ASSERT_TRUE(ok, "at least one singleton factory should start");
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    FATP_ASSERT_EQ(maxActiveFactories.load(std::memory_order_relaxed),
                   1,
                   "singleton factories must be serialized (cross-thread deadlock prevention gate)");

    allowExit.store(true, std::memory_order_release);

    t1.join();
    t2.join();

    FATP_ASSERT_EQ(entered.load(std::memory_order_relaxed), 2, "both singleton factories should have executed");
    FATP_ASSERT_TRUE(widgetPtr.load(std::memory_order_relaxed) != nullptr, "Widget singleton should resolve");
    FATP_ASSERT_TRUE(counterPtr.load(std::memory_order_relaxed) != nullptr,
                     "CounterService singleton should resolve");
    FATP_ASSERT_EQ(maxActiveFactories.load(std::memory_order_relaxed),
                   1,
                   "singleton factories must not execute concurrently");

    return true;
}

FATP_TEST_CASE(singleton_reregister_does_not_poison)
{
    ThreadSafeServiceLocator locator;

    std::atomic<int> oldInvocations{0};
    std::atomic<int> newInvocations{0};
    std::atomic<bool> oldEntered{false};
    std::atomic<bool> allowOldExit{false};
    std::atomic<Widget*> oldObserved{nullptr};

    auto regOld = locator.registerFactory<Widget>(
        [&]() -> std::unique_ptr<Widget> {
            oldInvocations.fetch_add(1, std::memory_order_relaxed);
            oldEntered.store(true, std::memory_order_release);

            while (!allowOldExit.load(std::memory_order_acquire))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            return std::make_unique<Widget>(1);
        },
        ServiceLifetime::Singleton);
    FATP_ASSERT_TRUE(regOld.has_value(), "old singleton factory registration should succeed");

    std::thread creator([&]() {
        oldObserved.store(locator.tryResolve<Widget>(), std::memory_order_relaxed);
    });

    while (!oldEntered.load(std::memory_order_acquire))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    FATP_ASSERT_TRUE(locator.unregister<Widget>(), "unregister should succeed while factory is running");

    auto regNew = locator.registerFactory<Widget>(
        [&]() -> std::unique_ptr<Widget> {
            newInvocations.fetch_add(1, std::memory_order_relaxed);
            return std::make_unique<Widget>(2);
        },
        ServiceLifetime::Singleton);
    FATP_ASSERT_TRUE(regNew.has_value(), "new singleton factory registration should succeed");

    allowOldExit.store(true, std::memory_order_release);
    creator.join();

    FATP_ASSERT_TRUE(oldObserved.load(std::memory_order_relaxed) == nullptr,
                     "creator resolve must fail if the singleton registration was replaced during creation");

    Widget* resolved = locator.tryResolve<Widget>();
    FATP_ASSERT_TRUE(resolved != nullptr, "resolve after re-register should succeed");
    FATP_ASSERT_EQ(resolved->mId, 2, "resolve must return the re-registered singleton instance");

    FATP_ASSERT_EQ(oldInvocations.load(std::memory_order_relaxed), 1, "old factory should have been invoked once");
    FATP_ASSERT_EQ(newInvocations.load(std::memory_order_relaxed), 1, "new factory should have been invoked once");

    return true;
}

FATP_TEST_CASE(circular_dependency_detected)
{
    DefaultServiceLocator locator;

    ServiceError innerErrorCode{};

    // Register a factory that tries to resolve itself - circular dependency
    auto reg = locator.registerFactory<Widget>(
        [&locator, &innerErrorCode]() -> std::unique_ptr<Widget> {
            // This should detect CircularDependency error
            auto inner = locator.resolveExpected<Widget>();
            if (!inner.has_value())
            {
                innerErrorCode = inner.error().mCode;
                // Propagate the error by returning nullptr
                return nullptr;
            }
            return std::make_unique<Widget>(42);
        },
        ServiceLifetime::Singleton);
    FATP_ASSERT_TRUE(reg.has_value(), "factory registration should succeed");

    // Attempting to resolve should detect the circular dependency
    auto result = locator.resolveExpected<Widget>();

    // The inner resolve should have detected circular dependency
    FATP_ASSERT_EQ(innerErrorCode,
                   ServiceError::CircularDependency,
                   "inner resolve should report CircularDependency error");

    // The outer resolve should fail because factory returned nullptr
    FATP_ASSERT_FALSE(result.has_value(), "circular dependency should fail resolution");

    return true;
}

FATP_TEST_CASE(is_registered_query)
{
    DefaultServiceLocator locator;

    CounterService svc;
    svc.mValue = 42;

    FATP_ASSERT_FALSE(locator.isRegistered<CounterService>(), "should not be registered initially");

    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(svc).has_value(), "registration should succeed");

    FATP_ASSERT_TRUE(locator.isRegistered<CounterService>(), "should be registered after registerInstance");
    FATP_ASSERT_FALSE(locator.isRegistered<CounterService>("other"), "named service should not be registered");

    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(svc, "other").has_value(),
                     "named registration should succeed");
    FATP_ASSERT_TRUE(locator.isRegistered<CounterService>("other"), "named service should now be registered");

    return true;
}

FATP_TEST_CASE(is_registered_checks_parent)
{
    DefaultServiceLocator parent;

    CounterService svc;
    svc.mValue = 100;

    FATP_ASSERT_TRUE(parent.registerInstance<CounterService>(svc).has_value(), "parent registration should succeed");

    auto scope = parent.makeScope();

    FATP_ASSERT_TRUE(scope.locator().isRegistered<CounterService>(),
                     "child should find service registered in parent");
    FATP_ASSERT_FALSE(scope.locator().isRegistered<Widget>(), "child should not find unregistered type");

    return true;
}

FATP_TEST_CASE(resolve_shared_expected_from_shared)
{
    DefaultServiceLocator locator;

    auto shared = std::make_shared<CounterService>();
    shared->mValue = 55;

    FATP_ASSERT_TRUE(locator.registerShared<CounterService>(shared).has_value(), "registerShared should succeed");

    auto result = locator.resolveSharedExpected<CounterService>();
    FATP_ASSERT_TRUE(result.has_value(), "resolveSharedExpected should succeed for shared registration");
    FATP_ASSERT_EQ(result.value().get(), shared.get(), "should return same pointer");
    FATP_ASSERT_EQ(result.value()->mValue, 55, "should have correct value");

    return true;
}

FATP_TEST_CASE(resolve_shared_expected_from_singleton_factory)
{
    DefaultServiceLocator locator;

    auto reg = locator.registerFactory<Widget>(
        []() -> std::unique_ptr<Widget> {
            return std::make_unique<Widget>(123);
        },
        ServiceLifetime::Singleton);

    FATP_ASSERT_TRUE(reg.has_value(), "singleton factory registration should succeed");

    auto result1 = locator.resolveSharedExpected<Widget>();
    FATP_ASSERT_TRUE(result1.has_value(), "resolveSharedExpected should succeed for singleton factory");
    FATP_ASSERT_EQ(result1.value()->mId, 123, "should have correct value");

    auto result2 = locator.resolveSharedExpected<Widget>();
    FATP_ASSERT_TRUE(result2.has_value(), "second resolveSharedExpected should succeed");
    FATP_ASSERT_EQ(result1.value().get(), result2.value().get(), "should return same singleton instance");

    return true;
}

FATP_TEST_CASE(resolve_shared_expected_fails_for_instance)
{
    DefaultServiceLocator locator;

    CounterService svc;
    svc.mValue = 10;

    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(svc).has_value(), "registerInstance should succeed");

    auto result = locator.resolveSharedExpected<CounterService>();
    FATP_ASSERT_FALSE(result.has_value(), "resolveSharedExpected should fail for instance registration");

    return true;
}

FATP_TEST_CASE(resolve_shared_expected_fails_for_transient)
{
    DefaultServiceLocator locator;

    auto reg = locator.registerFactory<Widget>(
        []() -> std::unique_ptr<Widget> {
            return std::make_unique<Widget>(1);
        },
        ServiceLifetime::Transient);

    FATP_ASSERT_TRUE(reg.has_value(), "transient factory registration should succeed");

    auto result = locator.resolveSharedExpected<Widget>();
    FATP_ASSERT_FALSE(result.has_value(), "resolveSharedExpected should fail for transient factory");

    return true;
}

FATP_TEST_CASE(transient_resolve_returns_correct_error_code)
{
    DefaultServiceLocator locator;

    auto reg = locator.registerFactory<Widget>(
        []() -> std::unique_ptr<Widget> {
            return std::make_unique<Widget>(1);
        },
        ServiceLifetime::Transient);

    FATP_ASSERT_TRUE(reg.has_value(), "transient factory registration should succeed");

    auto result = locator.resolveExpected<Widget>();
    FATP_ASSERT_FALSE(result.has_value(), "resolveExpected should fail for transient");
    FATP_ASSERT_EQ(result.error().mCode,
                   ServiceError::TransientRequiresCreate,
                   "should return TransientRequiresCreate error code");

    return true;
}

FATP_TEST_CASE(register_factory_invalid_lifetime_increments_stats)
{
    using LocatorWithStats =
        ServiceLocator<SingleThreadedPolicy, ServicePreventOverwritePolicy, AtomicServiceLocatorStatisticsPolicy>;

    LocatorWithStats locator;

    auto snap1 = locator.stats().snapshot();
    FATP_ASSERT_EQ(snap1.mRegistrationFailures, 0u, "initially no registration failures");

    // Cast an invalid lifetime value
    auto badLifetime = static_cast<ServiceLifetime>(99);
    auto reg = locator.registerFactory<Widget>(
        []() -> std::unique_ptr<Widget> {
            return std::make_unique<Widget>(1);
        },
        badLifetime);

    FATP_ASSERT_FALSE(reg.has_value(), "registration with invalid lifetime should fail");
    FATP_ASSERT_EQ(reg.error().mCode, ServiceError::InvalidLifetime, "should return InvalidLifetime error");

    auto snap2 = locator.stats().snapshot();
    FATP_ASSERT_EQ(snap2.mRegistrationFailures, 1u, "should increment registration failures for invalid lifetime");

    return true;
}

FATP_TEST_CASE(clear_increments_unregistration_stats)
{
    using LocatorWithStats =
        ServiceLocator<SingleThreadedPolicy, ServicePreventOverwritePolicy, AtomicServiceLocatorStatisticsPolicy>;

    LocatorWithStats locator;

    CounterService svc1, svc2, svc3;
    svc1.mValue = 1;
    svc2.mValue = 2;
    svc3.mValue = 3;

    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(svc1, "a").has_value(), "register a should succeed");
    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(svc2, "b").has_value(), "register b should succeed");
    FATP_ASSERT_TRUE(locator.registerInstance<CounterService>(svc3, "c").has_value(), "register c should succeed");

    auto snap1 = locator.stats().snapshot();
    FATP_ASSERT_EQ(snap1.mRegistrations, 3u, "should have 3 registrations");
    FATP_ASSERT_EQ(snap1.mUnregistrations, 0u, "initially no unregistrations");

    locator.clear();

    FATP_ASSERT_EQ(locator.size(), 0u, "locator should be empty after clear");

    auto snap2 = locator.stats().snapshot();
    FATP_ASSERT_EQ(snap2.mUnregistrations, 3u, "clear should increment unregistrations by count of cleared entries");

    return true;
}

FATP_TEST_CASE(registration_raii_register_shared_expected)
{
    DefaultServiceLocator locator;

    auto shared = std::make_shared<CounterService>();
    shared->mValue = 77;

    {
        auto regExpected = DefaultServiceLocator::Registration::registerSharedExpected(locator, shared);
        FATP_ASSERT_TRUE(regExpected.has_value(), "RAII shared registration should succeed");

        auto reg = std::move(regExpected.value());
        FATP_ASSERT_TRUE(locator.tryResolve<CounterService>() != nullptr,
                         "service should be resolvable while registration is alive");
        FATP_ASSERT_EQ(locator.resolve<CounterService>().mValue, 77, "resolved value matches");
    }

    FATP_ASSERT_TRUE(locator.tryResolve<CounterService>() == nullptr,
                     "service should be unregistered when Registration is destroyed");

    return true;
}

FATP_TEST_CASE(registration_raii_register_factory_expected)
{
    DefaultServiceLocator locator;

    std::atomic<int> created{0};

    {
        auto regExpected = DefaultServiceLocator::Registration::registerFactoryExpected<Widget>(
            locator,
            [&created]() -> std::unique_ptr<Widget> {
                created.fetch_add(1, std::memory_order_relaxed);
                return std::make_unique<Widget>(42);
            },
            ServiceLifetime::Singleton);

        FATP_ASSERT_TRUE(regExpected.has_value(), "RAII factory registration should succeed");

        auto reg = std::move(regExpected.value());

        Widget* p = locator.tryResolve<Widget>();
        FATP_ASSERT_TRUE(p != nullptr, "singleton should be resolvable");
        FATP_ASSERT_EQ(p->mId, 42, "singleton value matches");
        FATP_ASSERT_EQ(created.load(std::memory_order_relaxed), 1, "factory invoked once");
    }

    FATP_ASSERT_TRUE(locator.tryResolve<Widget>() == nullptr,
                     "factory should be unregistered when Registration is destroyed");

    return true;
}

// ============================================================================
// Bug Fix Regression Tests
// ============================================================================

/// Service for testing cache invalidation with destructor tracking
struct CacheTestService
{
    int mValue = 0;
    static inline std::atomic<int> sDestructorCount{0};

    CacheTestService() = default;
    explicit CacheTestService(int v) : mValue(v) {}
    ~CacheTestService() { ++sDestructorCount; }
};

/// P0: Cache invalidation on RAII unregister (UAF prevention)
FATP_TEST_CASE(cache_invalidation_on_raii_unregister)
{
    using fat_p::HotLoopServiceLocator;

    CacheTestService::sDestructorCount = 0;
    HotLoopServiceLocator locator;

    {
        auto regResult = HotLoopServiceLocator::Registration::registerSharedExpected<CacheTestService>(
            locator, std::make_shared<CacheTestService>(999));
        FATP_ASSERT_TRUE(regResult.has_value(), "Registration should succeed");

        // First resolve - populates cache
        CacheTestService* first = locator.tryResolve<CacheTestService>();
        FATP_ASSERT_NOT_NULLPTR(first, "First resolve should find service");
        FATP_ASSERT_EQ(first->mValue, 999, "Service value should match");
    }

    // shared_ptr released, destructor called
    FATP_ASSERT_EQ(CacheTestService::sDestructorCount.load(), 1,
                   "Destructor should be called after RAII unregister");

    FATP_ASSERT_EQ(locator.size(), static_cast<size_t>(0),
                   "Locator should be empty after RAII unregister");

    // Cache must be invalidated - should return nullptr, not stale pointer
    CacheTestService* second = locator.tryResolve<CacheTestService>();
    FATP_ASSERT_NULLPTR(second, "After RAII unregister, cache must be invalidated");

    return true;
}

/// P0: const T resolve with cache-enabled locators
FATP_TEST_CASE(const_resolve_with_cache)
{
    using fat_p::HotLoopServiceLocator;

    HotLoopServiceLocator locator;
    CounterService svc;
    svc.mValue = 42;

    auto reg = locator.registerInstance<CounterService>(svc);
    FATP_ASSERT_TRUE(reg.has_value(), "Registration should succeed");

    // This would not compile before the fix (static_cast<void*>(const T*) is ill-formed)
    const CounterService* resolved = locator.tryResolve<const CounterService>();
    FATP_ASSERT_NOT_NULLPTR(resolved, "const resolve should find service");
    FATP_ASSERT_EQ(resolved->mValue, 42, "Resolved value should match");

    // Cache hit for const T
    const CounterService* cached = locator.tryResolve<const CounterService>();
    FATP_ASSERT_EQ(resolved, cached, "Cache should return same pointer");

    // Named const resolve (no cache for named lookups)
    CounterService namedSvc;
    namedSvc.mValue = 7;

    auto namedReg = locator.registerInstance<CounterService>(namedSvc, "alpha");
    FATP_ASSERT_TRUE(namedReg.has_value(), "Named registration should succeed");

    const CounterService* namedResolved = locator.tryResolve<const CounterService>("alpha");
    FATP_ASSERT_NOT_NULLPTR(namedResolved, "Named const resolve should find service");
    FATP_ASSERT_EQ(namedResolved->mValue, 7, "Named const resolve should match");

    return true;
}

/// P2: Registration operator bool()
FATP_TEST_CASE(registration_operator_bool)
{
    DefaultServiceLocator locator;
    CounterService svc;

    // Default-constructed Registration should be false
    DefaultServiceLocator::Registration emptyReg;
    FATP_ASSERT_FALSE(static_cast<bool>(emptyReg), "Default Registration should be false");

    auto regResult = DefaultServiceLocator::Registration::registerInstanceExpected<CounterService>(
        locator, svc);
    FATP_ASSERT_TRUE(regResult.has_value(), "Registration should succeed");

    auto activeReg = std::move(regResult.value());
    FATP_ASSERT_TRUE(static_cast<bool>(activeReg), "Active Registration should be true");

    activeReg.reset();
    FATP_ASSERT_FALSE(static_cast<bool>(activeReg), "After reset, Registration should be false");

    return true;
}

/// P2: ServiceErrorInfo operator<<
FATP_TEST_CASE(service_error_info_stream_output)
{
    fat_p::ServiceErrorInfo err{
        fat_p::ServiceError::ServiceNotFound,
        "test message",
        "my_service"
    };

    std::ostringstream oss;
    oss << err;
    std::string output = oss.str();

    FATP_ASSERT_TRUE(output.find("Service not found") != std::string::npos,
                     "Output should contain error code");
    FATP_ASSERT_TRUE(output.find("test message") != std::string::npos,
                     "Output should contain message");
    FATP_ASSERT_TRUE(output.find("my_service") != std::string::npos,
                     "Output should contain name");

    return true;
}

} // namespace fat_p::testing::service_locator

namespace fat_p::testing
{

bool test_ServiceLocator()
{
    FATP_PRINT_HEADER(SERVICE LOCATOR)

    TestRunner runner;

    auto& out = *get_test_config().output;

    out << "\n" << colors::bold() << "=== Basic Registration & Resolution ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, instance_registration_and_resolution);
    FATP_RUN_TEST_NS(runner, service_locator, named_instances);

    out << "\n" << colors::bold() << "=== Registration Policies ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, prevent_overwrite_policy);
    FATP_RUN_TEST_NS(runner, service_locator, allow_overwrite_policy);

    out << "\n" << colors::bold() << "=== Ownership & Factories ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, shared_registration);
    FATP_RUN_TEST_NS(runner, service_locator, shared_null_rejected);
    FATP_RUN_TEST_NS(runner, service_locator, named_shared_registration_and_resolve_shared_expected);
    FATP_RUN_TEST_NS(runner, service_locator, singleton_factory_resolution);
    FATP_RUN_TEST_NS(runner, service_locator, named_singleton_factory_caches);
    FATP_RUN_TEST_NS(runner, service_locator, named_singleton_factory_error_propagation);
    FATP_RUN_TEST_NS(runner, service_locator, transient_factory_creation);
    FATP_RUN_TEST_NS(runner, service_locator, named_transient_factory_creation);

    out << "\n" << colors::bold() << "=== Scoping & Thread Safety ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, scoped_overrides);
    FATP_RUN_TEST_NS(runner, service_locator, named_scoped_overrides);
    FATP_RUN_TEST_NS(runner, service_locator, registration_raii_unregisters);
    FATP_RUN_TEST_NS(runner, service_locator, thread_safe_smoke);
    FATP_RUN_TEST_NS(runner, service_locator, concurrent_singleton_exactly_once);
    FATP_RUN_TEST_NS(runner, service_locator, cross_thread_singleton_factories_are_serialized);
    FATP_RUN_TEST_NS(runner, service_locator, singleton_reregister_does_not_poison);
    FATP_RUN_TEST_NS(runner, service_locator, circular_dependency_detected);

    out << "\n" << colors::bold() << "=== New API: isRegistered ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, is_registered_query);
    FATP_RUN_TEST_NS(runner, service_locator, is_registered_checks_parent);

    out << "\n" << colors::bold() << "=== New API: resolveSharedExpected ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, resolve_shared_expected_from_shared);
    FATP_RUN_TEST_NS(runner, service_locator, resolve_shared_expected_from_singleton_factory);
    FATP_RUN_TEST_NS(runner, service_locator, resolve_shared_expected_fails_for_instance);
    FATP_RUN_TEST_NS(runner, service_locator, resolve_shared_expected_fails_for_transient);

    out << "\n" << colors::bold() << "=== Error Codes & Statistics ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, transient_resolve_returns_correct_error_code);
    FATP_RUN_TEST_NS(runner, service_locator, register_factory_invalid_lifetime_increments_stats);
    FATP_RUN_TEST_NS(runner, service_locator, clear_increments_unregistration_stats);

    out << "\n" << colors::bold() << "=== RAII Registration Helpers ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, registration_raii_register_shared_expected);
    FATP_RUN_TEST_NS(runner, service_locator, registration_raii_register_factory_expected);

    out << "\n" << colors::bold() << "=== Bug Fix Regressions ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, cache_invalidation_on_raii_unregister);
    FATP_RUN_TEST_NS(runner, service_locator, const_resolve_with_cache);
    FATP_RUN_TEST_NS(runner, service_locator, registration_operator_bool);
    FATP_RUN_TEST_NS(runner, service_locator, service_error_info_stream_output);

    int failed = runner.print_summary();
    return failed == 0;
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_ServiceLocator() ? 0 : 1;
}
#endif

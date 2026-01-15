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
#include <memory>
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
        [&created]() -> std::unique_ptr<Widget>
        {
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

FATP_TEST_CASE(transient_factory_creation)
{
    DefaultServiceLocator locator;

    std::atomic<int> created{0};

    auto reg = locator.registerFactory<Widget>(
        [&created]() -> std::unique_ptr<Widget>
        {
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
        threads.emplace_back(
            [&]()
            {
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
        [&factoryInvocations]() -> std::unique_ptr<Widget>
        {
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
        threads.emplace_back(
            [&]()
            {
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

FATP_TEST_CASE(singleton_reregister_does_not_poison)
{
    ThreadSafeServiceLocator locator;

    std::atomic<int> oldInvocations{0};
    std::atomic<int> newInvocations{0};
    std::atomic<bool> oldEntered{false};
    std::atomic<bool> allowOldExit{false};
    std::atomic<Widget*> oldObserved{nullptr};

    auto regOld = locator.registerFactory<Widget>(
        [&]() -> std::unique_ptr<Widget>
        {
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

    std::thread creator(
        [&]()
        {
            oldObserved.store(locator.tryResolve<Widget>(), std::memory_order_relaxed);
        });

    while (!oldEntered.load(std::memory_order_acquire))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    FATP_ASSERT_TRUE(locator.unregister<Widget>(), "unregister should succeed while factory is running");

    auto regNew = locator.registerFactory<Widget>(
        [&]() -> std::unique_ptr<Widget>
        {
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
        [&locator, &innerErrorCode]() -> std::unique_ptr<Widget>
        {
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
    FATP_RUN_TEST_NS(runner, service_locator, singleton_factory_resolution);
    FATP_RUN_TEST_NS(runner, service_locator, transient_factory_creation);

    out << "\n" << colors::bold() << "=== Scoping & Thread Safety ===" << colors::reset() << std::endl;
    FATP_RUN_TEST_NS(runner, service_locator, scoped_overrides);
    FATP_RUN_TEST_NS(runner, service_locator, registration_raii_unregisters);
    FATP_RUN_TEST_NS(runner, service_locator, thread_safe_smoke);
    FATP_RUN_TEST_NS(runner, service_locator, concurrent_singleton_exactly_once);
    FATP_RUN_TEST_NS(runner, service_locator, singleton_reregister_does_not_poison);
    FATP_RUN_TEST_NS(runner, service_locator, circular_dependency_detected);

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

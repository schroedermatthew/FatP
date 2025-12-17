#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>

#include "Signal.h"
#include "FatPTest.h"

#ifndef ENABLE_TEST_APPLICATION
#include "test_Signal.h"
#endif

namespace fat_p::testing
{

namespace
{

bool test_basic_connection()
{
    Signal<void(int)> sig;
    int receivedValue = 0;

    auto conn = sig.connect([&](int v) { receivedValue = v; });

    sig.emit(42);
    SIMPLE_ASSERT(receivedValue == 42, "Should receive emitted value");

    return true;
}

bool test_multiple_connections()
{
    Signal<void(int)> sig;
    std::vector<int> values;

    auto c1 = sig.connect([&](int v) { values.push_back(v); });
    auto c2 = sig.connect([&](int v) { values.push_back(v * 2); });
    auto c3 = sig.connect([&](int v) { values.push_back(v * 3); });

    sig.emit(10);

    SIMPLE_ASSERT(values.size() == 3, "Should have 3 values");
    SIMPLE_ASSERT(values[0] == 10, "First slot receives 10");
    SIMPLE_ASSERT(values[1] == 20, "Second slot receives 20");
    SIMPLE_ASSERT(values[2] == 30, "Third slot receives 30");

    return true;
}

bool test_manual_connect_disconnect()
{
    Signal<void(int)> sig;
    int count = 0;

    ConnectionId id = sig.connectManual([&](int) { ++count; });

    sig.emit(1);
    SIMPLE_ASSERT(count == 1, "Should be called once");

    bool disconnected = sig.disconnect(id);
    SIMPLE_ASSERT(disconnected, "Should return true on disconnect");

    sig.emit(1);
    SIMPLE_ASSERT(count == 1, "Should not be called after disconnect");

    return true;
}

bool test_call_operator()
{
    Signal<void(int, int)> sig;
    int sum = 0;

    auto conn = sig.connect([&](int a, int b) { sum = a + b; });

    sig(3, 4);
    SIMPLE_ASSERT(sum == 7, "operator() should emit");

    return true;
}

bool test_scoped_connection_raii()
{
    Signal<void()> sig;
    int callCount = 0;

    {
        auto conn = sig.connect([&]() { ++callCount; });
        sig.emit();
        SIMPLE_ASSERT(callCount == 1, "Should be called once");
    }

    sig.emit();
    SIMPLE_ASSERT(callCount == 1, "Should not be called after scope exit");

    return true;
}

bool test_scoped_connection_move()
{
    Signal<void()> sig;
    int callCount = 0;

    ScopedConnection conn1;
    {
        auto conn2 = sig.connect([&]() { ++callCount; });
        conn1 = std::move(conn2);
        SIMPLE_ASSERT(!conn2.isConnected(), "Moved-from should be disconnected");
        SIMPLE_ASSERT(conn1.isConnected(), "Moved-to should be connected");
    }

    sig.emit();
    SIMPLE_ASSERT(callCount == 1, "Should still be connected via conn1");

    conn1.disconnect();
    sig.emit();
    SIMPLE_ASSERT(callCount == 1, "Should be disconnected now");

    return true;
}

bool test_scoped_connection_release()
{
    Signal<void()> sig;
    int callCount = 0;

    {
        auto conn = sig.connect([&]() { ++callCount; });
        conn.release();
        SIMPLE_ASSERT(!conn.isConnected(), "Should be released");
    }

    sig.emit();
    SIMPLE_ASSERT(callCount == 1, "Connection should still be active after release");

    return true;
}

bool test_priority_ordering()
{
    Signal<void(std::vector<int>&)> sig;

    auto c1 = sig.connect([](std::vector<int>& v) { v.push_back(1); }, 10);
    auto c2 = sig.connect([](std::vector<int>& v) { v.push_back(2); }, -5);
    auto c3 = sig.connect([](std::vector<int>& v) { v.push_back(3); }, 5);
    auto c4 = sig.connect([](std::vector<int>& v) { v.push_back(4); }, 0);

    std::vector<int> order;
    sig.emit(order);

    SIMPLE_ASSERT(order.size() == 4, "Should have 4 elements");
    SIMPLE_ASSERT(order[0] == 1, "Priority 10 first");
    SIMPLE_ASSERT(order[1] == 3, "Priority 5 second");
    SIMPLE_ASSERT(order[2] == 4, "Priority 0 third");
    SIMPLE_ASSERT(order[3] == 2, "Priority -5 last");

    return true;
}

bool test_disconnect_during_emission()
{
    Signal<void()> sig;
    int callCount = 0;
    ConnectionId selfId;

    selfId = sig.connectManual([&]() {
        ++callCount;
        sig.disconnect(selfId);
    });

    sig.emit();
    SIMPLE_ASSERT(callCount == 1, "Should be called once");

    sig.emit();
    SIMPLE_ASSERT(callCount == 1, "Should not be called after self-disconnect");

    return true;
}

bool test_connect_during_emission()
{
    Signal<void()> sig;
    int originalCount = 0;
    int newCount = 0;
    ScopedConnection newConn;

    auto conn = sig.connect([&]() {
        ++originalCount;
        if (originalCount == 1)
        {
            newConn = sig.connect([&]() { ++newCount; });
        }
    });

    sig.emit();
    SIMPLE_ASSERT(originalCount == 1, "Original called once");

    sig.emit();
    SIMPLE_ASSERT(originalCount == 2, "Original called twice");
    SIMPLE_ASSERT(newCount == 1, "New slot called on second emission");

    return true;
}

bool test_nested_emission()
{
    Signal<void(int)> sig;
    std::vector<int> calls;

    auto conn = sig.connect([&](int depth) {
        calls.push_back(depth);
        if (depth > 0)
        {
            sig.emit(depth - 1);
        }
    });

    sig.emit(3);

    SIMPLE_ASSERT(calls.size() == 4, "Should have 4 nested calls");
    SIMPLE_ASSERT(calls[0] == 3, "First call depth 3");
    SIMPLE_ASSERT(calls[1] == 2, "Second call depth 2");
    SIMPLE_ASSERT(calls[2] == 1, "Third call depth 1");
    SIMPLE_ASSERT(calls[3] == 0, "Fourth call depth 0");

    return true;
}

bool test_slot_count()
{
    Signal<void()> sig;

    SIMPLE_ASSERT(sig.slotCount() == 0, "Should start empty");
    SIMPLE_ASSERT(!sig.hasConnections(), "Should have no connections");

    auto c1 = sig.connect([]() {});
    SIMPLE_ASSERT(sig.slotCount() == 1, "Should have 1 slot");
    SIMPLE_ASSERT(sig.hasConnections(), "Should have connections");

    auto c2 = sig.connect([]() {});
    auto c3 = sig.connect([]() {});
    SIMPLE_ASSERT(sig.slotCount() == 3, "Should have 3 slots");

    c2.disconnect();
    SIMPLE_ASSERT(sig.activeSlotCount() == 2, "Should have 2 active slots");

    return true;
}

bool test_is_connected()
{
    Signal<void()> sig;

    ConnectionId id = sig.connectManual([]() {});
    SIMPLE_ASSERT(sig.isConnected(id), "Should be connected");

    sig.disconnect(id);
    SIMPLE_ASSERT(!sig.isConnected(id), "Should not be connected after disconnect");

    SIMPLE_ASSERT(!sig.isConnected(InvalidConnectionId), "Invalid ID should not be connected");

    return true;
}

bool test_disconnect_all()
{
    Signal<void()> sig;
    int count = 0;

    auto c1 = sig.connect([&]() { ++count; });
    auto c2 = sig.connect([&]() { ++count; });
    auto c3 = sig.connect([&]() { ++count; });

    sig.emit();
    SIMPLE_ASSERT(count == 3, "Should call all 3 slots");

    sig.disconnectAll();
    sig.emit();
    SIMPLE_ASSERT(count == 3, "Should not call after disconnectAll");

    return true;
}

class EventHandler
{
public:
    int lastValue = 0;
    int callCount = 0;

    void onValue(int v)
    {
        lastValue = v;
        ++callCount;
    }
};

bool test_member_function_connection()
{
    Signal<void(int)> sig;
    EventHandler handler;

    auto conn = sig.connect(&handler, &EventHandler::onValue);

    sig.emit(123);
    SIMPLE_ASSERT(handler.lastValue == 123, "Should receive 123");
    SIMPLE_ASSERT(handler.callCount == 1, "Should be called once");

    sig.emit(456);
    SIMPLE_ASSERT(handler.lastValue == 456, "Should receive 456");
    SIMPLE_ASSERT(handler.callCount == 2, "Should be called twice");

    return true;
}

bool test_catch_and_ignore_policy()
{
    Signal<void(), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;
    int count = 0;

    auto c1 = sig.connect([&]() { ++count; });
    auto c2 = sig.connect([]() { throw std::runtime_error("Oops!"); });
    auto c3 = sig.connect([&]() { ++count; });

    sig.emit();

    SIMPLE_ASSERT(count == 2, "Both non-throwing slots should be called");

    return true;
}

bool test_propagate_exception_policy()
{
    Signal<void(), SingleThreadedPolicy, PropagateExceptionPolicy> sig;
    int count = 0;

    auto c1 = sig.connect([&]() { ++count; });
    auto c2 = sig.connect([]() { throw std::runtime_error("Expected"); });
    auto c3 = sig.connect([&]() { ++count; });

    bool caught = false;
    try
    {
        sig.emit();
    }
    catch (const std::runtime_error& e)
    {
        caught = true;
        SIMPLE_ASSERT(std::string(e.what()).find("Expected") != std::string::npos,
                      "Should catch expected exception");
    }

    SIMPLE_ASSERT(caught, "Exception should propagate");
    SIMPLE_ASSERT(count == 1, "Only first slot should be called");

    return true;
}

bool test_emit_collect()
{
    Signal<int(int), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;

    auto c1 = sig.connect([](int x) { return x * 1; });
    auto c2 = sig.connect([](int x) { return x * 2; });
    auto c3 = sig.connect([](int x) { return x * 3; });

    auto results = sig.emitCollect(10);

    SIMPLE_ASSERT(results.size() == 3, "Should have 3 results");
    SIMPLE_ASSERT(results[0] == 10, "First result is 10");
    SIMPLE_ASSERT(results[1] == 20, "Second result is 20");
    SIMPLE_ASSERT(results[2] == 30, "Third result is 30");

    return true;
}

bool test_emit_collect_with_exceptions()
{
    Signal<int(int), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;

    auto c1 = sig.connect([](int x) { return x * 1; });
    auto c2 = sig.connect([](int) -> int { throw std::runtime_error("Skip"); });
    auto c3 = sig.connect([](int x) { return x * 3; });

    auto results = sig.emitCollect(10);

    SIMPLE_ASSERT(results.size() == 2, "Should have 2 results (one skipped)");
    SIMPLE_ASSERT(results[0] == 10, "First result is 10");
    SIMPLE_ASSERT(results[1] == 30, "Second result is 30 (third slot)");

    return true;
}

bool test_emit_collect_propagate_exception()
{
    Signal<int(int), SingleThreadedPolicy, PropagateExceptionPolicy> sig;
    int callCount = 0;

    auto c1 = sig.connect([&](int x) { ++callCount; return x * 1; });
    auto c2 = sig.connect([&](int) -> int { ++callCount; throw std::runtime_error("Stop"); });
    auto c3 = sig.connect([&](int x) { ++callCount; return x * 3; });

    bool caught = false;
    try
    {
        auto results = sig.emitCollect(10);
        SIMPLE_ASSERT(false, "Should have thrown exception");
        (void)results;
    }
    catch (const std::runtime_error&)
    {
        caught = true;
    }

    SIMPLE_ASSERT(caught, "Exception should propagate from emitCollect");
    SIMPLE_ASSERT(callCount == 2, "Only slots before and including throw should be called");

    return true;
}

bool test_emit_until()
{
    Signal<bool(int), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;
    int callCount = 0;

    auto c1 = sig.connect([&](int x) { ++callCount; return x > 10; });
    auto c2 = sig.connect([&](int x) { ++callCount; return x > 5; });
    auto c3 = sig.connect([&](int) { ++callCount; return true; });

    bool result = sig.emitUntil(7);

    SIMPLE_ASSERT(result, "Should return true (second slot)");
    SIMPLE_ASSERT(callCount == 2, "Third slot should not be called");

    return true;
}

bool test_emit_until_with_exceptions()
{
    Signal<bool(int), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;
    int callCount = 0;

    auto c1 = sig.connect([&](int) -> bool { ++callCount; throw std::runtime_error("Skip"); });
    auto c2 = sig.connect([&](int) { ++callCount; return true; });

    bool result = sig.emitUntil(5);

    SIMPLE_ASSERT(result, "Should return true from second slot");
    SIMPLE_ASSERT(callCount == 2, "Both slots attempted");

    return true;
}

bool test_thread_safe_emission()
{
    ThreadSafeSignal<void(int)> sig;
    std::atomic<int> total{0};

    auto conn = sig.connect([&](int v) {
        total.fetch_add(v, std::memory_order_relaxed);
    });

    std::vector<std::thread> threads;
    constexpr int numThreads = 4;
    constexpr int emitsPerThread = 1000;

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < emitsPerThread; ++j)
            {
                sig.emit(1);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    SIMPLE_ASSERT(total.load() == numThreads * emitsPerThread,
                  "All emissions should be counted");

    return true;
}

bool test_concurrent_connect_disconnect()
{
    ThreadSafeSignal<void()> sig;
    std::atomic<bool> running{true};
    std::atomic<int> emitCount{0};

    std::thread emitter([&]() {
        while (running.load())
        {
            sig.emit();
            emitCount.fetch_add(1);
        }
    });

    std::thread modifier([&]() {
        for (int i = 0; i < 100; ++i)
        {
            auto conn = sig.connect([]() {});
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    modifier.join();
    running.store(false);
    emitter.join();

    SIMPLE_ASSERT(emitCount.load() > 0, "Should have emitted");

    return true;
}

bool test_thread_safe_disconnect_during_emission()
{
    ThreadSafeSignal<void()> sig;
    std::atomic<int> callCount{0};
    ConnectionId selfId;

    selfId = sig.connectManual([&]() {
        callCount.fetch_add(1);
        sig.disconnect(selfId);
    });

    sig.emit();
    SIMPLE_ASSERT(callCount.load() == 1, "Should be called once");

    sig.emit();
    SIMPLE_ASSERT(callCount.load() == 1, "Should not be called after disconnect");

    return true;
}

bool test_inline_storage_efficiency()
{
    Signal<void()> sig;

    auto c1 = sig.connect([]() {});
    auto c2 = sig.connect([]() {});
    auto c3 = sig.connect([]() {});
    auto c4 = sig.connect([]() {});

    SIMPLE_ASSERT(sig.slotCount() == 4, "Should have 4 slots");

    auto c5 = sig.connect([]() {});
    SIMPLE_ASSERT(sig.slotCount() == 5, "Should have 5 slots");

    int count = 0;
    auto verifier = sig.connect([&]() { ++count; });
    sig.emit();
    SIMPLE_ASSERT(count == 1, "Verifier should be called");

    return true;
}

bool test_custom_inline_capacity_small()
{
    Signal<void(), SingleThreadedPolicy, CatchAndIgnorePolicy, 1> sig;
    int count = 0;

    auto c1 = sig.connect([&]() { ++count; });
    SIMPLE_ASSERT(sig.slotCount() == 1, "Should have 1 slot");

    auto c2 = sig.connect([&]() { ++count; });
    auto c3 = sig.connect([&]() { ++count; });
    SIMPLE_ASSERT(sig.slotCount() == 3, "Should have 3 slots after overflow");

    sig.emit();
    SIMPLE_ASSERT(count == 3, "All 3 slots called");

    return true;
}

bool test_custom_inline_capacity_large()
{
    Signal<void(), SingleThreadedPolicy, CatchAndIgnorePolicy, 8> sig;
    int count = 0;

    std::vector<ScopedConnection> conns;
    for (int i = 0; i < 8; ++i)
    {
        conns.push_back(sig.connect([&]() { ++count; }));
    }
    SIMPLE_ASSERT(sig.slotCount() == 8, "Should have 8 slots");

    sig.emit();
    SIMPLE_ASSERT(count == 8, "All 8 slots called");

    return true;
}

bool test_spinlock_signal()
{
    SpinlockSignal<void(int)> sig;
    std::atomic<int> total{0};

    auto conn = sig.connect([&](int v) {
        total.fetch_add(v, std::memory_order_relaxed);
    });

    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
    {
        threads.emplace_back([&]() {
            for (int j = 0; j < 100; ++j)
            {
                sig.emit(1);
            }
        });
    }

    for (auto& t : threads)
    {
        t.join();
    }

    SIMPLE_ASSERT(total.load() == 400, "All emissions counted");

    return true;
}

bool test_local_signal_alias()
{
    LocalSignal<void(int)> sig;
    int value = 0;

    auto conn = sig.connect([&](int v) { value = v; });
    sig.emit(42);

    SIMPLE_ASSERT(value == 42, "LocalSignal should work");

    return true;
}

bool test_empty_signal_emission()
{
    Signal<void()> sig;
    sig.emit();
    SIMPLE_ASSERT(sig.slotCount() == 0, "Should still be empty");

    return true;
}

bool test_double_disconnect()
{
    Signal<void()> sig;

    ConnectionId id = sig.connectManual([]() {});

    SIMPLE_ASSERT(sig.disconnect(id), "First disconnect should succeed");
    SIMPLE_ASSERT(!sig.disconnect(id), "Second disconnect should fail");

    return true;
}

bool test_double_disconnect_during_emission()
{
    Signal<void()> sig;
    ConnectionId targetId;
    int disconnectCount = 0;

    targetId = sig.connectManual([]() {});

    auto c1 = sig.connect([&]() {
        if (sig.disconnect(targetId))
        {
            ++disconnectCount;
        }
    });

    auto c2 = sig.connect([&]() {
        if (sig.disconnect(targetId))
        {
            ++disconnectCount;
        }
    });

    sig.emit();
    SIMPLE_ASSERT(disconnectCount == 1, "Only one disconnect should succeed");

    return true;
}

bool test_invalid_connection_id()
{
    Signal<void()> sig;

    SIMPLE_ASSERT(!sig.disconnect(InvalidConnectionId), "Should return false");
    SIMPLE_ASSERT(!sig.isConnected(InvalidConnectionId), "Should not be connected");

    return true;
}

bool test_move_signal()
{
    Signal<void()> sig1;
    int count = 0;

    auto conn = sig1.connect([&]() { ++count; });
    sig1.emit();
    SIMPLE_ASSERT(count == 1, "Should be called once");

    Signal<void()> sig2 = std::move(sig1);
    sig2.emit();
    SIMPLE_ASSERT(count == 2, "Should be called after move");

    return true;
}

bool test_move_assignment()
{
    Signal<void()> sig1;
    Signal<void()> sig2;
    int count1 = 0;
    int count2 = 0;

    auto conn1 = sig1.connect([&]() { ++count1; });
    auto conn2 = sig2.connect([&]() { ++count2; });

    sig1.emit();
    sig2.emit();
    SIMPLE_ASSERT(count1 == 1, "sig1 called");
    SIMPLE_ASSERT(count2 == 1, "sig2 called");

    sig2 = std::move(sig1);
    sig2.emit();
    SIMPLE_ASSERT(count1 == 2, "sig1's slot called via sig2");

    return true;
}

bool test_is_signal_trait()
{
    SIMPLE_ASSERT(is_signal_v<Signal<void()>>, "Signal<void()> is a signal");
    SIMPLE_ASSERT(is_signal_v<Signal<int(float, double)>>, "Signal<int(float,double)> is a signal");
    SIMPLE_ASSERT(is_signal_v<ThreadSafeSignal<void()>>, "ThreadSafeSignal is a signal");
    SIMPLE_ASSERT(is_signal_v<SpinlockSignal<void()>>, "SpinlockSignal is a signal");
    SIMPLE_ASSERT(is_signal_v<LocalSignal<void()>>, "LocalSignal is a signal");

    SIMPLE_ASSERT(!is_signal_v<int>, "int is not a signal");
    SIMPLE_ASSERT(!is_signal_v<std::function<void()>>, "std::function is not a signal");

    return true;
}

bool test_many_slots_performance()
{
    Signal<void()> sig;
    int count = 0;

    std::vector<ScopedConnection> connections;
    connections.reserve(100);

    for (int i = 0; i < 100; ++i)
    {
        connections.push_back(sig.connect([&]() { ++count; }));
    }

    SIMPLE_ASSERT(sig.slotCount() == 100, "Should have 100 slots");

    sig.emit();
    SIMPLE_ASSERT(count == 100, "All 100 slots called");

    return true;
}

void benchmark_signal()
{
    std::cout << "\n" << colors::cyan() << "Signal Benchmarks:" << colors::reset() << "\n\n";

    Signal<void(int)> sig;
    int dummy = 0;

    auto conn = sig.connect([&](int v) { dummy += v; });

    double emit_time = measure_perf([&sig]() {
        sig.emit(1);
    }, 100000, 1000);
    std::cout << "Emit (1 slot): " << format_time(emit_time) << "\n";

    auto c2 = sig.connect([&](int v) { dummy += v; });
    auto c3 = sig.connect([&](int v) { dummy += v; });
    auto c4 = sig.connect([&](int v) { dummy += v; });

    double emit4_time = measure_perf([&sig]() {
        sig.emit(1);
    }, 100000, 1000);
    std::cout << "Emit (4 slots): " << format_time(emit4_time) << "\n";

    Signal<void()> connSig;
    double connect_time = measure_perf([&connSig]() {
        auto c = connSig.connect([]() {});
        DoNotOptimize(c);
    }, 10000, 100);
    std::cout << "Connect: " << format_time(connect_time) << "\n";

    Signal<void()> disconnSig;
    std::vector<ConnectionId> ids;
    for (int i = 0; i < 1000; ++i)
    {
        ids.push_back(disconnSig.connectManual([]() {}));
    }
    size_t idx = 0;
    double disconnect_time = measure_perf([&disconnSig, &ids, &idx]() {
        if (idx < ids.size())
        {
            disconnSig.disconnect(ids[idx++]);
        }
    }, 1000, 10);
    std::cout << "Disconnect: " << format_time(disconnect_time) << "\n";

    ThreadSafeSignal<void(int)> tsSig;
    int tsDummy = 0;
    auto tsConn = tsSig.connect([&](int v) { tsDummy += v; });

    double ts_emit_time = measure_perf([&tsSig]() {
        tsSig.emit(1);
    }, 100000, 1000);
    std::cout << "Emit (ThreadSafe, 1 slot): " << format_time(ts_emit_time) << "\n";

    DoNotOptimize(dummy);
    DoNotOptimize(tsDummy);
}

} // anonymous namespace

bool test_Signal()
{
    PRINT_HEADER(SIGNAL)

    TestRunner runner;

    RUN_TEST(runner, basic_connection);
    RUN_TEST(runner, multiple_connections);
    RUN_TEST(runner, manual_connect_disconnect);
    RUN_TEST(runner, call_operator);
    RUN_TEST(runner, scoped_connection_raii);
    RUN_TEST(runner, scoped_connection_move);
    RUN_TEST(runner, scoped_connection_release);
    RUN_TEST(runner, priority_ordering);
    RUN_TEST(runner, disconnect_during_emission);
    RUN_TEST(runner, connect_during_emission);
    RUN_TEST(runner, nested_emission);
    RUN_TEST(runner, slot_count);
    RUN_TEST(runner, is_connected);
    RUN_TEST(runner, disconnect_all);
    RUN_TEST(runner, member_function_connection);
    RUN_TEST(runner, catch_and_ignore_policy);
    RUN_TEST(runner, propagate_exception_policy);
    RUN_TEST(runner, emit_collect);
    RUN_TEST(runner, emit_collect_with_exceptions);
    RUN_TEST(runner, emit_collect_propagate_exception);
    RUN_TEST(runner, emit_until);
    RUN_TEST(runner, emit_until_with_exceptions);
    RUN_TEST(runner, thread_safe_emission);
    RUN_TEST(runner, concurrent_connect_disconnect);
    RUN_TEST(runner, thread_safe_disconnect_during_emission);
    RUN_TEST(runner, inline_storage_efficiency);
    RUN_TEST(runner, custom_inline_capacity_small);
    RUN_TEST(runner, custom_inline_capacity_large);
    RUN_TEST(runner, spinlock_signal);
    RUN_TEST(runner, local_signal_alias);
    RUN_TEST(runner, empty_signal_emission);
    RUN_TEST(runner, double_disconnect);
    RUN_TEST(runner, double_disconnect_during_emission);
    RUN_TEST(runner, invalid_connection_id);
    RUN_TEST(runner, move_signal);
    RUN_TEST(runner, move_assignment);
    RUN_TEST(runner, is_signal_trait);
    RUN_TEST(runner, many_slots_performance);

    benchmark_signal();

    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Signal() ? 0 : 1;
}
#endif

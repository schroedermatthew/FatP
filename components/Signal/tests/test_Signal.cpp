/**
 * @file test_Signal.cpp
 * @brief Comprehensive unit tests for Signal.h
 */
/*
FATP_META:
  meta_version: 1
  component: Signal
  file_role: test
  path: components/Signal/tests/test_Signal.cpp
  layer: Testing
  namespace: fat_p
  summary: "Unit tests for Signal."
  api_stability: in_work
  related:
    docs_search: "Signal"
    headers:
      - include/fat_p/Signal.h
      - include/fat_p/FatPTest.h
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
#include <iostream>
#include <thread>
#include <vector>

#include "FatPTest.h"
#include "Signal.h"

namespace fat_p::testing::signal
{

FATP_TEST_CASE(basic_connection)
{
    Signal<void(int)> sig;
    int receivedValue = 0;

    auto conn = sig.connect([&](int v) {
        receivedValue = v;
    });

    sig.emit(42);
    FATP_ASSERT_EQ(receivedValue, 42, "Should receive emitted value");

    return true;
}

FATP_TEST_CASE(multiple_connections)
{
    Signal<void(int)> sig;
    std::vector<int> values;

    auto c1 = sig.connect([&](int v) {
        values.push_back(v);
    });
    auto c2 = sig.connect([&](int v) {
        values.push_back(v * 2);
    });
    auto c3 = sig.connect([&](int v) {
        values.push_back(v * 3);
    });

    sig.emit(10);

    FATP_ASSERT_EQ(values.size(), 3, "Should have 3 values");
    FATP_ASSERT_EQ(values[0], 10, "First slot receives 10");
    FATP_ASSERT_EQ(values[1], 20, "Second slot receives 20");
    FATP_ASSERT_EQ(values[2], 30, "Third slot receives 30");

    return true;
}

FATP_TEST_CASE(manual_connect_disconnect)
{
    Signal<void(int)> sig;
    int count = 0;

    ConnectionId id = sig.connectManual([&](int) {
        ++count;
    });

    sig.emit(1);
    FATP_ASSERT_EQ(count, 1, "Should be called once");

    bool disconnected = sig.disconnect(id);
    FATP_ASSERT_TRUE(disconnected, "Should return true on disconnect");

    sig.emit(1);
    FATP_ASSERT_EQ(count, 1, "Should not be called after disconnect");

    return true;
}

FATP_TEST_CASE(call_operator)
{
    Signal<void(int, int)> sig;
    int sum = 0;

    auto conn = sig.connect([&](int a, int b) {
        sum = a + b;
    });

    sig(3, 4);
    FATP_ASSERT_EQ(sum, 7, "operator() should emit");

    return true;
}

FATP_TEST_CASE(scoped_connection_raii)
{
    Signal<void()> sig;
    int callCount = 0;

    {
        auto conn = sig.connect([&]() {
            ++callCount;
        });
        sig.emit();
        FATP_ASSERT_EQ(callCount, 1, "Should be called once");
    }

    sig.emit();
    FATP_ASSERT_EQ(callCount, 1, "Should not be called after scope exit");

    return true;
}

FATP_TEST_CASE(scoped_connection_move)
{
    Signal<void()> sig;
    int callCount = 0;

    ScopedConnection conn1;
    {
        auto conn2 = sig.connect([&]() {
            ++callCount;
        });
        conn1 = std::move(conn2);
        FATP_ASSERT_FALSE(conn2.isConnected(), "Moved-from should be disconnected");
        FATP_ASSERT_TRUE(conn1.isConnected(), "Moved-to should be connected");
    }

    sig.emit();
    FATP_ASSERT_EQ(callCount, 1, "Should still be connected via conn1");

    conn1.disconnect();
    sig.emit();
    FATP_ASSERT_EQ(callCount, 1, "Should be disconnected now");

    return true;
}

FATP_TEST_CASE(scoped_connection_release)
{
    Signal<void()> sig;
    int callCount = 0;

    {
        auto conn = sig.connect([&]() {
            ++callCount;
        });
        conn.release();
        FATP_ASSERT_FALSE(conn.isConnected(), "Should be released");
    }

    sig.emit();
    FATP_ASSERT_EQ(callCount, 1, "Connection should still be active after release");

    return true;
}

FATP_TEST_CASE(priority_ordering)
{
    Signal<void(std::vector<int>&)> sig;

    auto c1 = sig.connect(
        [](std::vector<int>& v) {
            v.push_back(1);
        },
        10);
    auto c2 = sig.connect(
        [](std::vector<int>& v) {
            v.push_back(2);
        },
        -5);
    auto c3 = sig.connect(
        [](std::vector<int>& v) {
            v.push_back(3);
        },
        5);
    auto c4 = sig.connect(
        [](std::vector<int>& v) {
            v.push_back(4);
        },
        0);

    std::vector<int> order;
    sig.emit(order);

    FATP_ASSERT_EQ(order.size(), 4, "Should have 4 elements");
    FATP_ASSERT_EQ(order[0], 1, "Priority 10 first");
    FATP_ASSERT_EQ(order[1], 3, "Priority 5 second");
    FATP_ASSERT_EQ(order[2], 4, "Priority 0 third");
    FATP_ASSERT_EQ(order[3], 2, "Priority -5 last");

    return true;
}

FATP_TEST_CASE(disconnect_during_emission)
{
    Signal<void()> sig;
    int callCount = 0;
    ConnectionId selfId;

    selfId = sig.connectManual([&]() {
        ++callCount;
        sig.disconnect(selfId);
    });

    sig.emit();
    FATP_ASSERT_EQ(callCount, 1, "Should be called once");

    sig.emit();
    FATP_ASSERT_EQ(callCount, 1, "Should not be called after self-disconnect");

    return true;
}

FATP_TEST_CASE(connect_during_emission)
{
    Signal<void()> sig;
    int originalCount = 0;
    int newCount = 0;
    ScopedConnection newConn;

    auto conn = sig.connect([&]() {
        ++originalCount;
        if (originalCount == 1)
        {
            newConn = sig.connect([&]() {
                ++newCount;
            });
        }
    });

    sig.emit();
    FATP_ASSERT_EQ(originalCount, 1, "Original called once");

    sig.emit();
    FATP_ASSERT_EQ(originalCount, 2, "Original called twice");
    FATP_ASSERT_EQ(newCount, 1, "New slot called on second emission");

    return true;
}

FATP_TEST_CASE(nested_emission)
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

    FATP_ASSERT_EQ(calls.size(), 4, "Should have 4 nested calls");
    FATP_ASSERT_EQ(calls[0], 3, "First call depth 3");
    FATP_ASSERT_EQ(calls[1], 2, "Second call depth 2");
    FATP_ASSERT_EQ(calls[2], 1, "Third call depth 1");
    FATP_ASSERT_EQ(calls[3], 0, "Fourth call depth 0");

    return true;
}

FATP_TEST_CASE(slot_count)
{
    Signal<void()> sig;

    FATP_ASSERT_EQ(sig.slotCount(), 0, "Should start empty");
    FATP_ASSERT_FALSE(sig.hasConnections(), "Should have no connections");

    auto c1 = sig.connect([]() {});
    FATP_ASSERT_EQ(sig.slotCount(), 1, "Should have 1 slot");
    FATP_ASSERT_TRUE(sig.hasConnections(), "Should have connections");

    auto c2 = sig.connect([]() {});
    auto c3 = sig.connect([]() {});
    FATP_ASSERT_EQ(sig.slotCount(), 3, "Should have 3 slots");

    c2.disconnect();
    FATP_ASSERT_EQ(sig.activeSlotCount(), 2, "Should have 2 active slots");

    return true;
}

FATP_TEST_CASE(is_connected)
{
    Signal<void()> sig;

    ConnectionId id = sig.connectManual([]() {});
    FATP_ASSERT_TRUE(sig.isConnected(id), "Should be connected");

    sig.disconnect(id);
    FATP_ASSERT_FALSE(sig.isConnected(id), "Should not be connected after disconnect");

    FATP_ASSERT_FALSE(sig.isConnected(InvalidConnectionId), "Invalid ID should not be connected");

    return true;
}

FATP_TEST_CASE(disconnect_all)
{
    Signal<void()> sig;
    int count = 0;

    auto c1 = sig.connect([&]() {
        ++count;
    });
    auto c2 = sig.connect([&]() {
        ++count;
    });
    auto c3 = sig.connect([&]() {
        ++count;
    });

    sig.emit();
    FATP_ASSERT_EQ(count, 3, "Should call all 3 slots");

    sig.disconnectAll();
    sig.emit();
    FATP_ASSERT_EQ(count, 3, "Should not call after disconnectAll");

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

FATP_TEST_CASE(member_function_connection)
{
    Signal<void(int)> sig;
    EventHandler handler;

    auto conn = sig.connect(&handler, &EventHandler::onValue);

    sig.emit(123);
    FATP_ASSERT_EQ(handler.lastValue, 123, "Should receive 123");
    FATP_ASSERT_EQ(handler.callCount, 1, "Should be called once");

    sig.emit(456);
    FATP_ASSERT_EQ(handler.lastValue, 456, "Should receive 456");
    FATP_ASSERT_EQ(handler.callCount, 2, "Should be called twice");

    return true;
}

FATP_TEST_CASE(catch_and_ignore_policy)
{
    Signal<void(), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;
    int count = 0;

    auto c1 = sig.connect([&]() {
        ++count;
    });
    auto c2 = sig.connect([]() {
        throw std::runtime_error("Oops!");
    });
    auto c3 = sig.connect([&]() {
        ++count;
    });

    sig.emit();

    FATP_ASSERT_EQ(count, 2, "Both non-throwing slots should be called");

    return true;
}

FATP_TEST_CASE(propagate_exception_policy)
{
    Signal<void(), SingleThreadedPolicy, PropagateExceptionPolicy> sig;
    int count = 0;

    auto c1 = sig.connect([&]() {
        ++count;
    });
    auto c2 = sig.connect([]() {
        throw std::runtime_error("Expected");
    });
    auto c3 = sig.connect([&]() {
        ++count;
    });

    bool caught = false;
    try
    {
        sig.emit();
    }
    catch (const std::runtime_error& e)
    {
        caught = true;
        FATP_ASSERT_NE(std::string(e.what()).find("Expected"), std::string::npos, "Should catch expected exception");
    }

    FATP_ASSERT_TRUE(caught, "Exception should propagate");
    FATP_ASSERT_EQ(count, 1, "Only first slot should be called");

    return true;
}

FATP_TEST_CASE(emit_collect)
{
    Signal<int(int), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;

    auto c1 = sig.connect([](int x) {
        return x * 1;
    });
    auto c2 = sig.connect([](int x) {
        return x * 2;
    });
    auto c3 = sig.connect([](int x) {
        return x * 3;
    });

    auto results = sig.emitCollect(10);

    FATP_ASSERT_EQ(results.size(), 3, "Should have 3 results");
    FATP_ASSERT_EQ(results[0], 10, "First result is 10");
    FATP_ASSERT_EQ(results[1], 20, "Second result is 20");
    FATP_ASSERT_EQ(results[2], 30, "Third result is 30");

    return true;
}

FATP_TEST_CASE(emit_collect_with_exceptions)
{
    Signal<int(int), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;

    auto c1 = sig.connect([](int x) {
        return x * 1;
    });
    auto c2 = sig.connect([](int) -> int {
        throw std::runtime_error("Skip");
    });
    auto c3 = sig.connect([](int x) {
        return x * 3;
    });

    auto results = sig.emitCollect(10);

    FATP_ASSERT_EQ(results.size(), 2, "Should have 2 results (one skipped)");
    FATP_ASSERT_EQ(results[0], 10, "First result is 10");
    FATP_ASSERT_EQ(results[1], 30, "Second result is 30 (third slot)");

    return true;
}

FATP_TEST_CASE(emit_collect_propagate_exception)
{
    Signal<int(int), SingleThreadedPolicy, PropagateExceptionPolicy> sig;
    int callCount = 0;

    auto c1 = sig.connect([&](int x) {
        ++callCount;
        return x * 1;
    });
    auto c2 = sig.connect([&](int) -> int {
        ++callCount;
        throw std::runtime_error("Stop");
    });
    auto c3 = sig.connect([&](int x) {
        ++callCount;
        return x * 3;
    });

    bool caught = false;
    try
    {
        auto results = sig.emitCollect(10);
        FATP_ASSERT_TRUE(false, "Should have thrown exception");
        (void)results;
    }
    catch (const std::runtime_error&)
    {
        caught = true;
    }

    FATP_ASSERT_TRUE(caught, "Exception should propagate from emitCollect");
    FATP_ASSERT_EQ(callCount, 2, "Only slots before and including throw should be called");

    return true;
}

FATP_TEST_CASE(emit_until)
{
    Signal<bool(int), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;
    int callCount = 0;

    auto c1 = sig.connect([&](int x) {
        ++callCount;
        return x > 10;
    });
    auto c2 = sig.connect([&](int x) {
        ++callCount;
        return x > 5;
    });
    auto c3 = sig.connect([&](int) {
        ++callCount;
        return true;
    });

    bool result = sig.emitUntil(7);

    FATP_ASSERT_TRUE(result, "Should return true (second slot)");
    FATP_ASSERT_EQ(callCount, 2, "Third slot should not be called");

    return true;
}

FATP_TEST_CASE(emit_until_with_exceptions)
{
    Signal<bool(int), SingleThreadedPolicy, CatchAndIgnorePolicy> sig;
    int callCount = 0;

    auto c1 = sig.connect([&](int) -> bool {
        ++callCount;
        throw std::runtime_error("Skip");
    });
    auto c2 = sig.connect([&](int) {
        ++callCount;
        return true;
    });

    bool result = sig.emitUntil(5);

    FATP_ASSERT_TRUE(result, "Should return true from second slot");
    FATP_ASSERT_EQ(callCount, 2, "Both slots attempted");

    return true;
}

FATP_TEST_CASE(thread_safe_emission)
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

    FATP_ASSERT_EQ(total.load(), numThreads * emitsPerThread, "All emissions should be counted");

    return true;
}

FATP_TEST_CASE(concurrent_connect_disconnect)
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

    FATP_ASSERT_GT(emitCount.load(), 0, "Should have emitted");

    return true;
}

FATP_TEST_CASE(thread_safe_disconnect_during_emission)
{
    ThreadSafeSignal<void()> sig;
    std::atomic<int> callCount{0};
    ConnectionId selfId;

    selfId = sig.connectManual([&]() {
        callCount.fetch_add(1);
        sig.disconnect(selfId);
    });

    sig.emit();
    FATP_ASSERT_EQ(callCount.load(), 1, "Should be called once");

    sig.emit();
    FATP_ASSERT_EQ(callCount.load(), 1, "Should not be called after disconnect");

    return true;
}

FATP_TEST_CASE(inline_storage_efficiency)
{
    Signal<void()> sig;

    auto c1 = sig.connect([]() {});
    auto c2 = sig.connect([]() {});
    auto c3 = sig.connect([]() {});
    auto c4 = sig.connect([]() {});

    FATP_ASSERT_EQ(sig.slotCount(), 4, "Should have 4 slots");

    auto c5 = sig.connect([]() {});
    FATP_ASSERT_EQ(sig.slotCount(), 5, "Should have 5 slots");

    int count = 0;
    auto verifier = sig.connect([&]() {
        ++count;
    });
    sig.emit();
    FATP_ASSERT_EQ(count, 1, "Verifier should be called");

    return true;
}

FATP_TEST_CASE(custom_inline_capacity_small)
{
    Signal<void(), SingleThreadedPolicy, CatchAndIgnorePolicy, 1> sig;
    int count = 0;

    auto c1 = sig.connect([&]() {
        ++count;
    });
    FATP_ASSERT_EQ(sig.slotCount(), 1, "Should have 1 slot");

    auto c2 = sig.connect([&]() {
        ++count;
    });
    auto c3 = sig.connect([&]() {
        ++count;
    });
    FATP_ASSERT_EQ(sig.slotCount(), 3, "Should have 3 slots after overflow");

    sig.emit();
    FATP_ASSERT_EQ(count, 3, "All 3 slots called");

    return true;
}

FATP_TEST_CASE(custom_inline_capacity_large)
{
    Signal<void(), SingleThreadedPolicy, CatchAndIgnorePolicy, 8> sig;
    int count = 0;

    std::vector<ScopedConnection> conns;
    for (int i = 0; i < 8; ++i)
    {
        conns.push_back(sig.connect([&]() {
            ++count;
        }));
    }
    FATP_ASSERT_EQ(sig.slotCount(), 8, "Should have 8 slots");

    sig.emit();
    FATP_ASSERT_EQ(count, 8, "All 8 slots called");

    return true;
}

FATP_TEST_CASE(spinlock_signal)
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

    FATP_ASSERT_EQ(total.load(), 400, "All emissions counted");

    return true;
}

FATP_TEST_CASE(local_signal_alias)
{
    LocalSignal<void(int)> sig;
    int value = 0;

    auto conn = sig.connect([&](int v) {
        value = v;
    });
    sig.emit(42);

    FATP_ASSERT_EQ(value, 42, "LocalSignal should work");

    return true;
}

FATP_TEST_CASE(empty_signal_emission)
{
    Signal<void()> sig;
    sig.emit();
    FATP_ASSERT_EQ(sig.slotCount(), 0, "Should still be empty");

    return true;
}

FATP_TEST_CASE(double_disconnect)
{
    Signal<void()> sig;

    ConnectionId id = sig.connectManual([]() {});

    FATP_ASSERT_TRUE(sig.disconnect(id), "First disconnect should succeed");
    FATP_ASSERT_FALSE(sig.disconnect(id), "Second disconnect should fail");

    return true;
}

FATP_TEST_CASE(double_disconnect_during_emission)
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
    FATP_ASSERT_EQ(disconnectCount, 1, "Only one disconnect should succeed");

    return true;
}

FATP_TEST_CASE(invalid_connection_id)
{
    Signal<void()> sig;

    FATP_ASSERT_FALSE(sig.disconnect(InvalidConnectionId), "Should return false");
    FATP_ASSERT_FALSE(sig.isConnected(InvalidConnectionId), "Should not be connected");

    return true;
}

FATP_TEST_CASE(move_signal)
{
    Signal<void()> sig1;
    int count = 0;

    auto conn = sig1.connect([&]() {
        ++count;
    });
    sig1.emit();
    FATP_ASSERT_EQ(count, 1, "Should be called once");

    Signal<void()> sig2 = std::move(sig1);
    sig2.emit();
    FATP_ASSERT_EQ(count, 2, "Should be called after move");

    return true;
}

FATP_TEST_CASE(move_assignment)
{
    Signal<void()> sig1;
    Signal<void()> sig2;
    int count1 = 0;
    int count2 = 0;

    auto conn1 = sig1.connect([&]() {
        ++count1;
    });
    auto conn2 = sig2.connect([&]() {
        ++count2;
    });

    sig1.emit();
    sig2.emit();
    FATP_ASSERT_EQ(count1, 1, "sig1 called");
    FATP_ASSERT_EQ(count2, 1, "sig2 called");

    sig2 = std::move(sig1);
    sig2.emit();
    FATP_ASSERT_EQ(count1, 2, "sig1's slot called via sig2");

    return true;
}

FATP_TEST_CASE(is_signal_trait)
{
    FATP_ASSERT_TRUE(is_signal_v<Signal<void()>>, "Signal<void()> is a signal");
    FATP_ASSERT_TRUE(is_signal_v<Signal<int(float, double)>>, "Signal<int(float,double)> is a signal");
    FATP_ASSERT_TRUE(is_signal_v<ThreadSafeSignal<void()>>, "ThreadSafeSignal is a signal");
    FATP_ASSERT_TRUE(is_signal_v<SpinlockSignal<void()>>, "SpinlockSignal is a signal");
    FATP_ASSERT_TRUE(is_signal_v<LocalSignal<void()>>, "LocalSignal is a signal");

    FATP_ASSERT_FALSE(is_signal_v<int>, "int is not a signal");
    FATP_ASSERT_FALSE(is_signal_v<std::function<void()>>, "std::function is not a signal");

    return true;
}

FATP_TEST_CASE(many_slots_performance)
{
    Signal<void()> sig;
    int count = 0;

    std::vector<ScopedConnection> connections;
    connections.reserve(100);

    for (int i = 0; i < 100; ++i)
    {
        connections.push_back(sig.connect([&]() {
            ++count;
        }));
    }

    FATP_ASSERT_EQ(sig.slotCount(), 100, "Should have 100 slots");

    sig.emit();
    FATP_ASSERT_EQ(count, 100, "All 100 slots called");

    return true;
}

// =============================================================================
// Connect-during-emission (deferred activation)
// =============================================================================

FATP_TEST_CASE(connect_during_emission_not_called_in_current_emission)
{
    Signal<void()> sig;
    int outerCalls = 0;
    int innerCalls = 0;
    ScopedConnection inner;

    auto outer = sig.connect([&]() {
        ++outerCalls;
        if (!inner.isConnected())
        {
            inner = sig.connect([&]() { ++innerCalls; });
        }
    });

    sig.emit();
    FATP_ASSERT_EQ(outerCalls, 1, "Outer slot runs in first emission");
    FATP_ASSERT_EQ(innerCalls, 0, "Slot connected mid-emission must not run in that emission");

    sig.emit();
    FATP_ASSERT_EQ(outerCalls, 2, "Outer slot runs in second emission");
    FATP_ASSERT_EQ(innerCalls, 1, "Deferred slot must be active in the next emission");

    return true;
}

FATP_TEST_CASE(connect_during_emission_growth_past_inline_capacity)
{
    // Regression: connecting from inside a callback used to insert into the
    // SlotList emit() was iterating; enough insertions force a SmallVector
    // reallocation under the iterating loop (heap corruption / crash).
    Signal<void()> sig;   // InlineCapacity 4
    int adderCalls = 0;
    int lateCalls  = 0;
    std::vector<ScopedConnection> late;

    auto adder = sig.connect([&]() {
        ++adderCalls;
        for (int i = 0; i < 32; ++i)
        {
            late.push_back(sig.connect([&]() { ++lateCalls; }));
        }
    });

    sig.emit();
    FATP_ASSERT_EQ(adderCalls, 1, "Adder slot runs once");
    FATP_ASSERT_EQ(lateCalls, 0, "None of the 32 deferred slots run in the same emission");

    late.clear();   // disconnect the 32 deferred slots again
    sig.emit();
    FATP_ASSERT_EQ(adderCalls, 2, "Adder slot runs in second emission");
    FATP_ASSERT_EQ(lateCalls, 0, "Disconnected deferred slots never run");

    return true;
}

FATP_TEST_CASE(connect_during_emission_activates_and_runs_next_time)
{
    Signal<void()> sig;
    int lateCalls = 0;
    ConnectionId lateId{InvalidConnectionId};
    bool added = false;

    auto adder = sig.connect([&]() {
        if (!added)
        {
            added = true;
            lateId = sig.connectManual([&]() { ++lateCalls; });
        }
    });

    sig.emit();
    FATP_ASSERT_TRUE(sig.isConnected(lateId),
        "A connection parked during emission must report isConnected");
    FATP_ASSERT_EQ(lateCalls, 0, "Parked slot must not have run yet");

    sig.emit();
    FATP_ASSERT_EQ(lateCalls, 1, "Parked slot runs after activation");

    FATP_ASSERT_TRUE(sig.disconnect(lateId), "Activated slot must disconnect normally");
    sig.emit();
    FATP_ASSERT_EQ(lateCalls, 1, "Disconnected slot must not run again");

    return true;
}

FATP_TEST_CASE(connect_then_disconnect_same_id_inside_callback)
{
    Signal<void()> sig;
    int  lateCalls = 0;
    bool connectedInside = false;
    bool disconnectReturned = false;
    bool disconnectedAfter = false;

    auto outer = sig.connect([&]() {
        ConnectionId id = sig.connectManual([&]() { ++lateCalls; });
        connectedInside    = sig.isConnected(id);
        disconnectReturned = sig.disconnect(id);
        disconnectedAfter  = !sig.isConnected(id);
    });

    sig.emit();
    FATP_ASSERT_TRUE(connectedInside,
        "Pending connection must report connected inside the callback");
    FATP_ASSERT_TRUE(disconnectReturned,
        "Disconnecting a pending connection inside the callback must succeed");
    FATP_ASSERT_TRUE(disconnectedAfter,
        "Erased pending connection must report disconnected");

    sig.emit();
    FATP_ASSERT_EQ(lateCalls, 0,
        "A connection created and disconnected inside a callback never runs");

    return true;
}

FATP_TEST_CASE(connect_during_emission_priority_respected_on_activation)
{
    Signal<void()> sig;
    std::vector<int> order;
    ScopedConnection high;
    ScopedConnection low;
    bool added = false;

    auto normal = sig.connect([&]() {
        order.push_back(0);
        if (!added)
        {
            added = true;
            high = sig.connect([&]() { order.push_back(10); }, 10);
            low  = sig.connect([&]() { order.push_back(-5); }, -5);
        }
    }, 0);

    sig.emit();   // only the normal slot; high/low parked
    order.clear();

    sig.emit();
    FATP_ASSERT_EQ(order.size(), 3, "All three slots run in the second emission");
    FATP_ASSERT_EQ(order[0], 10, "Deferred high-priority slot must be first");
    FATP_ASSERT_EQ(order[1], 0,  "Normal slot in the middle");
    FATP_ASSERT_EQ(order[2], -5, "Deferred low-priority slot must be last");

    return true;
}

FATP_TEST_CASE(connect_during_nested_emission_waits_for_outermost)
{
    Signal<void()> sig;
    int lateCalls = 0;
    int depth = 0;
    bool added = false;

    auto outer = sig.connect([&]() {
        ++depth;
        if (depth == 1)
        {
            sig.emit();   // nested emission
            if (!added)
            {
                added = true;
                (void)sig.connectManual([&]() { ++lateCalls; });
            }
        }
        --depth;
    });

    sig.emit();
    FATP_ASSERT_EQ(lateCalls, 0,
        "Slot parked during the outer emission must not run in it (or nested ones)");

    sig.emit();
    FATP_ASSERT_TRUE(lateCalls > 0, "Parked slot must participate after outermost completes");

    return true;
}

FATP_TEST_CASE(disconnect_all_during_emission_discards_pending)
{
    Signal<void()> sig;
    int lateCalls = 0;

    auto outer = sig.connect([&]() {
        (void)sig.connectManual([&]() { ++lateCalls; });
        sig.disconnectAll();
    });

    sig.emit();
    sig.emit();
    FATP_ASSERT_EQ(lateCalls, 0,
        "disconnectAll during emission must also discard pending connections");
    FATP_ASSERT_EQ(sig.activeSlotCount(), 0, "No slots survive disconnectAll");

    return true;
}
} // namespace fat_p::testing::signal

namespace fat_p::testing
{


inline void run_benchmarks()
{
    using namespace fat_p::testing;

    auto& out = *get_test_config().output;
    out << "\n" << colors::cyan() << "Sanity Benchmark:" << colors::reset() << "\n";
    out << "  (benchmarks are provided by benchmark executables; unit tests do not run full benchmarks)\n\n";
}

bool test_Signal()
{
    FATP_PRINT_HEADER(SIGNAL)

    TestRunner runner;

    FATP_RUN_TEST_NS(runner, signal, basic_connection);
    FATP_RUN_TEST_NS(runner, signal, multiple_connections);
    FATP_RUN_TEST_NS(runner, signal, manual_connect_disconnect);
    FATP_RUN_TEST_NS(runner, signal, call_operator);
    FATP_RUN_TEST_NS(runner, signal, scoped_connection_raii);
    FATP_RUN_TEST_NS(runner, signal, scoped_connection_move);
    FATP_RUN_TEST_NS(runner, signal, scoped_connection_release);
    FATP_RUN_TEST_NS(runner, signal, priority_ordering);
    FATP_RUN_TEST_NS(runner, signal, disconnect_during_emission);
    FATP_RUN_TEST_NS(runner, signal, connect_during_emission);
    FATP_RUN_TEST_NS(runner, signal, nested_emission);
    FATP_RUN_TEST_NS(runner, signal, slot_count);
    FATP_RUN_TEST_NS(runner, signal, is_connected);
    FATP_RUN_TEST_NS(runner, signal, disconnect_all);
    FATP_RUN_TEST_NS(runner, signal, member_function_connection);
    FATP_RUN_TEST_NS(runner, signal, catch_and_ignore_policy);
    FATP_RUN_TEST_NS(runner, signal, propagate_exception_policy);
    FATP_RUN_TEST_NS(runner, signal, emit_collect);
    FATP_RUN_TEST_NS(runner, signal, emit_collect_with_exceptions);
    FATP_RUN_TEST_NS(runner, signal, emit_collect_propagate_exception);
    FATP_RUN_TEST_NS(runner, signal, emit_until);
    FATP_RUN_TEST_NS(runner, signal, emit_until_with_exceptions);
    FATP_RUN_TEST_NS(runner, signal, thread_safe_emission);
    FATP_RUN_TEST_NS(runner, signal, concurrent_connect_disconnect);
    FATP_RUN_TEST_NS(runner, signal, thread_safe_disconnect_during_emission);
    FATP_RUN_TEST_NS(runner, signal, inline_storage_efficiency);
    FATP_RUN_TEST_NS(runner, signal, custom_inline_capacity_small);
    FATP_RUN_TEST_NS(runner, signal, custom_inline_capacity_large);
    FATP_RUN_TEST_NS(runner, signal, spinlock_signal);
    FATP_RUN_TEST_NS(runner, signal, local_signal_alias);
    FATP_RUN_TEST_NS(runner, signal, empty_signal_emission);
    FATP_RUN_TEST_NS(runner, signal, double_disconnect);
    FATP_RUN_TEST_NS(runner, signal, double_disconnect_during_emission);
    FATP_RUN_TEST_NS(runner, signal, invalid_connection_id);
    FATP_RUN_TEST_NS(runner, signal, move_signal);
    FATP_RUN_TEST_NS(runner, signal, move_assignment);
    FATP_RUN_TEST_NS(runner, signal, is_signal_trait);
    FATP_RUN_TEST_NS(runner, signal, many_slots_performance);
    FATP_RUN_TEST_NS(runner, signal, connect_during_emission_not_called_in_current_emission);
    FATP_RUN_TEST_NS(runner, signal, connect_during_emission_growth_past_inline_capacity);
    FATP_RUN_TEST_NS(runner, signal, connect_during_emission_activates_and_runs_next_time);
    FATP_RUN_TEST_NS(runner, signal, connect_then_disconnect_same_id_inside_callback);
    FATP_RUN_TEST_NS(runner, signal, connect_during_emission_priority_respected_on_activation);
    FATP_RUN_TEST_NS(runner, signal, connect_during_nested_emission_waits_for_outermost);
    FATP_RUN_TEST_NS(runner, signal, disconnect_all_during_emission_discards_pending);


    return 0 == runner.print_summary();
}

} // namespace fat_p::testing

// =============================================================================
// Standalone Entry Point
// =============================================================================
#ifdef ENABLE_TEST_APPLICATION
int main()
{
    return fat_p::testing::test_Signal() ? 0 : 1;
}
#endif

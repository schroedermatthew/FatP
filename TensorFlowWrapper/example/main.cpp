// example/main.cpp
// Comprehensive example demonstrating the TensorFlow C++20 wrapper
//
// This example shows:
// 1. Basic graph construction and execution
// 2. Thread-safe tensor access with views
// 3. Multi-threaded session execution
// 4. SharedMutex for concurrent reads
// 5. Callback-based access
// 6. Error handling
// 7. Different tensor factories

#include "tf/all.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <numeric>
#include <random>
#include <thread>
#include <vector>

// ============================================================================
// Example 1: Basic graph construction and execution
// ============================================================================

void example_basic() {
    std::cout << "=== Example 1: Basic Graph Construction ===\n";
    
    // Create a simple graph: Const -> Identity
    tf::Graph<> graph;
    
    // Create a constant tensor [1, 8] with value 0.5
    std::vector<std::int64_t> shape = {1, 8};
    std::vector<float> values(8, 0.5f);
    auto const_tensor = tf::Tensor<>::FromVector<float>(shape, values);
    
    // Add Const operation
    auto const_op = std::move(graph.NewOperation("Const", "my_constant"))
        .SetAttrTensor("value", const_tensor.handle())
        .SetAttrType("dtype", TF_FLOAT)
        .Finish();
    
    // Add Identity operation
    std::move(graph.NewOperation("Identity", "output"))
        .AddInput(const_op, 0)
        .Finish();
    
    // Create session and run
    tf::Session<> session(graph);
    auto results = session.Run({tf::Fetch{"output", 0}});
    
    // Access results safely
    auto view = results[0].read<float>();
    std::cout << "Output (" << view.size() << " elements): ";
    for (float x : view) {
        std::cout << x << " ";
    }
    std::cout << "\n\n";
}

// ============================================================================
// Example 2: Thread-safe session with Mutex policy
// ============================================================================

void example_threaded_session() {
    std::cout << "=== Example 2: Multi-threaded Session ===\n";
    
    // Build graph (single-threaded)
    tf::Graph<> graph;
    
    std::vector<std::int64_t> shape = {1, 4};
    std::vector<float> values = {1.0f, 2.0f, 3.0f, 4.0f};
    auto tensor = tf::Tensor<>::FromVector<float>(shape, values);
    
    auto const_op = std::move(graph.NewOperation("Const", "data"))
        .SetAttrTensor("value", tensor.handle())
        .SetAttrType("dtype", TF_FLOAT)
        .Finish();
    
    std::move(graph.NewOperation("Identity", "result"))
        .AddInput(const_op, 0)
        .Finish();
    
    // Thread-safe session with Mutex policy
    tf::Session<tf::policy::Mutex> session(graph);
    
    std::atomic<int> total_runs{0};
    constexpr int num_threads = 4;
    constexpr int runs_per_thread = 5;
    
    std::vector<std::thread> workers;
    for (int i = 0; i < num_threads; ++i) {
        workers.emplace_back([&session, &total_runs, i]() {
            for (int j = 0; j < runs_per_thread; ++j) {
                auto results = session.Run({tf::Fetch{"result", 0}});
                
                auto view = results[0].read<float>();
                float sum = std::accumulate(view.begin(), view.end(), 0.0f);
                
                ++total_runs;
                std::cout << "Thread " << i << " run " << j 
                          << ": sum = " << sum << "\n";
            }
        });
    }
    
    for (auto& t : workers) {
        t.join();
    }
    
    std::cout << "Total runs completed: " << total_runs << "\n\n";
}

// ============================================================================
// Example 3: SharedMutex for concurrent tensor reads
// ============================================================================

void example_shared_tensor() {
    std::cout << "=== Example 3: SharedMutex Concurrent Reads ===\n";
    
    // Create tensor with SharedMutex policy
    std::vector<std::int64_t> shape = {1000};
    std::vector<float> data(1000);
    std::iota(data.begin(), data.end(), 0.0f);
    
    tf::Tensor<tf::policy::SharedMutex> tensor = 
        tf::Tensor<tf::policy::SharedMutex>::FromVector<float>(shape, data);
    
    std::atomic<int> max_concurrent{0};
    std::atomic<int> current_readers{0};
    
    auto reader = [&]() {
        for (int i = 0; i < 20; ++i) {
            auto view = tensor.read<float>();  // Shared lock
            
            int n = ++current_readers;
            max_concurrent = std::max(max_concurrent.load(), n);
            
            // Simulate work
            float sum = 0;
            for (float x : view.first(100)) sum += x;
            (void)sum;
            
            std::this_thread::sleep_for(std::chrono::microseconds(50));
            --current_readers;
        }
    };
    
    std::thread t1(reader), t2(reader), t3(reader), t4(reader);
    t1.join(); t2.join(); t3.join(); t4.join();
    
    std::cout << "Max concurrent readers: " << max_concurrent 
              << " (should be > 1 with SharedMutex)\n\n";
}

// ============================================================================
// Example 4: Write view for safe mutation
// ============================================================================

void example_write_view() {
    std::cout << "=== Example 4: Write View ===\n";
    
    std::vector<std::int64_t> shape = {8};
    auto tensor = tf::Tensor<tf::policy::Mutex>::Zeros<float>(shape);
    
    std::cout << "Before: ";
    {
        auto view = tensor.read<float>();
        for (float x : view) std::cout << x << " ";
    }
    std::cout << "\n";
    
    // Write using exclusive lock
    {
        auto view = tensor.write<float>();
        for (std::size_t i = 0; i < view.size(); ++i) {
            view[i] = static_cast<float>(i * i);
        }
    }  // Lock released
    
    std::cout << "After:  ";
    {
        auto view = tensor.read<float>();
        for (float x : view) std::cout << x << " ";
    }
    std::cout << "\n\n";
}

// ============================================================================
// Example 5: Callback-based access
// ============================================================================

void example_callbacks() {
    std::cout << "=== Example 5: Callback-based Access ===\n";
    
    std::vector<std::int64_t> shape = {5};
    std::vector<float> data = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    
    tf::SafeTensor tensor = tf::SafeTensor::FromVector<float>(shape, data);
    
    // Read with callback
    float sum = tensor.with_read<float>([](std::span<const float> s) {
        return std::accumulate(s.begin(), s.end(), 0.0f);
    });
    std::cout << "Sum: " << sum << "\n";
    
    // Write with callback
    tensor.with_write<float>([](std::span<float> s) {
        for (float& x : s) x *= 2.0f;
    });
    
    // Verify
    float new_sum = tensor.with_read<float>([](std::span<const float> s) {
        return std::accumulate(s.begin(), s.end(), 0.0f);
    });
    std::cout << "Sum after doubling: " << new_sum << "\n\n";
}

// ============================================================================
// Example 6: Different tensor factories
// ============================================================================

void example_factories() {
    std::cout << "=== Example 6: Tensor Factories ===\n";
    
    // FromVector
    auto t1 = tf::FastTensor::FromVector<float>({2, 2}, {1, 2, 3, 4});
    std::cout << "FromVector: shape=[" << t1.shape()[0] << "," << t1.shape()[1] 
              << "], dtype=" << t1.dtype_name() << "\n";
    
    // FromScalar
    auto t2 = tf::FastTensor::FromScalar<double>(3.14159);
    std::cout << "FromScalar: value=" << t2.read<double>()[0] 
              << ", dtype=" << t2.dtype_name() << "\n";
    
    // Allocate (uninitialized)
    auto t3 = tf::FastTensor::Allocate<std::int32_t>({100});
    std::cout << "Allocate: " << t3.num_elements() << " elements, "
              << t3.byte_size() << " bytes\n";
    
    // Zeros
    auto t4 = tf::FastTensor::Zeros<float>({3, 3});
    bool all_zero = t4.with_read<float>([](std::span<const float> s) {
        return std::all_of(s.begin(), s.end(), [](float x) { return x == 0.0f; });
    });
    std::cout << "Zeros: all zero = " << std::boolalpha << all_zero << "\n\n";
}

// ============================================================================
// Example 7: Error handling
// ============================================================================

void example_errors() {
    std::cout << "=== Example 7: Error Handling ===\n";
    
    auto tensor = tf::FastTensor::FromScalar<float>(1.0f);
    
    // Type mismatch
    try {
        auto view = tensor.read<double>();
        std::cout << "This should not print\n";
    } catch (const std::runtime_error& e) {
        std::cout << "Caught dtype error: " << e.what() << "\n";
    }
    
    // Null tensor
    try {
        tf::FastTensor::FromRaw(nullptr);
        std::cout << "This should not print\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Caught null error: " << e.what() << "\n";
    }
    
    // Dimension mismatch
    try {
        tf::FastTensor::FromVector<float>({2, 3}, {1, 2, 3});  // Need 6, got 3
        std::cout << "This should not print\n";
    } catch (const std::invalid_argument& e) {
        std::cout << "Caught dimension error (truncated): " 
                  << std::string(e.what()).substr(0, 60) << "...\n";
    }
    
    std::cout << "\n";
}

// ============================================================================
// Example 8: Graph with different policies
// ============================================================================

void example_graph_policies() {
    std::cout << "=== Example 8: Graph Policy Flexibility ===\n";
    
    // Graph with SharedMutex
    tf::SharedGraph graph;
    
    auto tensor = tf::FastTensor::FromScalar<float>(42.0f);
    
    auto const_op = std::move(graph.NewOperation("Const", "answer"))
        .SetAttrTensor("value", tensor.handle())
        .SetAttrType("dtype", TF_FLOAT)
        .Finish();
    
    std::move(graph.NewOperation("Identity", "out"))
        .AddInput(const_op, 0)
        .Finish();
    
    // Session with Mutex, Graph with SharedMutex - different policies OK!
    tf::SafeSession session(graph);
    
    auto result = session.Run("out");
    std::cout << "Result: " << result.read<float>()[0] << "\n\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    try {
        std::cout << "╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║     TensorFlow C++20 Wrapper - Merged Implementation     ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
        
        example_basic();
        example_threaded_session();
        example_shared_tensor();
        example_write_view();
        example_callbacks();
        example_factories();
        example_errors();
        example_graph_policies();
        
        std::cout << "✓ All examples completed successfully!\n";
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "✗ Fatal error: " << e.what() << "\n";
        return 1;
    }
}

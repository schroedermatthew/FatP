// tf/all.hpp
// Umbrella header for TensorFlow C++20 wrapper
//
// Include this single header to get all wrapper functionality.

#pragma once

#include "tf/policy.hpp"
#include "tf/status.hpp"
#include "tf/guarded_span.hpp"
#include "tf/tensor.hpp"
#include "tf/operation.hpp"
#include "tf/graph.hpp"
#include "tf/session.hpp"

// ============================================================================
// TensorFlow C++20 Wrapper - Quick Reference
// ============================================================================
//
// THREAD SAFETY POLICIES:
// ─────────────────────────────────────────────────────────────────────────────
//   tf::policy::NoLock      - Zero overhead, no synchronization (default)
//   tf::policy::Mutex       - Exclusive locking for thread-safe writes
//   tf::policy::SharedMutex - Reader-writer locks (concurrent reads OK)
//
// CORE CLASSES:
// ─────────────────────────────────────────────────────────────────────────────
//   tf::Tensor<Policy>      - RAII wrapper for TF_Tensor
//   tf::Graph<Policy>       - RAII wrapper for TF_Graph
//   tf::Session<Policy>     - RAII wrapper for TF_Session
//   tf::Operation           - Non-owning handle to TF_Operation
//   tf::Status              - RAII wrapper for TF_Status
//   tf::SessionOptions      - RAII wrapper for TF_SessionOptions
//   tf::GuardedSpan<T,G>    - Thread-safe view (span + lock guard)
//
// TYPE ALIASES:
// ─────────────────────────────────────────────────────────────────────────────
//   tf::FastTensor   = tf::Tensor<tf::policy::NoLock>
//   tf::SafeTensor   = tf::Tensor<tf::policy::Mutex>
//   tf::SharedTensor = tf::Tensor<tf::policy::SharedMutex>
//
//   tf::FastGraph    = tf::Graph<tf::policy::NoLock>
//   tf::SafeGraph    = tf::Graph<tf::policy::Mutex>
//   tf::SharedGraph  = tf::Graph<tf::policy::SharedMutex>
//
//   tf::FastSession  = tf::Session<tf::policy::NoLock>
//   tf::SafeSession  = tf::Session<tf::policy::Mutex>
//
// THREAD-SAFE TENSOR ACCESS:
// ─────────────────────────────────────────────────────────────────────────────
//   // View-based (lock held for view lifetime):
//   auto view = tensor.read<float>();    // Shared lock
//   auto view = tensor.write<float>();   // Exclusive lock
//   for (float x : view) { ... }
//
//   // Callback-based (hardest to misuse):
//   tensor.with_read<float>([](std::span<const float> s) { ... });
//   tensor.with_write<float>([](std::span<float> s) { ... });
//
//   // Unsafe (NO lock - caller must synchronize):
//   float* p = tensor.unsafe_data<float>();
//
// EXAMPLE USAGE:
// ─────────────────────────────────────────────────────────────────────────────
//   // Build graph
//   tf::Graph<> graph;
//   
//   auto tensor = tf::Tensor<>::FromVector<float>({1, 4}, {1, 2, 3, 4});
//   
//   auto const_op = graph.NewOperation("Const", "my_const")
//       .SetAttrTensor("value", tensor.handle())
//       .SetAttrType("dtype", TF_FLOAT)
//       .Finish();
//   
//   graph.NewOperation("Identity", "output")
//       .AddInput(const_op, 0)
//       .Finish();
//   
//   // Run inference
//   tf::Session<tf::policy::Mutex> session(graph);  // Thread-safe session
//   auto results = session.Run({tf::Fetch{"output", 0}});
//   
//   // Access results
//   auto view = results[0].read<float>();
//   for (float x : view) {
//       std::cout << x << " ";
//   }
//
// LOAD SAVEDMODEL (recommended for production):
// ─────────────────────────────────────────────────────────────────────────────
//   auto [session, graph] = tf::Session<>::LoadSavedModel("/path/to/model");
//   auto result = session.Run({tf::Feed{"input", tensor}}, {tf::Fetch{"output"}});
//
// DEVICE ENUMERATION:
// ─────────────────────────────────────────────────────────────────────────────
//   auto devices = session.ListDevices();
//   for (int i = 0; i < devices.count(); ++i) {
//       auto dev = devices.at(i);
//       std::cout << dev.name << " (" << dev.type << ")\n";
//   }
//   if (session.HasGPU()) { /* use GPU */ }
//
// SCALAR TYPES SUPPORTED:
// ─────────────────────────────────────────────────────────────────────────────
//   float, double
//   int8_t, int16_t, int32_t, int64_t
//   uint8_t, uint16_t, uint32_t, uint64_t
//   bool
//

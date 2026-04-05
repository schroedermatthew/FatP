#pragma once

/*
FATP_META:
  meta_version: 1
  component: DiagnosticContext
  file_role: public_header
  path: include/fat_p/DiagnosticContext.h
  namespace: [fat_p, fat_p::detail]
  layer: Foundation
  summary: "Thread-local diagnostic context stack for contract enforcement."
  api_stability: in_work
  related:
    docs_search: "DiagnosticContext"
    tests:
      - components/DiagnosticContext/tests/test_DiagnosticContext.cpp
      - components/DiagnosticContext/tests/test_DiagnosticContext_enforce.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 5
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
*/

/**
 * @file DiagnosticContext.h
 * @brief Thread-local diagnostic context stack for contract enforcement.
 *
 * Provides scoped key-value context that is automatically included in
 * enforcement failure messages.  Outer code pushes context where the
 * information is known; inner code's enforcement calls include it
 * automatically without parameter threading.
 *
 * @par Usage
 * @code
 * void processMesh(const Mesh& mesh, int timestep)
 * {
 *     FATP_DIAG_CONTEXT("timestep", timestep);
 *     for (int cell = 0; cell < n; ++cell)
 *     {
 *         FATP_DIAG_CONTEXT("cell", cell);
 *         compute(mesh, cell);  // enforce calls inside see the context
 *     }
 * }
 * @endcode
 *
 * @par Cost
 * Happy path: one pointer write per scope entry/exit (push/pop on a
 * thread-local intrusive linked list).  No allocation, no I/O, no
 * formatting.  Failure path: formats the context stack into a string.
 *
 * @par Thread Safety
 * The context stack is thread_local.  Each thread has its own stack.
 * No synchronization needed.
 *
 * @par Integration
 * The Enforcer in enforce_enforcers.h calls
 * DiagnosticContext::formatCurrent() in its fail_impl() and appends
 * the result to the error message.  No changes to enforce macros or
 * call sites are required.
 */

#include <sstream>
#include <string>
#include <type_traits>

// Self-contained formatting by default.  Define FATP_DIAG_USE_STRINGIFY
// before including this header to use fat_p::toString from Stringify.h
// for richer custom-type formatting.
#if defined(FATP_DIAG_USE_STRINGIFY)
#include "Stringify.h"
#define FATP_DIAG_TO_STRING_(val) fat_p::toString(val)
#else
namespace fat_p::detail {

/// @brief Fallback value-to-string conversion for diagnostic context.
template<typename T>
std::string diagToString(const T& val)
{
    if constexpr (std::is_same_v<std::decay_t<T>, std::string>)
        return val;
    else if constexpr (std::is_convertible_v<T, const char*>)
        return std::string(val);
    else if constexpr (std::is_arithmetic_v<std::decay_t<T>>)
        return std::to_string(val);
    else
    {
        std::ostringstream ss;
        ss << val;
        return ss.str();
    }
}

} // namespace fat_p::detail
#define FATP_DIAG_TO_STRING_(val) ::fat_p::detail::diagToString(val)
#endif

namespace fat_p
{

// ============================================================================
// DiagnosticContextFrame
// ============================================================================

/**
 * @brief RAII scope guard for one diagnostic context entry.
 *
 * Pushes a pre-formatted description onto the thread-local context
 * stack on construction and pops it on destruction.  Frame objects
 * live on the stack as local variables — no heap allocation for the
 * list structure.
 */
class DiagnosticContextFrame
{
public:
    /// @brief Construct with a pre-formatted description and push onto the stack.
    explicit DiagnosticContextFrame(std::string description) noexcept
        : mDescription(std::move(description))
        , mPrev(head())
    {
        head() = this;
    }

    /// @brief Pop this frame from the thread-local context stack.
    ~DiagnosticContextFrame()
    {
        head() = mPrev;
    }

    DiagnosticContextFrame(const DiagnosticContextFrame&) = delete;
    DiagnosticContextFrame& operator=(const DiagnosticContextFrame&) = delete;
    DiagnosticContextFrame(DiagnosticContextFrame&&) = delete;
    DiagnosticContextFrame& operator=(DiagnosticContextFrame&&) = delete;

    /// @brief Returns this frame's formatted description.
    const std::string& description() const noexcept { return mDescription; }

    /// @brief Returns the previous frame in the stack (toward the bottom).
    const DiagnosticContextFrame* prev() const noexcept { return mPrev; }

private:
    std::string mDescription;
    DiagnosticContextFrame* mPrev;

    /// @brief Thread-local stack head.
    static DiagnosticContextFrame*& head() noexcept
    {
        static thread_local DiagnosticContextFrame* sHead = nullptr;
        return sHead;
    }

    friend class DiagnosticContext;
};

// ============================================================================
// DiagnosticContext
// ============================================================================

/**
 * @brief Static interface to the thread-local diagnostic context stack.
 *
 * All methods are static.  The context stack is read by the Enforcer
 * on the failure path to enrich error messages.
 */
class DiagnosticContext
{
public:
    /// @brief Returns true if any context frames are on the current thread's stack.
    static bool hasContext() noexcept
    {
        return DiagnosticContextFrame::head() != nullptr;
    }

    /**
     * @brief Format the current context stack into a string.
     * @return Formatted context, or empty string if no context is set.
     *
     * Output reads outer-to-inner (the order frames were pushed):
     * "timestep=100, cell=37, face=2"
     *
     * Caps at 256 frames to avoid unbounded stack walking.  Context
     * deeper than 256 frames is silently truncated — the outermost
     * (oldest) frames are dropped.
     *
     * Only called on the failure path — never on the happy path.
     */
    static std::string formatCurrent()
    {
        auto* frame = DiagnosticContextFrame::head();
        if (!frame)
            return {};

        static constexpr std::size_t kMaxFrames = 256;
        const DiagnosticContextFrame* frames[kMaxFrames];
        std::size_t count = 0;
        while (frame && count < kMaxFrames)
        {
            frames[count++] = frame;
            frame = const_cast<DiagnosticContextFrame*>(frame->prev());
        }

        std::string result;
        for (std::size_t i = count; i > 0; --i)
        {
            if (i < count)
                result += ", ";
            result += frames[i - 1]->description();
        }
        return result;
    }

    /// @brief Returns the depth of the current context stack.
    static std::size_t depth() noexcept
    {
        std::size_t n = 0;
        auto* frame = DiagnosticContextFrame::head();
        while (frame)
        {
            ++n;
            frame = const_cast<DiagnosticContextFrame*>(frame->prev());
        }
        return n;
    }
};

// ============================================================================
// Formatting helpers
// ============================================================================

namespace detail
{

/// @brief Format one key-value pair into a string.
template<typename V>
std::string formatContext(const char* key, V&& value)
{
    std::ostringstream ss;
    ss << key << '=' << FATP_DIAG_TO_STRING_(std::forward<V>(value));
    return ss.str();
}

/// @brief Format two key-value pairs into a string.
template<typename V1, typename V2>
std::string formatContext(const char* k1, V1&& v1,
                          const char* k2, V2&& v2)
{
    std::ostringstream ss;
    ss << k1 << '=' << FATP_DIAG_TO_STRING_(std::forward<V1>(v1))
       << ", " << k2 << '=' << FATP_DIAG_TO_STRING_(std::forward<V2>(v2));
    return ss.str();
}

/// @brief Format three key-value pairs into a string.
template<typename V1, typename V2, typename V3>
std::string formatContext(const char* k1, V1&& v1,
                          const char* k2, V2&& v2,
                          const char* k3, V3&& v3)
{
    std::ostringstream ss;
    ss << k1 << '=' << FATP_DIAG_TO_STRING_(std::forward<V1>(v1))
       << ", " << k2 << '=' << FATP_DIAG_TO_STRING_(std::forward<V2>(v2))
       << ", " << k3 << '=' << FATP_DIAG_TO_STRING_(std::forward<V3>(v3));
    return ss.str();
}

// For more than 3 key-value pairs per scope, use multiple
// FATP_DIAG_CONTEXT calls — each creates a separate frame.

} // namespace detail

} // namespace fat_p

// ============================================================================
// Macros
// ============================================================================

#define FATP_DIAG_CONTEXT_IMPL_(line, ...)                                    \
    ::fat_p::DiagnosticContextFrame fatpDiagCtx##line(                        \
        ::fat_p::detail::formatContext(__VA_ARGS__))

#define FATP_DIAG_CONTEXT_EXPAND_(line, ...) \
    FATP_DIAG_CONTEXT_IMPL_(line, __VA_ARGS__)

/// @brief Push a diagnostic context frame (1-3 key-value pairs).
/// The frame is popped when the enclosing scope exits.
#define FATP_DIAG_CONTEXT(...) \
    FATP_DIAG_CONTEXT_EXPAND_(__LINE__, __VA_ARGS__)

/// @brief Debug-only diagnostic context.  Compiles to nothing in Release.
#ifdef NDEBUG
#define FATP_DEBUG_DIAG_CONTEXT(...) ((void)0)
#else
#define FATP_DEBUG_DIAG_CONTEXT(...) FATP_DIAG_CONTEXT(__VA_ARGS__)
#endif

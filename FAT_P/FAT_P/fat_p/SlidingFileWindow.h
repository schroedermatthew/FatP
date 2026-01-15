/**
 * @file SlidingFileWindow.h
 * @brief Policy-based sliding window access to large binary files with on-demand paging.
 *
 *
 *
 * @layer Domain
 *
 * @details Provides efficient deque-like access to large binary files that don't fit in memory.
 * Uses a sliding window mechanism with configurable size and automatic paging. Elements are
 * loaded on-demand and written back when the window shifts.
 *
 * Key features:
 * - Policy-based error handling (Expected or throwing)
 * - Configurable serialization policies for element I/O
 * - Sliding window with automatic paging (O(1) amortized access)
 * - Thread-safe access via ConcurrencyPolicy
 * - RAII-compliant file management with flush-on-close
 * - Fail-safe fallback for out-of-window access (direct I/O)
 * - Debug-only bounds checking (zero overhead in release)
 * - Support for both forward and reverse window shifting
 *
 * @performance
 * - In-window access: O(1)
 * - Out-of-window access: O(1) with I/O penalty
 * - Window shift: O(shift_distance) with I/O
 * - Memory: sizeof(ElementType) * window_size
 *
 * @section policies Available Policies
 *
 * **SerializationPolicy:**
 * - BinarySerializationPolicy: Raw binary I/O (memcpy, fastest)
 * - StreamSerializationPolicy: operator<< / operator>> based
 * - CustomSerializationPolicy: User-defined Read/Write methods
 *
 * **ErrorPolicy:**
 * - ExpectedFileErrorPolicy: Returns Expected<T, FileError>
 * - ThrowingFileErrorPolicy: Throws FileWindowException
 *
 * **ConcurrencyPolicy:**
 * - SingleThreadedPolicy: No synchronization (default)
 * - MutexSynchronizationPolicy: std::mutex protection
 * - SharedMutexPolicy: Read-write lock for concurrent reads
 *
 * @example Basic Usage
 * @code
 * struct DataPoint {
 *     double value;
 *     uint64_t timestamp;
 *
 *     void Read(std::istream& is) {
 *         is.read(reinterpret_cast<char*>(&value), sizeof(value));
 *         is.read(reinterpret_cast<char*>(&timestamp), sizeof(timestamp));
 *     }
 *
 *     void Write(std::ostream& os) const {
 *         os.write(reinterpret_cast<const char*>(&value), sizeof(value));
 *         os.write(reinterpret_cast<const char*>(&timestamp), sizeof(timestamp));
 *     }
 * };
 *
 * SlidingFileWindow<DataPoint> window;
 * auto result = window.open("data.bin", sizeof(DataPoint), 1000);
 * if (result) {
 *     for (size_t i = 0; i < window.size(); ++i) {
 *         auto elem_result = window[i];
 *         if (elem_result) {
 *             DataPoint& elem = *elem_result;
 *             elem.value *= 2.0;
 *         }
 *     }
 * }
 * @endcode
 *
 * @note Elements must be fixed-size for binary serialization
 * @note Window automatically flushes dirty elements on shift/close
 * @note C++17 minimum (uses std::filesystem, std::optional, if constexpr)
 * @note Header-only, no external dependencies beyond standard library
 */

#pragma once

/*
FATP_META:
  meta_version: 1
  component: SlidingFileWindow
  file_role: public_header
  path: fat_p/SlidingFileWindow.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for SlidingFileWindow."
  api_stability: in_work
  related:
    docs_search: "SlidingFileWindow"
    tests:
      - tests/test_SlidingFileWindow.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "ConcurrencyPolicies.h"
#include "Expected.h"

namespace fat_p
{

// =============================================================================
// Error Types
// =============================================================================

/**
 * @brief Error codes for file window operations
 */
enum class FileError
{
    FileNotOpen,
    InvalidIndex,
    ReadFailure,
    WriteFailure,
    SeekFailure,
    InvalidWindowSize,
    CorruptedState,
    AllocationFailure
};

/**
 * @brief Exception thrown when file operations fail (ThrowingFileErrorPolicy)
 */
class FileWindowException : public std::runtime_error
{
public:
    explicit FileWindowException(FileError error, const std::string& message = "")
        : std::runtime_error(format_message(error, message))
        , mError(error)
    {
    }

    FileError error() const noexcept
    {
        return mError;
    }

private:
    FileError mError;

    static std::string format_message(FileError error, const std::string& msg)
    {
        std::string base = "FileWindow error: ";
        switch (error)
        {
            case FileError::FileNotOpen:
                base += "File not open";
                break;
            case FileError::InvalidIndex:
                base += "Invalid index";
                break;
            case FileError::ReadFailure:
                base += "Read failure";
                break;
            case FileError::WriteFailure:
                base += "Write failure";
                break;
            case FileError::SeekFailure:
                base += "Seek failure";
                break;
            case FileError::InvalidWindowSize:
                base += "Invalid window size";
                break;
            case FileError::CorruptedState:
                base += "Corrupted state";
                break;
            case FileError::AllocationFailure:
                base += "Allocation failure";
                break;
        }
        return msg.empty() ? base : base + " - " + msg;
    }
};

// =============================================================================
// SerializationPolicy - How elements are read/written
// =============================================================================

/**
 * @brief Binary serialization using memcpy (fastest, requires POD-like types)
 * @tparam ElementType Type of elements (must be trivially copyable)
 */
template <typename ElementType>
class BinarySerializationPolicy
{
    static_assert(std::is_trivially_copyable_v<ElementType>,
                  "BinarySerializationPolicy requires trivially copyable type");

public:
    static bool read(std::istream& stream, ElementType& element) noexcept
    {
        stream.read(reinterpret_cast<char*>(&element), sizeof(ElementType));
        return stream.good();
    }

    static bool write(std::ostream& stream, const ElementType& element) noexcept
    {
        stream.write(reinterpret_cast<const char*>(&element), sizeof(ElementType));
        return stream.good();
    }

    static constexpr size_t element_size() noexcept
    {
        return sizeof(ElementType);
    }
};

/**
 * @brief Stream-based serialization using operator<< and operator>>
 * @tparam ElementType Type of elements (must support stream operators)
 */
template <typename ElementType>
class StreamSerializationPolicy
{
public:
    static bool read(std::istream& stream, ElementType& element)
    {
        try
        {
            stream >> element;
            return stream.good();
        }
        catch (...)
        {
            return false;
        }
    }

    static bool write(std::ostream& stream, const ElementType& element)
    {
        try
        {
            stream << element;
            return stream.good();
        }
        catch (...)
        {
            return false;
        }
    }

    static constexpr size_t element_size() noexcept
    {
        return sizeof(ElementType);
    }
};

/**
 * @brief Custom serialization using ElementType::Read() and ElementType::Write()
 * @tparam ElementType Type with Read(std::istream&) and Write(std::ostream&) methods
 */
template <typename ElementType>
class CustomSerializationPolicy
{
public:
    static bool read(std::istream& stream, ElementType& element)
    {
        try
        {
            element.Read(stream);
            return stream.good();
        }
        catch (...)
        {
            return false;
        }
    }

    static bool write(std::ostream& stream, const ElementType& element)
    {
        try
        {
            element.Write(stream);
            return stream.good();
        }
        catch (...)
        {
            return false;
        }
    }

    static constexpr size_t element_size() noexcept
    {
        return sizeof(ElementType);
    }
};

// =============================================================================
// ErrorPolicy - How errors are reported
// =============================================================================

/**
 * @brief Report errors via Expected<T, FileError>
 *
 * @note For reference types T&, uses std::reference_wrapper<T> since Expected
 *       doesn't support reference types directly.
 */
template <typename T, typename Error = FileError>
class ExpectedFileErrorPolicy
{
private:
    template <typename U>
    struct wrap_reference
    {
        using type = U;
    };

    template <typename U>
    struct wrap_reference<U&>
    {
        using type = std::reference_wrapper<U>;
    };

public:
    using wrapped_type = typename wrap_reference<T>::type;
    using result_type = Expected<wrapped_type, Error>;
    using void_result_type = Expected<void, Error>;

    static result_type report_error(Error error) noexcept
    {
        return make_unexpected(error);
    }

    template <typename U>
    static result_type report_success(U&& value) noexcept
    {
        return result_type(std::forward<U>(value));
    }

    static void_result_type report_void_error(Error error) noexcept
    {
        return make_unexpected(error);
    }

    static void_result_type report_void_success() noexcept
    {
        return void_result_type{};
    }
};

/**
 * @brief Report errors by throwing FileWindowException
 */
template <typename T>
class ThrowingFileErrorPolicy
{
public:
    using result_type = T;
    using void_result_type = void;

    [[noreturn]] static result_type report_error(FileError error)
    {
        throw FileWindowException(error);
    }

    template <typename U>
    static result_type report_success(U&& value) noexcept
    {
        return std::forward<U>(value);
    }

    [[noreturn]] static void_result_type report_void_error(FileError error)
    {
        throw FileWindowException(error);
    }

    static void_result_type report_void_success() noexcept
    {
    }
};

// =============================================================================
// Thread-Safety Policy Trait
// =============================================================================

/**
 * @brief Trait to detect if a ConcurrencyPolicy requires thread-safe return semantics
 *
 * Thread-safe policies must return copies (not references) from operator[] because
 * the lock is released before the caller uses the returned value. Returning a
 * reference would create a race condition where concurrent window shifts could
 * invalidate the reference.
 */
template <typename Policy>
struct is_threadsafe_policy : std::bool_constant<!std::is_same_v<Policy, SingleThreadedPolicy>>
{
};

template <typename Policy>
inline constexpr bool is_threadsafe_policy_v = is_threadsafe_policy<Policy>::value;

// =============================================================================
// SlidingFileWindow - Main class with policy composition
// =============================================================================

/**
 * @brief Policy-based sliding window for large file access
 *
 * @tparam ElementType Type of elements in file
 * @tparam SerializationPolicy How elements are serialized
 * @tparam ErrorPolicy How errors are reported
 * @tparam ConcurrencyPolicy Thread-safety policy
 *
 * @note For thread-safe variants (ConcurrencyPolicy != SingleThreadedPolicy),
 *       operator[] returns element copies rather than references. This prevents
 *       race conditions where a returned reference could be invalidated by
 *       concurrent window shifts.
 */
template <typename ElementType,
          typename SerializationPolicy = CustomSerializationPolicy<ElementType>,
          typename ErrorPolicy = ExpectedFileErrorPolicy<ElementType&, FileError>,
          typename ConcurrencyPolicy = SingleThreadedPolicy>
class SlidingFileWindow : private SerializationPolicy
{
public:
    // =============================================================================
    // Type Traits and Definitions
    // =============================================================================

    static constexpr bool is_threadsafe = is_threadsafe_policy_v<ConcurrencyPolicy>;

    using element_type = ElementType;

    // Single-threaded: use provided ErrorPolicy (typically returns reference)
    // Thread-safe: always return by value to prevent reference-outlives-lock bugs
    using result_type =
        std::conditional_t<is_threadsafe, Expected<ElementType, FileError>, typename ErrorPolicy::result_type>;

    using const_result_type = std::conditional_t<is_threadsafe,
                                                 Expected<ElementType, FileError>,
                                                 Expected<std::reference_wrapper<const ElementType>, FileError>>;

    using void_result_type = typename ErrorPolicy::void_result_type;

    // =============================================================================
    // Construction
    // =============================================================================

    SlidingFileWindow() = default;

    ~SlidingFileWindow()
    {
        close();
    }

    SlidingFileWindow(const SlidingFileWindow&) = delete;
    SlidingFileWindow& operator=(const SlidingFileWindow&) = delete;
    SlidingFileWindow(SlidingFileWindow&&) noexcept = default;
    SlidingFileWindow& operator=(SlidingFileWindow&&) noexcept = default;

    // =============================================================================
    // File Operations
    // =============================================================================

    /**
     * @brief Open a file with sliding window access
     *
     * @param filename Path to file
     * @param element_size Size of each element in bytes
     * @param window_size Number of elements in window (0 = entire file)
     * @param lag_offset Start window at (file_size - lag_offset) position
     * @return Result indicating success or error
     */
    void_result_type
    open(const std::string& filename, size_t element_size, size_t window_size = 5000, size_t lag_offset = 0)
    {
        auto guard = mMutex.lock();

        close_impl();

        if (element_size == 0)
        {
            return ErrorPolicy::report_void_error(FileError::InvalidWindowSize);
        }

        element_size_ = element_size;
        mFile.open(filename, std::ios::in | std::ios::out | std::ios::binary);

        if (!mFile.is_open())
        {
            return ErrorPolicy::report_void_error(FileError::FileNotOpen);
        }

        // Determine file size
        mFile.seekg(0, std::ios::end);
        if (!mFile.good())
        {
            mFile.close();
            return ErrorPolicy::report_void_error(FileError::SeekFailure);
        }

        auto file_bytes = static_cast<size_t>(mFile.tellg());
        file_size_ = file_bytes / element_size_;

        if (file_size_ == 0)
        {
            window_size_ = 0;
            begin_index_ = 0;
            end_index_ = 0;
            return ErrorPolicy::report_void_success();
        }

        // Clamp window size to file size
        window_size_ = std::min(window_size, file_size_);

        // Calculate starting position based on lag offset
        if (lag_offset > 0 && lag_offset < file_size_)
        {
            begin_index_ = file_size_ - lag_offset;
        }
        else
        {
            begin_index_ = 0;
        }

        end_index_ = begin_index_ + window_size_;
        if (end_index_ > file_size_)
        {
            end_index_ = file_size_;
            begin_index_ = file_size_ - window_size_;
        }

        // Load initial window
        mWindow.clear();

        mFile.seekg(begin_index_ * element_size_, std::ios::beg);
        for (size_t i = 0; i < window_size_; ++i)
        {
            ElementType elem;
            if (!SerializationPolicy::read(mFile, elem))
            {
                mFile.close();
                mWindow.clear();
                return ErrorPolicy::report_void_error(FileError::ReadFailure);
            }
            mWindow.push_back(std::move(elem));
        }

        return ErrorPolicy::report_void_success();
    }

    /**
     * @brief Close file and flush changes
     */
    void close()
    {
        auto guard = mMutex.lock();
        close_impl();
    }

    // =============================================================================
    // Element Access
    // =============================================================================

    /**
     * @brief Access element at index
     *
     * For single-threaded mode: returns reference to cached element (zero-copy).
     * For thread-safe mode: returns copy of element (prevents reference-outlives-lock).
     *
     * Thread-safe mode automatically shifts the window if the index is out of range,
     * keeping the lock held throughout the operation.
     *
     * @param index Element index in file
     * @return Result containing element (reference or copy depending on ConcurrencyPolicy)
     */
    result_type operator[](size_t index)
    {
        auto guard = mMutex.lock();

        if (!mFile.is_open())
        {
            if constexpr (is_threadsafe)
            {
                return make_unexpected(FileError::FileNotOpen);
            }
            else
            {
                return ErrorPolicy::report_error(FileError::FileNotOpen);
            }
        }

        if (index >= file_size_)
        {
            if constexpr (is_threadsafe)
            {
                return make_unexpected(FileError::InvalidIndex);
            }
            else
            {
                return ErrorPolicy::report_error(FileError::InvalidIndex);
            }
        }

        // Fast path: entire file in window
        if (window_size_ == file_size_)
        {
            if constexpr (is_threadsafe)
            {
                // Return COPY - lock released after return, reference would be unsafe
                return result_type(mWindow[index]);
            }
            else
            {
                return ErrorPolicy::report_success(std::ref(mWindow[index]));
            }
        }

        // Check if index is in current window
        auto window_index = get_window_index(index);

        if constexpr (is_threadsafe)
        {
            // Thread-safe: auto-shift window if needed (lock held throughout)
            if (!window_index)
            {
                shift_to_index_impl(index);
                window_index = get_window_index(index);
            }

            if (window_index)
            {
                // Return COPY - lock released after return
                return result_type(mWindow[*window_index]);
            }

            return make_unexpected(FileError::CorruptedState);
        }
        else
        {
            // Single-threaded: use fail-safe buffer for out-of-window access
            if (window_index)
            {
                return ErrorPolicy::report_success(std::ref(mWindow[*window_index]));
            }

            // Fail-safe: direct I/O for out-of-window access
            // Write back current element if dirty
            if (current_index_ < file_size_)
            {
                mFile.seekp(current_index_ * element_size_, std::ios::beg);
                if (!SerializationPolicy::write(mFile, current_element_))
                {
                    return ErrorPolicy::report_error(FileError::WriteFailure);
                }
            }

            // Read new element
            current_index_ = index;
            mFile.seekg(current_index_ * element_size_, std::ios::beg);
            if (!SerializationPolicy::read(mFile, current_element_))
            {
                return ErrorPolicy::report_error(FileError::ReadFailure);
            }

            return ErrorPolicy::report_success(std::ref(current_element_));
        }
    }

    /**
     * @brief Const access to element
     *
     * For single-threaded mode: returns const reference (in-window only).
     * For thread-safe mode: returns copy with auto-shift.
     */
    const_result_type operator[](size_t index) const
    {
        auto guard = mMutex.lock_shared();

        if (!mFile.is_open())
        {
            return make_unexpected(FileError::FileNotOpen);
        }

        if (index >= file_size_)
        {
            return make_unexpected(FileError::InvalidIndex);
        }

        if (window_size_ == file_size_)
        {
            if constexpr (is_threadsafe)
            {
                return const_result_type(mWindow[index]);
            }
            else
            {
                return std::cref(mWindow[index]);
            }
        }

        auto window_index = get_window_index(index);
        if (window_index)
        {
            if constexpr (is_threadsafe)
            {
                return const_result_type(mWindow[*window_index]);
            }
            else
            {
                return std::cref(mWindow[*window_index]);
            }
        }

        return make_unexpected(FileError::InvalidIndex);
    }

    // =============================================================================
    // Window Management
    // =============================================================================

    /**
     * @brief Shift window to include specified index
     * @param target_index Index to bring into window
     * @return Result indicating success or error
     */
    void_result_type shift_to_index(size_t target_index)
    {
        auto guard = mMutex.lock();

        if (window_size_ == file_size_ || mWindow.empty())
        {
            return ErrorPolicy::report_void_success();
        }

        if (target_index >= file_size_)
        {
            return ErrorPolicy::report_void_error(FileError::InvalidIndex);
        }

        shift_to_index_impl(target_index);

        return ErrorPolicy::report_void_success();
    }

    // =============================================================================
    // Query Operations
    // =============================================================================

    size_t size() const noexcept
    {
        auto guard = mMutex.lock_shared();
        return file_size_;
    }

    bool empty() const noexcept
    {
        auto guard = mMutex.lock_shared();
        return file_size_ == 0;
    }

    bool is_open() const noexcept
    {
        auto guard = mMutex.lock_shared();
        return mFile.is_open();
    }

    size_t window_size() const noexcept
    {
        auto guard = mMutex.lock_shared();
        return window_size_;
    }

    size_t begin_index() const noexcept
    {
        auto guard = mMutex.lock_shared();
        return begin_index_;
    }

    size_t end_index() const noexcept
    {
        auto guard = mMutex.lock_shared();
        return end_index_;
    }

private:
    // =============================================================================
    // Internal Shift Implementation (caller must hold lock)
    // =============================================================================

    void shift_to_index_impl(size_t target_index) noexcept
    {
        // Check if already in window
        if (target_index >= begin_index_ && target_index < end_index_)
        {
            return;
        }

        // Calculate shift direction and distance
        if (target_index >= end_index_)
        {
            // Shift forward
            size_t shift_count = target_index - end_index_ + 1;
            for (size_t i = 0; i < shift_count && end_index_ < file_size_; ++i)
            {
                shift_forward_one();
            }
        }
        else if (target_index < begin_index_)
        {
            // Shift backward
            size_t shift_count = begin_index_ - target_index;
            for (size_t i = 0; i < shift_count && begin_index_ > 0; ++i)
            {
                shift_backward_one();
            }
        }
    }

    // =============================================================================
    // Internal Helpers
    // =============================================================================

    void close_impl() noexcept
    {
        if (mFile.is_open())
        {
            flush_window();
            mFile.close();

            file_size_ = 0;
            window_size_ = 0;
            begin_index_ = 0;
            end_index_ = 0;
            element_size_ = 0;
            current_index_ = std::numeric_limits<size_t>::max();
            mWindow.clear();
        }
    }

    void flush_window() noexcept
    {
        if (mWindow.empty())
        {
            return;
        }

        size_t write_index = begin_index_;
        for (const auto& elem : mWindow)
        {
            mFile.seekp(write_index * element_size_, std::ios::beg);
            SerializationPolicy::write(mFile, elem);
            ++write_index;
        }

        mFile.flush();
    }

    bool shift_forward_one() noexcept
    {
        if (mWindow.empty() || end_index_ >= file_size_)
        {
            return true;
        }

        // Write front element back to file
        mFile.seekp(begin_index_ * element_size_, std::ios::beg);
        if (!SerializationPolicy::write(mFile, mWindow.front()))
        {
            return false;
        }

        mWindow.pop_front();
        ++begin_index_;

        // Read new element at end
        mFile.seekg(end_index_ * element_size_, std::ios::beg);
        ElementType temp;
        if (!SerializationPolicy::read(mFile, temp))
        {
            return false;
        }

        mWindow.push_back(std::move(temp));
        ++end_index_;

        return true;
    }

    bool shift_backward_one() noexcept
    {
        if (mWindow.empty() || begin_index_ == 0)
        {
            return true;
        }

        // Write back element back to file
        mFile.seekp((end_index_ - 1) * element_size_, std::ios::beg);
        if (!SerializationPolicy::write(mFile, mWindow.back()))
        {
            return false;
        }

        mWindow.pop_back();
        --end_index_;

        // Read new element at front
        --begin_index_;
        mFile.seekg(begin_index_ * element_size_, std::ios::beg);
        ElementType temp;
        if (!SerializationPolicy::read(mFile, temp))
        {
            return false;
        }

        mWindow.push_front(std::move(temp));

        return true;
    }

    std::optional<size_t> get_window_index(size_t file_index) const noexcept
    {
        if (file_index >= begin_index_ && file_index < end_index_)
        {
            return file_index - begin_index_;
        }
        return std::nullopt;
    }

    // =============================================================================
    // Members
    // =============================================================================

    size_t element_size_ = 0;
    size_t file_size_ = 0;
    size_t window_size_ = 0;
    size_t begin_index_ = 0;
    size_t end_index_ = 0;

    mutable std::fstream mFile;
    std::deque<ElementType> mWindow;

    // Fail-safe direct access buffer (single-threaded mode only)
    ElementType current_element_{};
    size_t current_index_ = std::numeric_limits<size_t>::max();

    mutable ConcurrencyPolicy mMutex;
};

// =============================================================================
// Convenience Aliases
// =============================================================================

/**
 * @brief Simple sliding window with custom serialization (default)
 */
template <typename ElementType>
using SimpleSlidingWindow = SlidingFileWindow<ElementType>;

/**
 * @brief Thread-safe sliding window (returns element copies, auto-shifts window)
 *
 * Unlike the single-threaded variant, operator[] returns copies of elements
 * rather than references. This prevents race conditions where concurrent
 * window shifts could invalidate references held by other threads.
 *
 * operator[] also auto-shifts the window if the requested index is out of
 * range, keeping the lock held throughout the entire operation.
 */
template <typename ElementType>
using ThreadSafeSlidingWindow = SlidingFileWindow<ElementType,
                                                  CustomSerializationPolicy<ElementType>,
                                                  ExpectedFileErrorPolicy<ElementType, FileError>,
                                                  MutexSynchronizationPolicy>;

/**
 * @brief Binary POD sliding window (fastest)
 */
template <typename ElementType>
using BinarySlidingWindow = SlidingFileWindow<ElementType, BinarySerializationPolicy<ElementType>>;

} // namespace fat_p

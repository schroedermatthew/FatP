/**
 * @file SlidingFileWindow.h
 * @brief Policy-based sliding window access to large binary files with on-demand paging.
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
        , error_(error)
    {
    }

    FileError error() const noexcept { return error_; }

private:
    FileError error_;

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
template<typename ElementType>
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

    static constexpr size_t element_size() noexcept { return sizeof(ElementType); }
};

/**
 * @brief Stream-based serialization using operator<< and operator>>
 * @tparam ElementType Type of elements (must support stream operators)
 */
template<typename ElementType>
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

    static constexpr size_t element_size() noexcept { return sizeof(ElementType); }
};

/**
 * @brief Custom serialization using ElementType::Read() and ElementType::Write()
 * @tparam ElementType Type with Read(std::istream&) and Write(std::ostream&) methods
 */
template<typename ElementType>
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

    static constexpr size_t element_size() noexcept { return sizeof(ElementType); }
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
template<typename T, typename Error = FileError>
class ExpectedFileErrorPolicy
{
private:
    template<typename U>
    struct wrap_reference
    {
        using type = U;
    };

    template<typename U>
    struct wrap_reference<U&>
    {
        using type = std::reference_wrapper<U>;
    };

public:
    using wrapped_type = typename wrap_reference<T>::type;
    using result_type = Expected<wrapped_type, Error>;
    using void_result_type = Expected<void, Error>;

    static result_type report_error(Error error) noexcept { return make_unexpected(error); }

    template<typename U>
    static result_type report_success(U&& value) noexcept
    {
        return result_type(std::forward<U>(value));
    }

    static void_result_type report_void_error(Error error) noexcept
    {
        return make_unexpected(error);
    }

    static void_result_type report_void_success() noexcept { return void_result_type{}; }
};

/**
 * @brief Report errors by throwing FileWindowException
 */
template<typename T>
class ThrowingFileErrorPolicy
{
public:
    using result_type = T;
    using void_result_type = void;

    [[noreturn]] static result_type report_error(FileError error)
    {
        throw FileWindowException(error);
    }

    template<typename U>
    static result_type report_success(U&& value) noexcept
    {
        return std::forward<U>(value);
    }

    [[noreturn]] static void_result_type report_void_error(FileError error)
    {
        throw FileWindowException(error);
    }

    static void_result_type report_void_success() noexcept {}
};

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
 */
template<typename ElementType,
         typename SerializationPolicy = CustomSerializationPolicy<ElementType>,
         typename ErrorPolicy = ExpectedFileErrorPolicy<ElementType&, FileError>,
         typename ConcurrencyPolicy = SingleThreadedPolicy>
class SlidingFileWindow : private SerializationPolicy
{
public:
    using element_type = ElementType;
    using result_type = typename ErrorPolicy::result_type;
    using void_result_type = typename ErrorPolicy::void_result_type;

    // =============================================================================
    // Construction
    // =============================================================================

    SlidingFileWindow() = default;

    ~SlidingFileWindow() { close(); }

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
    void_result_type open(const std::string& filename,
                          size_t element_size,
                          size_t window_size = 5000,
                          size_t lag_offset = 0)
    {
        auto guard = mutex_.lock();

        close_impl();

        if (element_size == 0)
        {
            return ErrorPolicy::report_void_error(FileError::InvalidWindowSize);
        }

        element_size_ = element_size;
        file_.open(filename, std::ios::in | std::ios::out | std::ios::binary);

        if (!file_.is_open())
        {
            return ErrorPolicy::report_void_error(FileError::FileNotOpen);
        }

        // Determine file size
        file_.seekg(0, std::ios::end);
        if (!file_.good())
        {
            file_.close();
            return ErrorPolicy::report_void_error(FileError::SeekFailure);
        }

        auto file_bytes = static_cast<size_t>(file_.tellg());
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
        window_.clear();

        file_.seekg(begin_index_ * element_size_, std::ios::beg);
        for (size_t i = 0; i < window_size_; ++i)
        {
            ElementType elem;
            if (!SerializationPolicy::read(file_, elem))
            {
                file_.close();
                window_.clear();
                return ErrorPolicy::report_void_error(FileError::ReadFailure);
            }
            window_.push_back(std::move(elem));
        }

        return ErrorPolicy::report_void_success();
    }

    /**
     * @brief Close file and flush changes
     */
    void close()
    {
        auto guard = mutex_.lock();
        close_impl();
    }

    // =============================================================================
    // Element Access
    // =============================================================================

    /**
     * @brief Access element at index (may trigger I/O for out-of-window)
     * 
     * For in-window elements, returns reference to cached element.
     * For out-of-window elements, loads element into fail-safe buffer.
     * 
     * @param index Element index in file
     * @return Result containing element reference or error
     */
    result_type operator[](size_t index)
    {
        auto guard = mutex_.lock();

        if (!file_.is_open())
        {
            return ErrorPolicy::report_error(FileError::FileNotOpen);
        }

        if (index >= file_size_)
        {
            return ErrorPolicy::report_error(FileError::InvalidIndex);
        }

        // Fast path: entire file in window
        if (window_size_ == file_size_)
        {
            return ErrorPolicy::report_success(std::ref(window_[index]));
        }

        // Check if index is in current window
        auto window_index = get_window_index(index);
        if (window_index)
        {
            return ErrorPolicy::report_success(std::ref(window_[*window_index]));
        }

        // Fail-safe: direct I/O for out-of-window access
        // Write back current element if dirty
        if (current_index_ < file_size_)
        {
            file_.seekp(current_index_ * element_size_, std::ios::beg);
            if (!SerializationPolicy::write(file_, current_element_))
            {
                return ErrorPolicy::report_error(FileError::WriteFailure);
            }
        }

        // Read new element
        current_index_ = index;
        file_.seekg(current_index_ * element_size_, std::ios::beg);
        if (!SerializationPolicy::read(file_, current_element_))
        {
            return ErrorPolicy::report_error(FileError::ReadFailure);
        }

        return ErrorPolicy::report_success(std::ref(current_element_));
    }

    /**
     * @brief Const access to element (in-window only)
     */
    Expected<std::reference_wrapper<const ElementType>, FileError>
    operator[](size_t index) const
    {
        auto guard = mutex_.lock_shared();

        if (!file_.is_open())
        {
            return make_unexpected(FileError::FileNotOpen);
        }

        if (index >= file_size_)
        {
            return make_unexpected(FileError::InvalidIndex);
        }

        if (window_size_ == file_size_)
        {
            return std::cref(window_[index]);
        }

        auto window_index = get_window_index(index);
        if (window_index)
        {
            return std::cref(window_[*window_index]);
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
        auto guard = mutex_.lock();

        if (window_size_ == file_size_ || window_.empty())
        {
            return ErrorPolicy::report_void_success();
        }

        if (target_index >= file_size_)
        {
            return ErrorPolicy::report_void_error(FileError::InvalidIndex);
        }

        // Check if already in window
        if (target_index >= begin_index_ && target_index < end_index_)
        {
            return ErrorPolicy::report_void_success();
        }

        // Calculate shift direction and distance
        if (target_index >= end_index_)
        {
            // Shift forward
            size_t shift_count = target_index - end_index_ + 1;
            for (size_t i = 0; i < shift_count && end_index_ < file_size_; ++i)
            {
                if (!shift_forward_one())
                {
                    return ErrorPolicy::report_void_error(FileError::WriteFailure);
                }
            }
        }
        else if (target_index < begin_index_)
        {
            // Shift backward
            size_t shift_count = begin_index_ - target_index;
            for (size_t i = 0; i < shift_count && begin_index_ > 0; ++i)
            {
                if (!shift_backward_one())
                {
                    return ErrorPolicy::report_void_error(FileError::WriteFailure);
                }
            }
        }

        return ErrorPolicy::report_void_success();
    }

    // =============================================================================
    // Query Operations
    // =============================================================================

    size_t size() const noexcept
    {
        auto guard = mutex_.lock_shared();
        return file_size_;
    }

    bool empty() const noexcept
    {
        auto guard = mutex_.lock_shared();
        return file_size_ == 0;
    }

    bool is_open() const noexcept
    {
        auto guard = mutex_.lock_shared();
        return file_.is_open();
    }

    size_t window_size() const noexcept
    {
        auto guard = mutex_.lock_shared();
        return window_size_;
    }

    size_t begin_index() const noexcept
    {
        auto guard = mutex_.lock_shared();
        return begin_index_;
    }

    size_t end_index() const noexcept
    {
        auto guard = mutex_.lock_shared();
        return end_index_;
    }

private:
    // =============================================================================
    // Internal Helpers
    // =============================================================================

    void close_impl() noexcept
    {
        if (file_.is_open())
        {
            flush_window();
            file_.close();

            file_size_ = 0;
            window_size_ = 0;
            begin_index_ = 0;
            end_index_ = 0;
            element_size_ = 0;
            current_index_ = std::numeric_limits<size_t>::max();
            window_.clear();
        }
    }

    void flush_window() noexcept
    {
        if (window_.empty())
        {
            return;
        }

        size_t write_index = begin_index_;
        for (const auto& elem : window_)
        {
            file_.seekp(write_index * element_size_, std::ios::beg);
            SerializationPolicy::write(file_, elem);
            ++write_index;
        }

        file_.flush();
    }

    bool shift_forward_one() noexcept
    {
        if (window_.empty() || end_index_ >= file_size_)
        {
            return true;
        }

        // Write front element back to file
        file_.seekp(begin_index_ * element_size_, std::ios::beg);
        if (!SerializationPolicy::write(file_, window_.front()))
        {
            return false;
        }

        window_.pop_front();
        ++begin_index_;

        // Read new element at end
        file_.seekg(end_index_ * element_size_, std::ios::beg);
        ElementType temp;
        if (!SerializationPolicy::read(file_, temp))
        {
            return false;
        }

        window_.push_back(std::move(temp));
        ++end_index_;

        return true;
    }

    bool shift_backward_one() noexcept
    {
        if (window_.empty() || begin_index_ == 0)
        {
            return true;
        }

        // Write back element back to file
        file_.seekp((end_index_ - 1) * element_size_, std::ios::beg);
        if (!SerializationPolicy::write(file_, window_.back()))
        {
            return false;
        }

        window_.pop_back();
        --end_index_;

        // Read new element at front
        --begin_index_;
        file_.seekg(begin_index_ * element_size_, std::ios::beg);
        ElementType temp;
        if (!SerializationPolicy::read(file_, temp))
        {
            return false;
        }

        window_.push_front(std::move(temp));

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

    mutable std::fstream file_;
    std::deque<ElementType> window_;

    // Fail-safe direct access buffer
    ElementType current_element_{};
    size_t current_index_ = std::numeric_limits<size_t>::max();

    mutable ConcurrencyPolicy mutex_;
};

// =============================================================================
// Convenience Aliases
// =============================================================================

/**
 * @brief Simple sliding window with custom serialization (default)
 */
template<typename ElementType>
using SimpleSlidingWindow = SlidingFileWindow<ElementType>;

/**
 * @brief Thread-safe sliding window
 */
template<typename ElementType>
using ThreadSafeSlidingWindow =
    SlidingFileWindow<ElementType,
                      CustomSerializationPolicy<ElementType>,
                      ExpectedFileErrorPolicy<ElementType&, FileError>,
                      MutexSynchronizationPolicy>;

/**
 * @brief Binary POD sliding window (fastest)
 */
template<typename ElementType>
using BinarySlidingWindow =
    SlidingFileWindow<ElementType, BinarySerializationPolicy<ElementType>>;

} // namespace fat_p

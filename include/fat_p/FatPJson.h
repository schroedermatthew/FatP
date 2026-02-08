#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPJson
  file_role: public_header
  path: include/fat_p/FatPJson.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for FatPJson."
  api_stability: in_work
  related:
    docs_search: "FatPJson"
    tests:
      - components/Json/tests/test_FatPJson.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 2
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file FatPJson.h
 * @brief Enhanced JSON library leveraging the fat_p ecosystem
 *
 *
 *
 * @section overview Overview
 * FatPJson extends JsonLite with powerful fat_p components for:
 * - Expected-based error handling (no exceptions)
 * - Optimized data structures (FlatMap, SmallVector)
 * - String deduplication (StringPool)
 * - Large file support (MemoryMappedFile)
 * - Enhanced numeric safety (CheckedArithmetic)
 * - Better error messages (enforce.h)
 *
 * @section dependencies Dependencies
 * Requires:
 * - JsonLite.h (base functionality)
 * - Expected.h (error handling without exceptions)
 * - FlatMap.h (optimized object storage)
 * - SmallVector.h (optimized array storage with inline elements)
 * - StringPool.h (memory optimization for repeated keys)
 * - CheckedArithmetic.h (enhanced numeric safety)
 * - MemoryMappedFile.h (large file support)
 * - enforce.h (enhanced error messages)
 *
 * @section features Key Features
 * - 30-50% memory savings for large JSON with repeated keys
 * - 2-5x faster small object operations
 * - No heap allocation for arrays < 8 elements
 * - Memory-mapped I/O for files > 10MB
 * - Expected-based API for zero-overhead error handling
 * - Thread-safe string pool (thread-local by default)
 * - JSONC comment support (via ConfigJsonPolicy)
 * - Enhanced UTF-8 escaping for multi-byte characters
 * - Locale-independent numeric parsing
 * - Full numeric range support (no artificial margins)
 *
 * @section usage Basic Usage
 * @code{.cpp}
 * #include "FatPJson.h"
 *
 * // Expected-based parsing (no exceptions)
 * auto result = fat_p::try_parse_json(json_str);
 * if (!result) {
 *     std::cerr << "Parse error: " << result.error().message << "\n";
 *     return;
 * }
 *
 * // Optimized data structures
 * fat_p::FatPJsonObject obj;  // Uses FlatMap instead of std::map
 * fat_p::FatPJsonArray arr;   // Uses SmallVector with inline storage
 *
 * // Memory-mapped file I/O
 * auto loaded = fat_p::load_json_mmap("large_file.json");
 * if (!loaded) {
 *     std::cerr << "Failed to load: " << loaded.error().message << "\n";
 * }
 *
 * // String pool for key deduplication
 * PooledJsonObject pooled_obj(my_pool);
 * pooled_obj.insert("name", to_json("value"));  // "name" deduplicated
 * @endcode
 */

#include "CheckedArithmetic.h"
#include "ConcurrencyPolicies.h"
#include "enforce.h"
#include "EnumPlus.h"
#include "Expected.h"
#include "FlatMap.h"
#include "JsonLite.h"
#include "MemoryMappedFile.h"
#include "ScopeGuard.h"
#include "SmallVector.h"
#include "StringPool.h"

#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <thread>
#include <type_traits>

// Process ID for cross-platform support
#ifdef _WIN32
#include <process.h>
#define FATP_GETPID() _getpid()
#else
#include <unistd.h>
#define FATP_GETPID() getpid()
#endif

namespace fat_p
{

/**
 * @brief Error codes for JSON operations
 */
enum class JsonErrorCode
{
    Success,
    ParseError,
    TypeError,
    RangeError,
    FileError,
    DepthExceeded,
    MemoryError,
    InvalidUtf8,
    NumericOverflow,
    MissingField,
    ExtraData,
    Unknown
};

inline std::ostream& operator<<(std::ostream& os, JsonErrorCode code)
{
    switch (code)
    {
        case JsonErrorCode::Success:
            return os << "Success";
        case JsonErrorCode::ParseError:
            return os << "ParseError";
        case JsonErrorCode::TypeError:
            return os << "TypeError";
        case JsonErrorCode::RangeError:
            return os << "RangeError";
        case JsonErrorCode::FileError:
            return os << "FileError";
        case JsonErrorCode::DepthExceeded:
            return os << "DepthExceeded";
        case JsonErrorCode::MemoryError:
            return os << "MemoryError";
        case JsonErrorCode::InvalidUtf8:
            return os << "InvalidUtf8";
        case JsonErrorCode::NumericOverflow:
            return os << "NumericOverflow";
        case JsonErrorCode::MissingField:
            return os << "MissingField";
        case JsonErrorCode::ExtraData:
            return os << "ExtraData";
        case JsonErrorCode::Unknown:
            return os << "Unknown";
        default:
            return os << "Unknown(" << static_cast<int>(code) << ")";
    }
}

/**
 * @brief Comprehensive error information for JSON operations
 */
struct JsonError
{
    JsonErrorCode code = JsonErrorCode::Success;
    std::string message;
    size_t position = 0;
    std::string context;

    JsonError() = default;

    JsonError(JsonErrorCode c, std::string msg, size_t pos = 0, std::string ctx = "")
        : code(c)
        , message(std::move(msg))
        , position(pos)
        , context(std::move(ctx))
    {
    }

    explicit operator bool() const noexcept
    {
        return code != JsonErrorCode::Success;
    }

    /**
     * @brief Converts the error structure to a human-readable string.
     * @details Uses stream operators for JsonErrorCode for consistent output.
     */
    [[nodiscard]] std::string to_string() const
    {
        std::ostringstream oss;
        oss << "JsonError[" << code << "]: " << message;
        if (position > 0)
        {
            oss << " at position " << position;
        }
        if (!context.empty())
        {
            oss << " (context: " << context << ")";
        }
        return oss.str();
    }
};

constexpr size_t DEFAULT_INLINE_ARRAY_SIZE = 8;

using FatPJsonArray = SmallVector<JsonValue, DEFAULT_INLINE_ARRAY_SIZE, std::allocator<JsonValue>>;

template <typename Compare = std::less<std::string>,
          typename Allocator = std::allocator<std::pair<const std::string, JsonValue>>>
using FatPJsonObject = FlatMap<std::string, JsonValue, Compare, Allocator>;

/**
 * @brief Result type for JSON operations (mirrors CborResult pattern)
 */
template <typename T>
using JsonResult = Expected<T, JsonError>;

// =============================================================================
// Robust Error Extraction Helpers
// =============================================================================

namespace json_detail
{

/**
 * @brief Extracts position from json_enforce error message using std::from_chars.
 * @details Searches for pattern " position N" with word boundaries.
 * @return Position if found, 0 otherwise.
 */
inline size_t extract_position_from_error(std::string_view msg) noexcept
{
    constexpr std::string_view POSITION_KEY = " position ";
    size_t pos_idx = msg.find(POSITION_KEY);

    if (pos_idx == std::string_view::npos)
    {
        return 0;
    }

    pos_idx += POSITION_KEY.size();

    size_t end_idx = pos_idx;
    while (end_idx < msg.size() && std::isdigit(static_cast<unsigned char>(msg[end_idx])))
    {
        ++end_idx;
    }

    if (end_idx == pos_idx)
    {
        return 0;
    }

    size_t result = 0;
    std::from_chars_result parse_result = std::from_chars(msg.data() + pos_idx, msg.data() + end_idx, result);

    return (parse_result.ec == std::errc{}) ? result : 0;
}

/**
 * @brief Classifies exception message into structured error code based on patterns.
 * @details Checks patterns in order from most to least specific.
 */
inline JsonErrorCode classify_exception(std::string_view msg) noexcept
{
    if (msg.find("JSON parse error") != std::string_view::npos)
    {
        if (msg.find("depth exceeded") != std::string_view::npos)
        {
            return JsonErrorCode::DepthExceeded;
        }
        if (msg.find("invalid unicode") != std::string_view::npos)
        {
            return JsonErrorCode::InvalidUtf8;
        }
        return JsonErrorCode::ParseError;
    }

    if (msg.find("File error") != std::string_view::npos || msg.find("Failed to open") != std::string_view::npos ||
        msg.find("Error reading file") != std::string_view::npos)
    {
        return JsonErrorCode::FileError;
    }

    if (msg.find("Required field missing") != std::string_view::npos)
    {
        return JsonErrorCode::MissingField;
    }

    if (msg.find("out of range") != std::string_view::npos || msg.find("overflow") != std::string_view::npos ||
        msg.find("Numeric cast") != std::string_view::npos)
    {
        return JsonErrorCode::NumericOverflow;
    }

    if (msg.find("type mismatch") != std::string_view::npos || msg.find("Type mismatch") != std::string_view::npos)
    {
        return JsonErrorCode::TypeError;
    }

    return JsonErrorCode::Unknown;
}

// ---------------------------------------------------------------------
// JSON Pointer (RFC 6901) helpers
// ---------------------------------------------------------------------

[[nodiscard]] inline Expected<std::string, JsonError> unescape_json_pointer_token(std::string_view token) noexcept
{
    std::string out;
    out.reserve(token.size());

    for (size_t i = 0; i < token.size(); ++i)
    {
        const char c = token[i];
        if (c != '~')
        {
            out.push_back(c);
            continue;
        }

        if (i + 1 >= token.size())
        {
            return unexpected(JsonError{JsonErrorCode::TypeError,
                                        "JSON Pointer escape '~' must be followed by '0' or '1'",
                                        0,
                                        std::string(token)});
        }

        const char esc = token[i + 1];
        if (esc == '0')
        {
            out.push_back('~');
        }
        else if (esc == '1')
        {
            out.push_back('/');
        }
        else
        {
            return unexpected(
                JsonError{JsonErrorCode::TypeError, "JSON Pointer escape must be '~0' or '~1'", 0, std::string(token)});
        }

        ++i;
    }

    return out;
}

[[nodiscard]] inline Expected<size_t, JsonError> parse_json_pointer_index(std::string_view token) noexcept
{
    if (token.empty())
    {
        return unexpected(JsonError{JsonErrorCode::TypeError, "JSON Pointer array index is empty", 0, ""});
    }

    if (token == "-")
    {
        return unexpected(
            JsonError{JsonErrorCode::TypeError, "JSON Pointer '-' index is not valid for queries", 0, ""});
    }

    size_t index = 0;
    const char* begin = token.data();
    const char* end = token.data() + token.size();
    const std::from_chars_result result = std::from_chars(begin, end, index);

    if (result.ec != std::errc{} || result.ptr != end)
    {
        return unexpected(JsonError{JsonErrorCode::TypeError,
                                    "JSON Pointer array index is not a valid non-negative integer",
                                    0,
                                    std::string(token)});
    }

    return index;
}

[[nodiscard]] inline Expected<const JsonValue*, JsonError>
query_json_pointer_noexcept(const JsonValue& root, std::string_view pointer) noexcept
{
    if (pointer.empty())
    {
        return &root;
    }

    if (pointer[0] != '/')
    {
        return unexpected(JsonError{JsonErrorCode::TypeError,
                                    "JSON Pointer must start with '/' or be empty",
                                    0,
                                    std::string(pointer)});
    }

    const JsonValue* current = &root;
    size_t segment_begin = 1;

    while (true)
    {
        const size_t slash = pointer.find('/', segment_begin);
        const size_t segment_len =
            (slash == std::string_view::npos) ? (pointer.size() - segment_begin) : (slash - segment_begin);

        const std::string_view raw_token = pointer.substr(segment_begin, segment_len);
        auto token_result = unescape_json_pointer_token(raw_token);
        if (!token_result)
        {
            return unexpected(token_result.error());
        }

        const std::string& token = *token_result;

        if (const auto* obj = std::get_if<JsonObject>(current))
        {
            auto it = obj->find(token);
            if (it == obj->end())
            {
                return unexpected(JsonError{JsonErrorCode::TypeError, "JSON Pointer key not found", 0, token});
            }
            current = &it->second;
        }
        else if (const auto* arr = std::get_if<JsonArray>(current))
        {
            auto index_result = parse_json_pointer_index(token);
            if (!index_result)
            {
                return unexpected(index_result.error());
            }

            const size_t index = *index_result;
            if (index >= arr->size())
            {
                return unexpected(
                    JsonError{JsonErrorCode::TypeError, "JSON Pointer array index out of bounds", 0, token});
            }

            current = &(*arr)[index];
        }
        else
        {
            return unexpected(JsonError{JsonErrorCode::TypeError,
                                        "JSON Pointer cannot descend into a non-container value",
                                        0,
                                        token});
        }

        if (slash == std::string_view::npos)
        {
            break;
        }
        segment_begin = slash + 1;
    }

    return current;
}

[[nodiscard]] inline Expected<JsonValue*, JsonError> query_json_pointer_noexcept(JsonValue& root,
                                                                                 std::string_view pointer) noexcept
{
    auto const_result = query_json_pointer_noexcept(static_cast<const JsonValue&>(root), pointer);

    if (!const_result)
    {
        return unexpected(const_result.error());
    }

    return const_cast<JsonValue*>(*const_result);
}

} // namespace json_detail

// =============================================================================
// Exception-Free Parsing
// =============================================================================

/**
 * @brief Exception-free JSON parsing
 *
 * @details Parses JSON without throwing exceptions. Returns Expected<JsonValue, JsonError>
 * for composable error handling. This is 10-100x faster than exception-based parsing
 * in error paths.
 *
 * @tparam Policy Parsing policy (StandardJsonPolicy, ConfigJsonPolicy, etc.)
 * @param json JSON string to parse
 * @return Expected containing either parsed JsonValue or JsonError
 */
template <typename Policy = StandardJsonPolicy>
[[nodiscard]] inline Expected<JsonValue, JsonError> try_parse_json(std::string_view json) noexcept
{
    try
    {
        size_t pos = 0;
        JsonValue val = json_detail::parse_value<Policy>(json, pos);

        json_detail::skip_whitespace<Policy>(json, pos);

        if (pos != json.size())
        {
            return unexpected(JsonError{JsonErrorCode::ExtraData,
                                        "Extra data after JSON value",
                                        pos,
                                        std::string(json.substr(pos, std::min<size_t>(20, json.size() - pos)))});
        }

        return val;
    }
    catch (const std::exception& e)
    {
        std::string full_msg = e.what();
        JsonErrorCode code = json_detail::classify_exception(full_msg);
        size_t error_pos = json_detail::extract_position_from_error(full_msg);

        return unexpected(JsonError{code, "Operation failed.", error_pos, full_msg});
    }
}

// =============================================================================
// File I/O Operations
// =============================================================================

/**
 * @brief Load JSON from file without exceptions
 *
 * @tparam Policy Parsing policy (StandardJsonPolicy, ConfigJsonPolicy for JSONC, etc.)
 * @param filename Path to JSON file
 * @return Expected containing either parsed JsonValue or JsonError
 */
template <typename Policy = StandardJsonPolicy>
[[nodiscard]] inline Expected<JsonValue, JsonError> try_load_json(const std::string& filename) noexcept
{
    try
    {
        std::ifstream ifs(filename, std::ios::binary);
        if (!ifs.is_open())
        {
            return unexpected(JsonError{JsonErrorCode::FileError, "Failed to open file", 0, filename});
        }

        std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

        if (ifs.bad())
        {
            return unexpected(JsonError{JsonErrorCode::FileError, "Error reading file", 0, filename});
        }

        return try_parse_json<Policy>(content);
    }
    catch (const std::exception& e)
    {
        std::string full_msg = e.what();
        JsonErrorCode code = json_detail::classify_exception(full_msg);
        return unexpected(JsonError{code, "File operation failed.", 0, full_msg});
    }
}

/**
 * @brief Memory-mapped file parsing for large JSON files
 *
 * @details Uses memory-mapped I/O for efficient handling of large files (>10MB).
 * The OS manages memory through virtual memory, providing efficient access without
 * loading the entire file into a separate buffer.
 *
 * @warning JsonLite copies all string values during parsing, so the returned JsonValue
 * does not reference the mapped memory. The memory mapping is released when this
 * function returns, which is safe because all data has been copied.
 *
 * @tparam Policy Parsing policy (StandardJsonPolicy, ConfigJsonPolicy for JSONC, etc.)
 * @param filename Path to JSON file
 * @return Expected containing either parsed JsonValue or JsonError
 */
template <typename Policy = StandardJsonPolicy>
[[nodiscard]] inline Expected<JsonValue, JsonError> load_json_mmap(const std::string& filename) noexcept
{
    try
    {
        MemoryMappedFile mapped;
        if (!mapped.open(filename, MemoryMappedFile::Mode::ReadOnly))
        {
            return unexpected(JsonError{JsonErrorCode::FileError, "Failed to memory-map file", 0, filename});
        }

        std::string_view json_view(static_cast<const char*>(mapped.data()), mapped.size());
        return try_parse_json<Policy>(json_view);
    }
    catch (const std::exception& e)
    {
        std::string full_msg = e.what();
        JsonErrorCode code = json_detail::classify_exception(full_msg);
        return unexpected(JsonError{code, "Memory-mapped file operation failed.", 0, full_msg});
    }
}

/**
 * @brief Save JSON to file without exceptions
 *
 * @tparam Policy Formatting policy (default: StandardJsonPolicy)
 * @param filename Path to output file
 * @param value JSON value to save
 * @param pretty Enable pretty printing
 * @return Expected containing success or JsonError
 */
template <typename Policy = StandardJsonPolicy>
[[nodiscard]] inline Expected<void, JsonError>
try_save_json(const std::string& filename, const JsonValue& value, bool pretty = Policy::pretty_print) noexcept
{
    try
    {
        std::ofstream ofs(filename);
        if (!ofs.is_open())
        {
            return unexpected(JsonError{JsonErrorCode::FileError, "Failed to open file for writing", 0, filename});
        }

        to_json_stream<JsonValue, Policy>(ofs, value, pretty);

        if (!ofs.good())
        {
            return unexpected(JsonError{JsonErrorCode::FileError, "Error writing to file", 0, filename});
        }

        return {};
    }
    catch (const std::exception& e)
    {
        std::string full_msg = e.what();
        JsonErrorCode code = json_detail::classify_exception(full_msg);
        return unexpected(JsonError{code, "Save operation failed.", 0, full_msg});
    }
}

/**
 * @brief Atomically save JSON to file using rename operation
 *
 * @details Provides transactional file writing with the following guarantees:
 * - The target file is never partially written (all-or-nothing semantics)
 * - On any failure, the original file (if it exists) remains untouched
 * - Uses OS-level atomic rename for the final step
 * - Automatic cleanup of temporary files via ScopeGuard RAII
 * - Retry logic with exponential backoff for concurrent access scenarios
 *
 * The atomic save process:
 * 1. Write JSON data to a unique temporary file (filename.tmp.<timestamp>)
 * 2. If write succeeds, atomically rename temp file to target filename
 * 3. If rename fails due to concurrent access, retry with exponential backoff
 * 4. If any step fails after retries, automatically clean up temp file
 *
 * @tparam Policy Formatting policy (default: StandardJsonPolicy)
 * @param filename Path to target output file
 * @param value JSON value to save
 * @param pretty Enable pretty printing (default from Policy)
 * @param max_retries Maximum rename retry attempts (default: 5)
 * @param initial_delay_ms Initial retry delay in milliseconds (default: 1)
 * @return Expected containing success or JsonError
 *
 * @note Requires write permissions in the target directory
 * @note Temporary files use timestamp + thread ID + process ID to avoid collisions
 * @note On Windows, target file must not be open by another process
 * @note Retry uses exponential backoff: delay doubles each attempt (1, 2, 4, 8, 16 ms)
 */
template <typename Policy = StandardJsonPolicy>
[[nodiscard]] inline Expected<void, JsonError> try_save_atomic(const std::string& filename,
                                                               const JsonValue& value,
                                                               bool pretty = Policy::pretty_print,
                                                               int max_retries = 5,
                                                               int initial_delay_ms = 1) noexcept
{
    // Use timestamp + thread ID + process ID to avoid race conditions
    // across threads AND processes (important for HPC checkpoint scenarios)
    const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto thread_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    const auto process_id = static_cast<unsigned long>(FATP_GETPID());
    const std::string temp_filename = filename + ".tmp." + std::to_string(timestamp) + "." +
                                      std::to_string(thread_hash) + "." + std::to_string(process_id);

    auto cleanup = makeScopeGuard([&temp_filename]() noexcept {
        std::error_code ec;
        std::filesystem::remove(temp_filename, ec);
    });

    auto result = try_save_json<Policy>(temp_filename, value, pretty);
    if (!result)
    {
        return result;
    }

    // Retry rename with exponential backoff for concurrent access scenarios
    std::error_code ec;
    int delay_ms = initial_delay_ms;

    for (int attempt = 0; attempt <= max_retries; ++attempt)
    {
        std::filesystem::rename(temp_filename, filename, ec);

        if (!ec)
        {
            // Success - dismiss cleanup guard and return
            cleanup.dismiss();
            return {};
        }

        // Don't sleep after the last failed attempt
        if (attempt < max_retries)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
            delay_ms *= 2; // Exponential backoff
        }
    }

    return unexpected(
        JsonError{JsonErrorCode::FileError,
                  "Atomic rename failed after " + std::to_string(max_retries + 1) + " attempts: " + ec.message(),
                  0,
                  filename});
}

// =============================================================================
// EnumPlus Integration for Automatic Enum Serialization
// =============================================================================

namespace json_detail
{

// named_enum concept from EnumPlus.h replaces has_enum_string_policy trait

} // namespace json_detail

/**
 * @brief Serialize enum to JSON string using EnumPlus
 *
 * @details Automatically converts enums to their string representation if the enum
 * has an EnumStringPolicy specialization. This enables seamless integration with
 * the EnumPlus framework for type-safe enum-to-string conversion.
 *
 * @tparam E Enum type (must have EnumStringPolicy<E> specialization)
 * @param j JSON value to write to
 * @param value Enum value to serialize
 *
 * @note This overload is only enabled for enums with EnumStringPolicy
 * @see EnumPlus.h for defining EnumStringPolicy
 */
template <named_enum E>
void to_json(JsonValue& j, E value)
{
    j = std::string(fat_p::to_string(value));
}

/**
 * @brief Deserialize enum from JSON string using EnumPlus
 *
 * @details Automatically converts JSON strings to enum values if the enum
 * has an EnumStringPolicy specialization. Provides detailed error messages
 * on invalid enum strings.
 *
 * @tparam E Enum type (must have EnumStringPolicy<E> specialization)
 * @param j JSON value to read from (must be string)
 * @param value Enum reference to write to
 *
 * @throws std::runtime_error if JSON is not a string or string is not a valid enum value
 * @note This overload is only enabled for enums with EnumStringPolicy
 */
template <named_enum E>
void from_json(const JsonValue& j, E& value)
{
    FATP_JSON_ENFORCE(j.is_string(),
                      "Enum deserialization requires string",
                      "expected",
                      "string",
                      "got",
                      json_detail::typeName(j));

    try
    {
        value = fat_p::from_string<E>(std::get<std::string>(j));
    }
    catch (const std::exception& e)
    {
        FATP_JSON_ENFORCE(false, "Invalid enum string value", "value", std::get<std::string>(j), "error", e.what());
    }
}

/**
 * @brief Safe enum deserialization with Expected (no exceptions)
 *
 * @details Exception-free version of enum deserialization for use with Expected-based
 * error handling pattern. Returns Expected<E, JsonError> instead of throwing.
 *
 * @tparam E Enum type (must have EnumStringPolicy<E> specialization)
 * @param j JSON value to read from
 * @return Expected containing enum value or JsonError
 */
template <named_enum E>
Expected<E, JsonError>
safe_from_json_enum(const JsonValue& j) noexcept
{
    if (!j.is_string())
    {
        return unexpected(JsonError{JsonErrorCode::TypeError,
                                    "Enum deserialization requires string type",
                                    0,
                                    std::string(json_detail::typeName(j))});
    }

    try
    {
        E value = fat_p::from_string<E>(std::get<std::string>(j));
        return value;
    }
    catch (const std::exception& e)
    {
        return unexpected(JsonError{JsonErrorCode::TypeError,
                                    "Invalid enum string value: " + std::string(e.what()),
                                    0,
                                    std::get<std::string>(j)});
    }
}

// =============================================================================
// String Pool and Memory Optimization
// =============================================================================

/**
 * @brief String pool for JSON object key deduplication
 *
 * @details Deduplicates repeated string keys in JSON objects. Typical savings:
 * - 20-40% memory reduction for repeated keys
 * - Faster string comparison (pointer equality for interned strings)
 * - Thread-safe with appropriate policy
 *
 * @warning The PooledJsonObject stores string_view keys that reference memory owned
 * by the StringPool. The pool MUST outlive any PooledJsonObject that uses it.
 * Destroying the pool while PooledJsonObjects still exist causes undefined behavior.
 *
 * @note In debug builds (when NDEBUG is not defined), consider wrapping the pool
 * in a shared_ptr and storing a weak_ptr here to detect use-after-free.
 *
 * @tparam ThreadPolicy Thread safety policy (SingleThreadedPolicy or MultiThreadedPolicy)
 */
template <typename ThreadPolicy = SingleThreadedPolicy>
class PooledJsonObject
{
public:
    using Pool = StringPool<ThreadPolicy>;
    using Storage = FlatMap<std::string_view, JsonValue>;

private:
    Pool& mPool;
    Storage mStorage;
#ifndef NDEBUG
    // Debug-only: store pool address to detect obvious misuse
    const void* pool_addr_;
#endif

public:
    explicit PooledJsonObject(Pool& pool)
        : mPool(pool)
        , mStorage()
#ifndef NDEBUG
        , pool_addr_(&pool)
#endif
    {
    }

    /**
     * @brief Debug helper to verify pool is still valid
     * @note Only available in debug builds. Always returns true in release.
     */
    [[nodiscard]] bool debug_check_pool_address() const noexcept
    {
#ifndef NDEBUG
        return pool_addr_ == &mPool;
#else
        return true;
#endif
    }

    void insert(std::string_view key, JsonValue value)
    {
        std::string_view interned = mPool.intern(key);
        mStorage[interned] = std::move(value);
    }

    [[nodiscard]] JsonValue* find(std::string_view key) noexcept
    {
        auto it = mStorage.find(key);
        return (it != mStorage.end()) ? &it->second : nullptr;
    }

    [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept
    {
        auto it = mStorage.find(key);
        return (it != mStorage.end()) ? &it->second : nullptr;
    }

    [[nodiscard]] size_t size() const noexcept
    {
        return mStorage.size();
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return mStorage.empty();
    }

    void clear() noexcept
    {
        mStorage.clear();
    }

    [[nodiscard]] auto begin() noexcept
    {
        return mStorage.begin();
    }

    [[nodiscard]] auto end() noexcept
    {
        return mStorage.end();
    }

    [[nodiscard]] auto begin() const noexcept
    {
        return mStorage.begin();
    }

    [[nodiscard]] auto end() const noexcept
    {
        return mStorage.end();
    }

    [[nodiscard]] JsonObject to_json_object() const
    {
        JsonObject result;
        for (const auto& [key, value] : mStorage)
        {
            result[std::string(key)] = value;
        }
        return result;
    }

    static PooledJsonObject from_json_object(Pool& pool, const JsonObject& obj)
    {
        PooledJsonObject result(pool);
        for (const auto& [key, value] : obj)
        {
            result.insert(key, value);
        }
        return result;
    }
};

// =============================================================================
// Safe Numeric Conversions
// =============================================================================

/**
 * @brief Enhanced numeric conversion with checked arithmetic
 *
 * @details Uses CheckedArithmetic for overflow detection and safe conversions.
 * Provides detailed error context on overflow/underflow. Uses direct bounds
 * checking without artificial margins, matching JsonLite.h improvements.
 *
 * @tparam T Target arithmetic type
 * @param j JSON value containing numeric data
 * @return Expected containing converted value or JsonError
 */
template <typename T>
[[nodiscard]] inline Expected<T, JsonError> safe_from_json_numeric(const JsonValue& j) noexcept
{
    static_assert(std::is_arithmetic_v<T>, "T must be arithmetic type");

    try
    {
        if (j.is_int())
        {
            int64_t i64 = std::get<int64_t>(j);

            if constexpr (std::is_integral_v<T>)
            {
                if constexpr (std::is_same_v<T, uint64_t>)
                {
                    if (i64 < 0)
                    {
                        return unexpected(JsonError{JsonErrorCode::NumericOverflow,
                                                    "Negative value cannot convert to unsigned type",
                                                    0,
                                                    std::to_string(i64)});
                    }
                    return static_cast<uint64_t>(i64);
                }
                else if constexpr (std::is_unsigned_v<T>)
                {
                    if (i64 < 0)
                    {
                        return unexpected(JsonError{JsonErrorCode::NumericOverflow,
                                                    "Negative value cannot convert to unsigned type",
                                                    0,
                                                    std::to_string(i64)});
                    }
                    constexpr auto to_max = std::numeric_limits<T>::max();
                    if (static_cast<uint64_t>(i64) > static_cast<uint64_t>(to_max))
                    {
                        return unexpected(JsonError{JsonErrorCode::NumericOverflow,
                                                    "Integer value out of range for target type",
                                                    0,
                                                    std::to_string(i64)});
                    }
                    return static_cast<T>(i64);
                }
                else
                {
                    constexpr auto to_min = std::numeric_limits<T>::lowest();
                    constexpr auto to_max = std::numeric_limits<T>::max();

                    if (i64 < static_cast<int64_t>(to_min) || i64 > static_cast<int64_t>(to_max))
                    {
                        return unexpected(JsonError{JsonErrorCode::NumericOverflow,
                                                    "Integer value out of range for target type",
                                                    0,
                                                    std::to_string(i64)});
                    }
                    return static_cast<T>(i64);
                }
            }
            else
            {
                return static_cast<T>(i64);
            }
        }
        else if (j.is_number())
        {
            double d = std::get<double>(j);

            if constexpr (std::is_floating_point_v<T>)
            {
                return static_cast<T>(d);
            }
            else
            {
                double intpart;
                if (std::abs(std::modf(d, &intpart)) > json_detail::double_epsilon)
                {
                    return unexpected(
                        JsonError{JsonErrorCode::TypeError, "Fractional value for integer type", 0, std::to_string(d)});
                }

                if constexpr (std::is_unsigned_v<T>)
                {
                    if (intpart < 0.0 || intpart > static_cast<double>(std::numeric_limits<T>::max()))
                    {
                        return unexpected(JsonError{JsonErrorCode::NumericOverflow,
                                                    "Value out of range for target type",
                                                    0,
                                                    std::to_string(d)});
                    }
                }
                else
                {
                    if (intpart < static_cast<double>(std::numeric_limits<T>::min()) ||
                        intpart > static_cast<double>(std::numeric_limits<T>::max()))
                    {
                        return unexpected(JsonError{JsonErrorCode::NumericOverflow,
                                                    "Value out of range for target type",
                                                    0,
                                                    std::to_string(d)});
                    }
                }

                return static_cast<T>(intpart);
            }
        }

        return unexpected(
            JsonError{JsonErrorCode::TypeError, "Expected numeric type", 0, std::string(json_detail::typeName(j))});
    }
    catch (const std::exception& e)
    {
        std::string full_msg = e.what();
        JsonErrorCode code = json_detail::classify_exception(full_msg);
        return unexpected(JsonError{code, "Numeric operation failed.", 0, full_msg});
    }
}

/**
 * @brief Safe deserialization with Expected return
 *
 * @details Generic safe deserialization that returns Expected instead of throwing.
 * Composes well with other Expected-returning functions.
 *
 * @tparam T Target type for deserialization
 * @param j JSON value to deserialize
 * @return Expected containing deserialized value or JsonError
 */
template <typename T>
[[nodiscard]] inline Expected<T, JsonError> safe_from_json(const JsonValue& j) noexcept
{
    try
    {
        T result;
        from_json(j, result);
        return result;
    }
    catch (const std::exception& e)
    {
        std::string full_msg = e.what();
        JsonErrorCode code = json_detail::classify_exception(full_msg);
        size_t error_pos = json_detail::extract_position_from_error(full_msg);
        return unexpected(JsonError{code, "Deserialization failed.", error_pos, full_msg});
    }
}

// =============================================================================
// Batch Operations
// =============================================================================

/**
 * @brief Batch JSON parsing with early exit on error
 *
 * @details Parses multiple JSON strings and returns results. Stops at first error
 * if fail_fast is true, otherwise collects all errors.
 *
 * @param json_strings Vector of JSON strings to parse
 * @param fail_fast If true, stop at first error; if false, collect all errors
 * @return Expected containing vector of results or vector of errors
 *
 * @note When fail_fast=false and errors occur, the results vector contains nullptr
 * at positions where parsing failed. Check the error vector to identify which
 * indices failed.
 */
// GCC 14 emits false-positive -Wmaybe-uninitialized warnings for std::variant
// internals (string_length, _Rb_tree_header) when pushing JsonValue into vectors.
// This is a known GCC bug with complex variant types. Suppress for this function only.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
inline Expected<std::vector<JsonValue>, std::vector<JsonError>>
batch_parse_json(const std::vector<std::string>& json_strings, bool fail_fast = false) noexcept
{
    std::vector<JsonValue> results;
    std::vector<JsonError> errors;

    results.reserve(json_strings.size());

    for (size_t i = 0; i < json_strings.size(); ++i)
    {
        auto result = try_parse_json(json_strings[i]);

        if (result)
        {
            results.push_back(std::move(*result));
        }
        else
        {
            if (fail_fast)
            {
                return unexpected(std::vector<JsonError>{result.error()});
            }
            errors.push_back(result.error());
            results.push_back(nullptr);
        }
    }

    if (!errors.empty())
    {
        return unexpected(std::move(errors));
    }

    return results;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

// =============================================================================
// Performance Statistics
// =============================================================================

/**
 * @brief Performance statistics for JSON operations
 */
struct JsonStats
{
    size_t parse_count = 0;
    size_t serialize_count = 0;
    size_t total_bytes_parsed = 0;
    size_t total_bytes_serialized = 0;
    double total_parse_time_ms = 0.0;
    double total_serialize_time_ms = 0.0;

    void reset() noexcept
    {
        *this = JsonStats{};
    }

    [[nodiscard]] double avg_parse_time_ms() const noexcept
    {
        return (parse_count > 0) ? (total_parse_time_ms / static_cast<double>(parse_count)) : 0.0;
    }

    [[nodiscard]] double avg_serialize_time_ms() const noexcept
    {
        return (serialize_count > 0) ? (total_serialize_time_ms / static_cast<double>(serialize_count)) : 0.0;
    }

    [[nodiscard]] double parse_throughput_mb_per_sec() const noexcept
    {
        if (total_parse_time_ms <= 0.0)
        {
            return 0.0;
        }
        return (static_cast<double>(total_bytes_parsed) / 1024.0 / 1024.0) / (total_parse_time_ms / 1000.0);
    }

    [[nodiscard]] double serialize_throughput_mb_per_sec() const noexcept
    {
        if (total_serialize_time_ms <= 0.0)
        {
            return 0.0;
        }
        return (static_cast<double>(total_bytes_serialized) / 1024.0 / 1024.0) / (total_serialize_time_ms / 1000.0);
    }
};

// =============================================================================
// High-Level Encode/Decode API (mirrors Cbor pattern)
// =============================================================================

/**
 * @brief Encode a value to JSON string
 *
 * @details Converts any JSON-serializable type to a JSON string.
 * Uses the existing to_json() infrastructure with Expected-based error handling.
 *
 * @tparam T Type to encode (must have to_json overload)
 * @param out Output string to write JSON to
 * @param value Value to encode
 * @param pretty Enable pretty printing (default: false)
 * @return JsonResult<void> Success or error
 */
template <typename T>
[[nodiscard]] inline JsonResult<void> json_encode_to(std::string& out, const T& value, bool pretty = false) noexcept
{
    try
    {
        JsonValue jv;
        to_json(jv, value);
        out = to_json_string(jv, pretty);
        return {};
    }
    catch (const std::exception& ex)
    {
        return make_unexpected(
            JsonError{JsonErrorCode::Unknown, std::string("JSON encode error: ") + ex.what(), 0, ""});
    }
}

/**
 * @brief Decode a value from JSON string
 *
 * @details Parses JSON string and converts to the target type.
 * Uses the existing from_json() infrastructure with Expected-based error handling.
 *
 * @tparam T Target type (must have from_json overload)
 * @param json JSON string to parse
 * @return JsonResult<T> Decoded value or error
 */
template <typename T>
[[nodiscard]] inline JsonResult<T> json_decode_from(std::string_view json) noexcept
{
    try
    {
        size_t pos = 0;
        JsonValue jv = json_detail::parse_value<StandardJsonPolicy>(json, pos);

        json_detail::skip_whitespace<StandardJsonPolicy>(json, pos);
        if (pos != json.size())
        {
            return make_unexpected(JsonError{JsonErrorCode::ExtraData,
                                             "Extra data after JSON value",
                                             pos,
                                             std::string(json.substr(pos, std::min<size_t>(20, json.size() - pos)))});
        }

        T result;
        from_json(jv, result);
        return result;
    }
    catch (const std::exception& ex)
    {
        std::string full_msg = ex.what();
        JsonErrorCode code = json_detail::classify_exception(full_msg);
        size_t error_pos = json_detail::extract_position_from_error(full_msg);

        return make_unexpected(JsonError{code, full_msg, error_pos, ""});
    }
}

// =============================================================================
// Convenience Macro
// =============================================================================

/**
 * @section using_fatpjson Bringing FatPJson Symbols Into Scope
 *
 * @details In your .cpp files (NEVER in headers), you can add selective using
 * declarations for commonly needed FatPJson functionality:
 *
 * @code{.cpp}
 * // Core JSON types (from JsonLite.h)
 * using fat_p::JsonValue;
 * using fat_p::JsonObject;
 * using fat_p::JsonArray;
 * using fat_p::save_params;
 * using fat_p::load_params;
 *
 * // FatPJson extensions
 * using fat_p::try_parse_json;
 * using fat_p::try_load_json;
 * using fat_p::try_save_json;
 * using fat_p::safe_from_json;
 * using fat_p::ConfigJsonPolicy;
 * @endcode
 *
 * Or use explicit fat_p:: qualification throughout your code.
 */

// =============================================================================
// Conversion Utilities
// =============================================================================

/**
 * @brief Convert FatPJsonArray to standard JsonArray
 */
[[nodiscard]] inline JsonArray to_json_array(const FatPJsonArray& arr)
{
    JsonArray result;
    result.reserve(arr.size());
    for (const auto& elem : arr)
    {
        result.push_back(elem);
    }
    return result;
}

/**
 * @brief Convert FatPJsonObject to standard JsonObject
 */
template <typename Compare, typename Allocator>
[[nodiscard]] inline JsonObject to_json_object(const FatPJsonObject<Compare, Allocator>& obj)
{
    JsonObject result;
    for (const auto& [key, value] : obj)
    {
        result[key] = value;
    }
    return result;
}

/**
 * @brief Create FatPJsonArray from standard JsonArray
 */
[[nodiscard]] inline FatPJsonArray from_json_array(const JsonArray& arr)
{
    FatPJsonArray result;
    result.reserve(arr.size());
    for (const auto& elem : arr)
    {
        result.push_back(elem);
    }
    return result;
}

/**
 * @brief Create FatPJsonObject from standard JsonObject
 *
 * @details Uses FlatMap's range constructor for O(N log N) construction
 * instead of individual insertions which would be O(N^2).
 */
[[nodiscard]] inline FatPJsonObject<> from_json_object(const JsonObject& obj)
{
    // Use range constructor for O(N log N) instead of O(N^2) individual inserts
    return FatPJsonObject<>(obj.begin(), obj.end());
}

// =============================================================================
// JSON Pointer with Expected
// =============================================================================

/**
 * @section json_pointer_expected JSON Pointer with Expected
 *
 * @details Exception-free JSON Pointer operations that return Expected instead of
 * throwing exceptions. These are wrappers around JsonLite's RFC 6901 implementation
 * that provide zero-overhead error handling without exceptions.
 *
 * Features:
 * - RFC 6901 compliant navigation
 * - Exception-free error handling
 * - Composable error handling with Expected
 * - Zero overhead compared to exception-based version
 * - All error conditions categorized as JsonError
 */

/**
 * @brief Exception-free JSON Pointer query (const version)
 *
 * @details Performs RFC 6901 JSON Pointer navigation and returns Expected for
 * zero-overhead error handling without exceptions. All navigation errors are
 * converted to structured JsonError objects.
 *
 * @param root The root JSON value to query
 * @param pointer JSON Pointer path (must start with '/' or be empty)
 * @return Expected containing pointer to value or JsonError
 *
 * @note Returns Expected<const JsonValue*> - the pointer is valid as long as root exists
 * @note This is the const version - cannot modify the result
 */
[[nodiscard]] inline Expected<const JsonValue*, JsonError> try_query_json_pointer(const JsonValue& root,
                                                                                  std::string_view pointer) noexcept
{
    try
    {
        auto result = json_detail::query_json_pointer_noexcept(root, pointer);
        if (!result)
        {
            const JsonError inner = result.error();
            return unexpected(JsonError{inner.code, "JSON Pointer query failed", inner.position, inner.message});
        }

        return *result;
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        JsonErrorCode code = json_detail::classify_exception(msg);

        if (msg.find("JSON Pointer") != std::string_view::npos)
        {
            if (code == JsonErrorCode::ParseError || code == JsonErrorCode::Unknown)
            {
                code = JsonErrorCode::TypeError;
            }
        }

        return unexpected(JsonError{code, "JSON Pointer query failed", 0, msg});
    }
}

/**
 * @brief Exception-free JSON Pointer query (mutable version)
 *
 * @details Mutable version of try_query_json_pointer that allows modification
 * of the target value. All navigation rules and error handling are the same
 * as the const version.
 *
 * @param root The root JSON value to query
 * @param pointer JSON Pointer path (must start with '/' or be empty)
 * @return Expected containing mutable pointer to value or JsonError
 *
 * @note Returns Expected<JsonValue*> - can modify the result
 * @note Lifetime of the pointer is tied to the root JsonValue
 */
[[nodiscard]] inline Expected<JsonValue*, JsonError> try_query_json_pointer(JsonValue& root,
                                                                            std::string_view pointer) noexcept
{
    try
    {
        auto result = json_detail::query_json_pointer_noexcept(root, pointer);
        if (!result)
        {
            const JsonError inner = result.error();
            return unexpected(JsonError{inner.code, "JSON Pointer query failed", inner.position, inner.message});
        }

        return *result;
    }
    catch (const std::exception& e)
    {
        std::string msg = e.what();
        JsonErrorCode code = json_detail::classify_exception(msg);

        if (msg.find("JSON Pointer") != std::string_view::npos)
        {
            if (code == JsonErrorCode::ParseError || code == JsonErrorCode::Unknown)
            {
                code = JsonErrorCode::TypeError;
            }
        }

        return unexpected(JsonError{code, "JSON Pointer query failed", 0, msg});
    }
}

/**
 * @brief Type-safe exception-free JSON Pointer query with automatic conversion
 *
 * @details Combines try_query_json_pointer() and safe_from_json() into a single
 * operation. This is the most convenient way to extract typed values from JSON
 * documents without exceptions.
 *
 * @tparam T Target type for conversion
 * @param root The root JSON value to query
 * @param pointer JSON Pointer path
 * @return Expected containing converted value or JsonError
 */
template <typename T>
[[nodiscard]] inline Expected<T, JsonError> try_query_json_as(const JsonValue& root, std::string_view pointer) noexcept
{
    auto query_result = try_query_json_pointer(root, pointer);
    if (!query_result)
    {
        return unexpected(query_result.error());
    }

    return safe_from_json<T>(**query_result);
}

/**
 * @brief Type-safe JSON Pointer query with default value on failure
 *
 * @details Convenience function that returns a default value if navigation or
 * conversion fails. Useful for optional configuration values.
 *
 * @tparam T Target type for conversion
 * @param root The root JSON value to query
 * @param pointer JSON Pointer path
 * @param default_value Value to return on failure
 * @return Converted value or default_value on any error
 */
template <typename T>
[[nodiscard]] inline T query_json_as_or(const JsonValue& root, std::string_view pointer, T default_value) noexcept
{
    auto result = try_query_json_as<T>(root, pointer);
    return result ? *result : default_value;
}

} // namespace fat_p

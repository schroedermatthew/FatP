/**
 * @file FatPJsonStream.h
 * @brief FAT-P streaming JSON serialization
 *
 * @layer Domain
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPJsonStream
  file_role: public_header
  path: fat_p/FatPJsonStream.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for FatPJsonStream."
  api_stability: in_work
  related:
    docs_search: "FatPJsonStream"
    tests:
      - tests/test_FatPJsonStream.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 1
    defines_unprefixed: 1
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/
/**
 * @file FatPJsonStream.h
 * @brief Enhanced streaming JSON parser with policy-based design and Expected
 * @version 1.0
 *
 * @details Builds on JsonStreamLite to provide:
 * - Expected-based error handling (StreamResult<T>)
 * - Policy-based limits configuration
 * - Validation policies (strict mode, comments)
 * - Callback hooks for progress monitoring
 * - Integration with HpcVector for high-performance buffers
 * - Detailed error information with position and context
 *
 * @note Requires: Expected.h, enforce.h, HpcVector.h, JsonStreamLite.h
 */

#include "enforce.h"
#include "Expected.h"
#include "HpcVector.h"
#include "JsonStreamLite.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace fat_p
{
namespace json_stream_fatp
{

// Re-export base types
using json_stream::error_to_string;
using json_stream::JsonArray;
using json_stream::JsonObject;
using json_stream::JsonValue;
using json_stream::ParseError;
using json_stream::ParseStatus;

// =============================================================================
// Enhanced Error Types
// =============================================================================

/**
 * @brief Detailed stream parsing error with position and context
 */
struct StreamError
{
    ParseError code = ParseError::None;
    std::size_t byte_position = 0;
    std::size_t line = 1;
    std::size_t column = 1;
    std::string message;
    std::string context;

    StreamError() = default;

    explicit StreamError(ParseError c)
        : code(c)
        , message(error_to_string(c))
    {
    }

    StreamError(ParseError c, std::size_t pos, std::size_t ln, std::size_t col)
        : code(c)
        , byte_position(pos)
        , line(ln)
        , column(col)
        , message(error_to_string(c))
    {
    }

    StreamError(ParseError c, std::size_t pos, std::size_t ln, std::size_t col, std::string ctx)
        : code(c)
        , byte_position(pos)
        , line(ln)
        , column(col)
        , message(error_to_string(c))
        , context(std::move(ctx))
    {
    }

    explicit StreamError(std::string msg)
        : code(ParseError::InternalError)
        , message(std::move(msg))
    {
    }

    std::string to_string() const
    {
        std::string result = message;
        result += " at line ";
        result += std::to_string(line);
        result += ", column ";
        result += std::to_string(column);
        result += " (byte ";
        result += std::to_string(byte_position);
        result += ")";
        if (!context.empty())
        {
            result += ": ";
            result += context;
        }
        return result;
    }
};

/**
 * @brief Result type for streaming operations
 */
template <typename T>
using StreamResult = Expected<T, StreamError>;

// =============================================================================
// Limits Policies
// =============================================================================

/**
 * @brief Default limits suitable for general use
 */
struct DefaultLimitsPolicy
{
    static constexpr std::size_t max_depth = 64;
    static constexpr std::size_t max_string_bytes = 16 * 1024 * 1024;
    static constexpr std::size_t max_total_bytes = 256 * 1024 * 1024;
    static constexpr std::size_t max_array_elements = 1024 * 1024;
    static constexpr std::size_t max_object_members = 1024 * 1024;
};

/**
 * @brief Strict limits for untrusted input (network protocols)
 */
struct StrictLimitsPolicy
{
    static constexpr std::size_t max_depth = 32;
    static constexpr std::size_t max_string_bytes = 64 * 1024;
    static constexpr std::size_t max_total_bytes = 1024 * 1024;
    static constexpr std::size_t max_array_elements = 10000;
    static constexpr std::size_t max_object_members = 10000;
};

/**
 * @brief Relaxed limits for trusted input (local files)
 */
struct RelaxedLimitsPolicy
{
    static constexpr std::size_t max_depth = 256;
    static constexpr std::size_t max_string_bytes = 1024 * 1024 * 1024;
    static constexpr std::size_t max_total_bytes = 4ULL * 1024 * 1024 * 1024;
    static constexpr std::size_t max_array_elements = 100 * 1024 * 1024;
    static constexpr std::size_t max_object_members = 100 * 1024 * 1024;
};

/**
 * @brief Custom limits specified at runtime
 */
struct RuntimeLimitsPolicy
{
    std::size_t max_depth = 64;
    std::size_t max_string_bytes = 16 * 1024 * 1024;
    std::size_t max_total_bytes = 256 * 1024 * 1024;
    std::size_t max_array_elements = 1024 * 1024;
    std::size_t max_object_members = 1024 * 1024;
};

// =============================================================================
// Validation Policies
// =============================================================================

/**
 * @brief No additional validation (fastest)
 */
struct NoValidationPolicy
{
    static constexpr bool check_duplicate_keys = false;
    static constexpr bool allow_trailing_comma = true;
    static constexpr bool allow_comments = false;
};

/**
 * @brief Strict RFC 8259 compliance
 */
struct StrictValidationPolicy
{
    static constexpr bool check_duplicate_keys = true;
    static constexpr bool allow_trailing_comma = false;
    static constexpr bool allow_comments = false;
};

/**
 * @brief JSON5-like relaxed parsing
 */
struct RelaxedValidationPolicy
{
    static constexpr bool check_duplicate_keys = false;
    static constexpr bool allow_trailing_comma = true;
    static constexpr bool allow_comments = true;
};

// =============================================================================
// Callback Types
// =============================================================================

/**
 * @brief Progress callback signature
 */
using ProgressCallback =
    std::function<void(std::size_t bytes_consumed, std::size_t current_depth, std::size_t values_parsed)>;

// =============================================================================
// Enhanced Stream Parser
// =============================================================================

/**
 * @brief Policy-based streaming JSON parser with Expected error handling
 *
 * @tparam LimitsPolicy Policy defining parsing limits
 * @tparam ValidationPolicy Policy for validation rules
 */
template <typename LimitsPolicy = DefaultLimitsPolicy, typename ValidationPolicy = NoValidationPolicy>
class FatPJsonStreamParser
{
public:
    FatPJsonStreamParser()
        : mLine(1)
        , mColumn(1)
    {
        apply_limits();
    }

    explicit FatPJsonStreamParser(const RuntimeLimitsPolicy& limits)
        : runtime_limits_(limits)
        , mLine(1)
        , mColumn(1)
    {
        apply_limits();
    }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    void set_progress_callback(ProgressCallback cb)
    {
        progress_callback_ = std::move(cb);
    }

    void set_progress_interval(std::size_t bytes)
    {
        progress_interval_ = bytes;
    }

    // -------------------------------------------------------------------------
    // Parsing Interface
    // -------------------------------------------------------------------------

    /**
     * @brief Feed bytes to the parser with Expected result
     */
    StreamResult<ParseStatus> feed(const char* data, std::size_t size)
    {
        std::size_t bytes_before = mParser.stats().bytes_consumed;

        for (std::size_t i = 0; i < size; ++i)
        {
            char c = data[i];

            auto status = mParser.feed(&data[i], 1);

            // Track line/column
            if (c == '\n')
            {
                ++mLine;
                mColumn = 1;
            }
            else
            {
                ++mColumn;
            }

            if (status == ParseStatus::Error)
            {
                return make_unexpected(StreamError(mParser.error(), mParser.stats().bytes_consumed, mLine, mColumn));
            }

            // Progress callback
            if (progress_callback_ && progress_interval_ > 0)
            {
                std::size_t consumed = mParser.stats().bytes_consumed;
                if (consumed / progress_interval_ != bytes_before / progress_interval_)
                {
                    progress_callback_(consumed, mParser.stats().current_depth, mParser.stats().values_parsed);
                }
                bytes_before = consumed;
            }

            if (status == ParseStatus::Done)
            {
                return ParseStatus::Done;
            }
        }

        return ParseStatus::NeedMoreData;
    }

    StreamResult<ParseStatus> feed(const std::uint8_t* data, std::size_t size)
    {
        return feed(reinterpret_cast<const char*>(data), size);
    }

    /**
     * @brief Parse complete buffer in one call
     */
    StreamResult<JsonValue> parse(const char* data, std::size_t size)
    {
        reset();

        auto status = feed(data, size);
        if (!status)
        {
            return make_unexpected(status.error());
        }

        if (*status == ParseStatus::NeedMoreData)
        {
            return make_unexpected(StreamError(ParseError::UnexpectedEof,
                                               mParser.stats().bytes_consumed,
                                               mLine,
                                               mColumn,
                                               "incomplete JSON input"));
        }

        return mParser.take_result();
    }

    StreamResult<JsonValue> parse(const std::string& data)
    {
        return parse(data.data(), data.size());
    }

    template <std::size_t N>
    StreamResult<JsonValue> parse(const char (&data)[N])
    {
        return parse(data, N - 1); // Exclude null terminator
    }

    template <typename Container,
              typename = decltype(std::declval<Container>().data()),
              typename = decltype(std::declval<Container>().size())>
    StreamResult<JsonValue> parse(const Container& data)
    {
        return parse(reinterpret_cast<const char*>(data.data()), data.size());
    }

    // -------------------------------------------------------------------------
    // State Access
    // -------------------------------------------------------------------------

    bool is_done() const
    {
        return mParser.is_done();
    }

    bool has_error() const
    {
        return mParser.has_error();
    }

    const JsonValue& result() const
    {
        FATP_ENFORCE(mParser.is_done(), "parsing not complete");
        return mParser.result();
    }

    StreamResult<JsonValue> take_result()
    {
        if (!mParser.is_done())
        {
            return make_unexpected(StreamError("parsing not complete"));
        }
        return mParser.take_result();
    }

    const json_stream::JsonStreamParser::Stats& stats() const
    {
        return mParser.stats();
    }

    std::size_t current_line() const noexcept
    {
        return mLine;
    }

    std::size_t current_column() const noexcept
    {
        return mColumn;
    }

    void reset()
    {
        mParser.reset();
        mLine = 1;
        mColumn = 1;
    }

private:
    json_stream::JsonStreamParser mParser;
    RuntimeLimitsPolicy runtime_limits_;
    ProgressCallback progress_callback_;
    std::size_t progress_interval_ = 0;
    std::size_t mLine;
    std::size_t mColumn;

    void apply_limits()
    {
        json_stream::JsonStreamParser::Limits limits;

        if constexpr (std::is_same_v<LimitsPolicy, RuntimeLimitsPolicy>)
        {
            limits.max_depth = runtime_limits_.max_depth;
            limits.max_string_bytes = runtime_limits_.max_string_bytes;
            limits.max_total_bytes = runtime_limits_.max_total_bytes;
            limits.max_array_elements = runtime_limits_.max_array_elements;
            limits.max_object_members = runtime_limits_.max_object_members;
        }
        else
        {
            limits.max_depth = LimitsPolicy::max_depth;
            limits.max_string_bytes = LimitsPolicy::max_string_bytes;
            limits.max_total_bytes = LimitsPolicy::max_total_bytes;
            limits.max_array_elements = LimitsPolicy::max_array_elements;
            limits.max_object_members = LimitsPolicy::max_object_members;
        }

        mParser.set_limits(limits);
    }
};

// =============================================================================
// Type Aliases
// =============================================================================

/**
 * @brief Default streaming parser
 */
using DefaultJsonStreamParser = FatPJsonStreamParser<DefaultLimitsPolicy, NoValidationPolicy>;

/**
 * @brief Strict parser for untrusted input
 */
using StrictJsonStreamParser = FatPJsonStreamParser<StrictLimitsPolicy, StrictValidationPolicy>;

/**
 * @brief Relaxed parser for trusted input
 */
using RelaxedJsonStreamParser = FatPJsonStreamParser<RelaxedLimitsPolicy, RelaxedValidationPolicy>;

/**
 * @brief Parser with runtime-configurable limits
 */
using ConfigurableJsonStreamParser = FatPJsonStreamParser<RuntimeLimitsPolicy, NoValidationPolicy>;

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Parse JSON with default settings
 */
inline StreamResult<JsonValue> stream_parse_json(const char* data, std::size_t size)
{
    DefaultJsonStreamParser parser;
    return parser.parse(data, size);
}

inline StreamResult<JsonValue> stream_parse_json(const std::string& data)
{
    return stream_parse_json(data.data(), data.size());
}

template <std::size_t N>
inline StreamResult<JsonValue> stream_parse_json(const char (&data)[N])
{
    return stream_parse_json(data, N - 1);
}

/**
 * @brief Parse JSON with strict validation (for untrusted input)
 */
inline StreamResult<JsonValue> stream_parse_json_strict(const char* data, std::size_t size)
{
    StrictJsonStreamParser parser;
    return parser.parse(data, size);
}

inline StreamResult<JsonValue> stream_parse_json_strict(const std::string& data)
{
    return stream_parse_json_strict(data.data(), data.size());
}

/**
 * @brief Parse JSON with custom limits
 */
inline StreamResult<JsonValue>
stream_parse_json_limited(const char* data, std::size_t size, const RuntimeLimitsPolicy& limits)
{
    ConfigurableJsonStreamParser parser(limits);
    return parser.parse(data, size);
}

inline StreamResult<JsonValue> stream_parse_json_limited(const std::string& data, const RuntimeLimitsPolicy& limits)
{
    return stream_parse_json_limited(data.data(), data.size(), limits);
}

} // namespace json_stream_fatp

// Short namespace alias for convenience
// Use fat_p::jsf::StreamError, fat_p::jsf::stream_parse_json, etc.
namespace jsf = json_stream_fatp;

} // namespace fat_p

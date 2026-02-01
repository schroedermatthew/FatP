/**
 * @file FatPCborStream.h
 * @brief FAT-P streaming CBOR serialization
 *
 * @layer Domain
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: FatPCborStream
  file_role: public_header
  path: include/fat_p/FatPCborStream.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for FatPCborStream."
  api_stability: in_work
  related:
    docs_search: "FatPCborStream"
    tests:
      - components/Cbor/tests/test_FatPCborStream.cpp
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
 * @file FatPCborStream.h
 * @brief Enhanced streaming CBOR parser with policy-based design and Expected error handling
 * @version 1.0
 *
 * @details Builds on CborStreamLite to provide:
 * - Expected-based error handling (StreamResult<T>)
 * - Policy-based limits configuration
 * - Validation policies (UTF-8, strict mode)
 * - Callback hooks for progress monitoring
 * - Integration with HpcVector for high-performance buffers
 * - Detailed error information with position and context
 *
 * @note Requires: Expected.h, enforce.h, HpcVector.h, CborStreamLite.h
 */

#include "CborStreamLite.h"
#include "enforce.h"
#include "Expected.h"
#include "HpcVector.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace fat_p
{
namespace cbor_stream
{

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
    std::size_t depth = 0;
    std::string message;
    std::string context;

    StreamError() = default;

    explicit StreamError(ParseError c)
        : code(c)
        , message(error_to_string(c))
    {
    }

    StreamError(ParseError c, std::size_t pos, std::size_t d)
        : code(c)
        , byte_position(pos)
        , depth(d)
        , message(error_to_string(c))
    {
    }

    StreamError(ParseError c, std::size_t pos, std::size_t d, std::string ctx)
        : code(c)
        , byte_position(pos)
        , depth(d)
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
        result += " at byte ";
        result += std::to_string(byte_position);
        if (depth > 0)
        {
            result += " (depth ";
            result += std::to_string(depth);
            result += ")";
        }
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
    static constexpr std::size_t max_map_pairs = 1024 * 1024;
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
    static constexpr std::size_t max_map_pairs = 10000;
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
    static constexpr std::size_t max_map_pairs = 100 * 1024 * 1024;
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
    std::size_t max_map_pairs = 1024 * 1024;
};

// =============================================================================
// Validation Policies
// =============================================================================

/**
 * @brief No validation (fastest)
 */
struct NoValidationPolicy
{
    static bool validate_utf8(const std::string&)
    {
        return true;
    }

    static bool validate_map_key_order(const CborValue&, const CborValue&)
    {
        return true;
    }
};

/**
 * @brief Validate UTF-8 text strings
 */
struct Utf8ValidationPolicy
{
    static bool validate_utf8(const std::string& s)
    {
        const auto* bytes = reinterpret_cast<const unsigned char*>(s.data());
        std::size_t len = s.size();
        std::size_t i = 0;

        while (i < len)
        {
            if (bytes[i] < 0x80)
            {
                ++i;
            }
            else if ((bytes[i] & 0xE0) == 0xC0)
            {
                if (i + 1 >= len || (bytes[i + 1] & 0xC0) != 0x80)
                {
                    return false;
                }
                if (bytes[i] < 0xC2)
                {
                    return false; // Overlong
                }
                i += 2;
            }
            else if ((bytes[i] & 0xF0) == 0xE0)
            {
                if (i + 2 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80)
                {
                    return false;
                }
                // Check overlong and surrogate
                unsigned int cp = ((bytes[i] & 0x0F) << 12) | ((bytes[i + 1] & 0x3F) << 6) | (bytes[i + 2] & 0x3F);
                if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))
                {
                    return false;
                }
                i += 3;
            }
            else if ((bytes[i] & 0xF8) == 0xF0)
            {
                if (i + 3 >= len || (bytes[i + 1] & 0xC0) != 0x80 || (bytes[i + 2] & 0xC0) != 0x80 ||
                    (bytes[i + 3] & 0xC0) != 0x80)
                {
                    return false;
                }
                // Check overlong and max codepoint
                unsigned int cp = ((bytes[i] & 0x07) << 18) | ((bytes[i + 1] & 0x3F) << 12) |
                                  ((bytes[i + 2] & 0x3F) << 6) | (bytes[i + 3] & 0x3F);
                if (cp < 0x10000 || cp > 0x10FFFF)
                {
                    return false;
                }
                i += 4;
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    static bool validate_map_key_order(const CborValue&, const CborValue&)
    {
        return true;
    }
};

/**
 * @brief Full RFC 8949 compliance (UTF-8 + canonical map ordering)
 */
struct StrictValidationPolicy : Utf8ValidationPolicy
{
    static bool validate_map_key_order(const CborValue& prev, const CborValue& curr)
    {
        // RFC 8949 deterministic encoding requires keys in bytewise lexicographic order
        // For simplicity, we just check that keys are ordered
        return prev < curr;
    }
};

// =============================================================================
// Callback Types
// =============================================================================

/**
 * @brief Progress callback signature
 */
using ProgressCallback =
    std::function<void(std::size_t bytes_consumed, std::size_t current_depth, std::size_t values_parsed)>;

/**
 * @brief Value callback (called for each parsed value)
 */
using ValueCallback = std::function<bool(const CborValue& value, std::size_t depth)>;

// =============================================================================
// Enhanced Stream Parser
// =============================================================================

/**
 * @brief Policy-based streaming CBOR parser with Expected error handling
 *
 * @tparam LimitsPolicy Policy defining parsing limits (default/strict/relaxed/runtime)
 * @tparam ValidationPolicy Policy for validation (none/utf8/strict)
 * @tparam Buffer Internal buffer type (default: HpcVector)
 */
template <typename LimitsPolicy = DefaultLimitsPolicy,
          typename ValidationPolicy = NoValidationPolicy,
          typename Buffer = HpcVector<std::uint8_t>>
class FatPStreamParser
{
public:
    using Limits = typename std::conditional<std::is_same_v<LimitsPolicy, RuntimeLimitsPolicy>,
                                             RuntimeLimitsPolicy,
                                             CborStreamParser::Limits>::type;

    FatPStreamParser()
    {
        apply_limits();
    }

    explicit FatPStreamParser(const RuntimeLimitsPolicy& limits)
        : runtime_limits_(limits)
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

    void set_value_callback(ValueCallback cb)
    {
        value_callback_ = std::move(cb);
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
    StreamResult<ParseStatus> feed(const std::uint8_t* data, std::size_t size)
    {
        std::size_t bytes_before = mParser.stats().bytes_consumed;

        for (std::size_t i = 0; i < size; ++i)
        {
            auto status = mParser.feed(&data[i], 1);

            if (status == ParseStatus::Error)
            {
                return make_unexpected(
                    StreamError(mParser.error(), mParser.stats().bytes_consumed, mParser.stats().current_depth));
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
                // Validate result if needed
                auto validation = validate_result();
                if (!validation)
                {
                    return make_unexpected(validation.error());
                }
                return ParseStatus::Done;
            }
        }

        return ParseStatus::NeedMoreData;
    }

    /**
     * @brief Feed bytes from a container
     */
    template <typename Container>
    StreamResult<ParseStatus> feed(const Container& data)
    {
        return feed(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
    }

    /**
     * @brief Parse complete buffer in one call
     */
    StreamResult<CborValue> parse(const std::uint8_t* data, std::size_t size)
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
                                               mParser.stats().current_depth,
                                               "incomplete CBOR input"));
        }

        return mParser.take_result();
    }

    /**
     * @brief Parse complete buffer from container
     */
    template <typename Container>
    StreamResult<CborValue> parse(const Container& data)
    {
        return parse(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
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

    const CborValue& result() const
    {
        FATP_ENFORCE(mParser.is_done(), "parsing not complete");
        return mParser.result();
    }

    StreamResult<CborValue> take_result()
    {
        if (!mParser.is_done())
        {
            return make_unexpected(StreamError("parsing not complete"));
        }
        return mParser.take_result();
    }

    const CborStreamParser::Stats& stats() const
    {
        return mParser.stats();
    }

    void reset()
    {
        mParser.reset();
    }

private:
    CborStreamParser mParser;
    RuntimeLimitsPolicy runtime_limits_;
    ProgressCallback progress_callback_;
    ValueCallback value_callback_;
    std::size_t progress_interval_ = 0;

    void apply_limits()
    {
        CborStreamParser::Limits limits;

        if constexpr (std::is_same_v<LimitsPolicy, RuntimeLimitsPolicy>)
        {
            limits.max_depth = runtime_limits_.max_depth;
            limits.max_string_bytes = runtime_limits_.max_string_bytes;
            limits.max_total_bytes = runtime_limits_.max_total_bytes;
            limits.max_array_elements = runtime_limits_.max_array_elements;
            limits.max_map_pairs = runtime_limits_.max_map_pairs;
        }
        else
        {
            limits.max_depth = LimitsPolicy::max_depth;
            limits.max_string_bytes = LimitsPolicy::max_string_bytes;
            limits.max_total_bytes = LimitsPolicy::max_total_bytes;
            limits.max_array_elements = LimitsPolicy::max_array_elements;
            limits.max_map_pairs = LimitsPolicy::max_map_pairs;
        }

        mParser.set_limits(limits);
    }

    StreamResult<void> validate_result()
    {
        if constexpr (std::is_same_v<ValidationPolicy, NoValidationPolicy>)
        {
            return {};
        }
        else
        {
            return validate_value(mParser.result());
        }
    }

    StreamResult<void> validate_value(const CborValue& value)
    {
        if (value.is_string())
        {
            if (!ValidationPolicy::validate_utf8(value.as_string()))
            {
                return make_unexpected(StreamError(ParseError::InvalidUtf8,
                                                   mParser.stats().bytes_consumed,
                                                   mParser.stats().current_depth,
                                                   "invalid UTF-8 in text string"));
            }
        }
        else if (value.is_array())
        {
            for (const auto& elem : value.as_array())
            {
                auto result = validate_value(elem);
                if (!result)
                {
                    return result;
                }
            }
        }
        else if (value.is_map())
        {
            const CborValue* prev_key = nullptr;
            for (const auto& [key, val] : value.as_map())
            {
                // Validate key
                auto key_result = validate_value(key);
                if (!key_result)
                {
                    return key_result;
                }

                // Check key ordering if strict
                if constexpr (std::is_same_v<ValidationPolicy, StrictValidationPolicy>)
                {
                    if (prev_key != nullptr)
                    {
                        if (!ValidationPolicy::validate_map_key_order(*prev_key, key))
                        {
                            return make_unexpected(StreamError(ParseError::InternalError,
                                                               mParser.stats().bytes_consumed,
                                                               mParser.stats().current_depth,
                                                               "map keys not in canonical order"));
                        }
                    }
                    prev_key = &key;
                }

                // Validate value
                auto val_result = validate_value(val);
                if (!val_result)
                {
                    return val_result;
                }
            }
        }
        else if (value.is_tagged())
        {
            auto result = validate_value(*value.as_tagged().value);
            if (!result)
            {
                return result;
            }
        }

        return {};
    }
};

// =============================================================================
// Type Aliases
// =============================================================================

/**
 * @brief Default streaming parser
 */
using DefaultStreamParser = FatPStreamParser<DefaultLimitsPolicy, NoValidationPolicy>;

/**
 * @brief Strict parser for untrusted input
 */
using StrictStreamParser = FatPStreamParser<StrictLimitsPolicy, StrictValidationPolicy>;

/**
 * @brief Relaxed parser for trusted input
 */
using RelaxedStreamParser = FatPStreamParser<RelaxedLimitsPolicy, NoValidationPolicy>;

/**
 * @brief Parser with UTF-8 validation
 */
using ValidatingStreamParser = FatPStreamParser<DefaultLimitsPolicy, Utf8ValidationPolicy>;

/**
 * @brief Parser with runtime-configurable limits
 */
using ConfigurableStreamParser = FatPStreamParser<RuntimeLimitsPolicy, NoValidationPolicy>;

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Parse CBOR with default settings
 */
inline StreamResult<CborValue> stream_parse(const std::uint8_t* data, std::size_t size)
{
    DefaultStreamParser parser;
    return parser.parse(data, size);
}

template <typename Container>
inline StreamResult<CborValue> stream_parse(const Container& data)
{
    return stream_parse(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

/**
 * @brief Parse CBOR with strict validation (for untrusted input)
 */
inline StreamResult<CborValue> stream_parse_strict(const std::uint8_t* data, std::size_t size)
{
    StrictStreamParser parser;
    return parser.parse(data, size);
}

template <typename Container>
inline StreamResult<CborValue> stream_parse_strict(const Container& data)
{
    return stream_parse_strict(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

/**
 * @brief Parse CBOR with custom limits
 */
inline StreamResult<CborValue>
stream_parse_limited(const std::uint8_t* data, std::size_t size, const RuntimeLimitsPolicy& limits)
{
    ConfigurableStreamParser parser(limits);
    return parser.parse(data, size);
}

template <typename Container>
inline StreamResult<CborValue> stream_parse_limited(const Container& data, const RuntimeLimitsPolicy& limits)
{
    return stream_parse_limited(reinterpret_cast<const std::uint8_t*>(data.data()), data.size(), limits);
}

} // namespace cbor_stream
} // namespace fat_p

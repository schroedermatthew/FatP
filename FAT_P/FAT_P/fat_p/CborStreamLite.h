/**
 * @file CborStreamLite.h
 * @brief Streaming CBOR parser with incremental processing
 *
 * @layer Foundation
 */
#pragma once

/*
FATP_META:
  meta_version: 1
  component: CborStreamLite
  file_role: public_header
  path: fat_p/CborStreamLite.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for CborStreamLite."
  api_stability: in_work
  related:
    docs_search: "CborStreamLite"
    tests:
      - tests/test_CborStreamLite.cpp
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
 * @file CborStreamLite.h
 * @brief Streaming CBOR parser with incremental DOM construction
 * @version 1.0
 *
 * @details Provides a streaming parser for CBOR (RFC 8949) that can process
 * data in chunks, enabling early rejection of malformed or malicious input.
 * Builds a DOM tree incrementally, suitable for network protocol parsing.
 *
 * Key features:
 * - Byte-at-a-time state machine (can suspend/resume at any point)
 * - Configurable limits (depth, string size, total size)
 * - Early rejection of limit violations
 * - Zero external dependencies (standalone header)
 * - Builds CborValue DOM compatible with further processing
 *
 * @note CBOR is easier to stream than JSON because lengths are prefix-encoded.
 * @note Does not support indefinite-length encoding (rare in practice)
 */

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace fat_p
{
namespace cbor_stream
{

// =============================================================================
// CBOR Value Type (DOM)
// =============================================================================

/**
 * @brief CBOR major types per RFC 8949
 */
enum class MajorType : std::uint8_t
{
    UnsignedInt = 0,
    NegativeInt = 1,
    ByteString  = 2,
    TextString  = 3,
    Array       = 4,
    Map         = 5,
    Tag         = 6,
    Simple      = 7
};

// Forward declaration
class CborValue;

using CborArray = std::vector<CborValue>;
using CborMap = std::map<CborValue, CborValue>;
using CborBytes = std::vector<std::uint8_t>;

/**
 * @brief Simple values (booleans, null, undefined)
 */
enum class SimpleValue : std::uint8_t
{
    False     = 20,
    True      = 21,
    Null      = 22,
    Undefined = 23
};

/**
 * @brief Tagged value wrapper
 */
struct CborTagged
{
    std::uint64_t tag;
    std::unique_ptr<CborValue> value;

    CborTagged()
        : tag(0)
        , value(nullptr)
    {
    }

    CborTagged(std::uint64_t t, std::unique_ptr<CborValue> v)
        : tag(t)
        , value(std::move(v))
    {
    }

    // Copy support
    CborTagged(const CborTagged& other);
    CborTagged& operator=(const CborTagged& other);

    // Move support
    CborTagged(CborTagged&&) noexcept = default;
    CborTagged& operator=(CborTagged&&) noexcept = default;

    bool operator<(const CborTagged& other) const
    {
        if (tag != other.tag)
        {
            return tag < other.tag;
        }
        return false;
    }

    bool operator==(const CborTagged& other) const
    {
        return tag == other.tag;
    }
};

/**
 * @brief Variant type representing any CBOR value
 */
class CborValue
{
public:
    using Variant = std::variant<
        std::monostate,
        std::uint64_t,
        std::int64_t,
        CborBytes,
        std::string,
        CborArray,
        CborMap,
        CborTagged,
        SimpleValue,
        double
    >;

    CborValue()
        : data_(std::monostate{})
    {
    }

    explicit CborValue(std::uint64_t v)
        : data_(v)
    {
    }

    explicit CborValue(std::int64_t v)
        : data_(v)
    {
    }

    template <typename T,
              std::enable_if_t<std::is_integral_v<T> &&
                               std::is_signed_v<T> &&
                               !std::is_same_v<T, std::int64_t>, int> = 0>
    explicit CborValue(T v)
        : data_(static_cast<std::int64_t>(v))
    {
    }

    template <typename T,
              std::enable_if_t<std::is_integral_v<T> &&
                               std::is_unsigned_v<T> &&
                               !std::is_same_v<T, std::uint64_t> &&
                               !std::is_same_v<T, bool>, int> = 0>
    explicit CborValue(T v)
        : data_(static_cast<std::uint64_t>(v))
    {
    }

    explicit CborValue(CborBytes v)
        : data_(std::move(v))
    {
    }

    explicit CborValue(std::string v)
        : data_(std::move(v))
    {
    }

    explicit CborValue(const char* v)
        : data_(std::string(v))
    {
    }

    explicit CborValue(CborArray v)
        : data_(std::move(v))
    {
    }

    explicit CborValue(CborMap v)
        : data_(std::move(v))
    {
    }

    explicit CborValue(CborTagged v)
        : data_(std::move(v))
    {
    }

    explicit CborValue(SimpleValue v)
        : data_(v)
    {
    }

    explicit CborValue(double v)
        : data_(v)
    {
    }

    explicit CborValue(float v)
        : data_(static_cast<double>(v))
    {
    }

    explicit CborValue(bool v)
        : data_(v ? SimpleValue::True : SimpleValue::False)
    {
    }

    bool is_null() const
    {
        auto* sv = std::get_if<SimpleValue>(&data_);
        return sv && *sv == SimpleValue::Null;
    }

    bool is_undefined() const
    {
        auto* sv = std::get_if<SimpleValue>(&data_);
        return sv && *sv == SimpleValue::Undefined;
    }

    bool is_bool() const
    {
        auto* sv = std::get_if<SimpleValue>(&data_);
        return sv && (*sv == SimpleValue::True || *sv == SimpleValue::False);
    }

    bool is_unsigned() const
    {
        return std::holds_alternative<std::uint64_t>(data_);
    }

    bool is_signed() const
    {
        return std::holds_alternative<std::int64_t>(data_);
    }

    bool is_integer() const
    {
        return is_unsigned() || is_signed();
    }

    bool is_float() const
    {
        return std::holds_alternative<double>(data_);
    }

    bool is_bytes() const
    {
        return std::holds_alternative<CborBytes>(data_);
    }

    bool is_string() const
    {
        return std::holds_alternative<std::string>(data_);
    }

    bool is_array() const
    {
        return std::holds_alternative<CborArray>(data_);
    }

    bool is_map() const
    {
        return std::holds_alternative<CborMap>(data_);
    }

    bool is_tagged() const
    {
        return std::holds_alternative<CborTagged>(data_);
    }

    bool as_bool() const
    {
        auto* sv = std::get_if<SimpleValue>(&data_);
        if (!sv)
        {
            throw std::runtime_error("CborValue: not a boolean");
        }
        if (*sv == SimpleValue::True)
        {
            return true;
        }
        if (*sv == SimpleValue::False)
        {
            return false;
        }
        throw std::runtime_error("CborValue: not a boolean");
    }

    std::uint64_t as_unsigned() const
    {
        return std::get<std::uint64_t>(data_);
    }

    std::int64_t as_signed() const
    {
        return std::get<std::int64_t>(data_);
    }

    double as_float() const
    {
        return std::get<double>(data_);
    }

    const CborBytes& as_bytes() const
    {
        return std::get<CborBytes>(data_);
    }

    const std::string& as_string() const
    {
        return std::get<std::string>(data_);
    }

    const CborArray& as_array() const
    {
        return std::get<CborArray>(data_);
    }

    const CborMap& as_map() const
    {
        return std::get<CborMap>(data_);
    }

    const CborTagged& as_tagged() const
    {
        return std::get<CborTagged>(data_);
    }

    CborBytes& as_bytes()
    {
        return std::get<CborBytes>(data_);
    }

    std::string& as_string()
    {
        return std::get<std::string>(data_);
    }

    CborArray& as_array()
    {
        return std::get<CborArray>(data_);
    }

    CborMap& as_map()
    {
        return std::get<CborMap>(data_);
    }

    CborTagged& as_tagged()
    {
        return std::get<CborTagged>(data_);
    }

    bool operator<(const CborValue& other) const
    {
        return data_ < other.data_;
    }

    bool operator==(const CborValue& other) const
    {
        return data_ == other.data_;
    }

    const Variant& variant() const
    {
        return data_;
    }

    Variant& variant()
    {
        return data_;
    }

private:
    Variant data_;
};

inline CborTagged::CborTagged(const CborTagged& other)
    : tag(other.tag)
    , value(other.value ? std::make_unique<CborValue>(*other.value) : nullptr)
{
}

inline CborTagged& CborTagged::operator=(const CborTagged& other)
{
    if (this != &other)
    {
        tag = other.tag;
        value = other.value ? std::make_unique<CborValue>(*other.value) : nullptr;
    }
    return *this;
}

// =============================================================================
// Parse Status and Errors
// =============================================================================

enum class ParseStatus
{
    NeedMoreData,
    Done,
    Error
};

enum class ParseError
{
    None,
    UnexpectedEof,
    InvalidInitialByte,
    InvalidAdditionalInfo,
    MaxDepthExceeded,
    MaxStringSizeExceeded,
    MaxTotalSizeExceeded,
    MaxArraySizeExceeded,
    MaxMapSizeExceeded,
    InvalidUtf8,
    IndefiniteLengthNotSupported,
    ReservedAdditionalInfo,
    InvalidSimpleValue,
    InvalidFloatEncoding,
    InternalError
};

inline const char* error_to_string(ParseError err)
{
    switch (err)
    {
        case ParseError::None:
            return "no error";
        case ParseError::UnexpectedEof:
            return "unexpected end of input";
        case ParseError::InvalidInitialByte:
            return "invalid initial byte";
        case ParseError::InvalidAdditionalInfo:
            return "invalid additional info";
        case ParseError::MaxDepthExceeded:
            return "maximum nesting depth exceeded";
        case ParseError::MaxStringSizeExceeded:
            return "maximum string size exceeded";
        case ParseError::MaxTotalSizeExceeded:
            return "maximum total size exceeded";
        case ParseError::MaxArraySizeExceeded:
            return "maximum array size exceeded";
        case ParseError::MaxMapSizeExceeded:
            return "maximum map size exceeded";
        case ParseError::InvalidUtf8:
            return "invalid UTF-8 in text string";
        case ParseError::IndefiniteLengthNotSupported:
            return "indefinite length not supported";
        case ParseError::ReservedAdditionalInfo:
            return "reserved additional info value";
        case ParseError::InvalidSimpleValue:
            return "invalid simple value";
        case ParseError::InvalidFloatEncoding:
            return "invalid float encoding";
        case ParseError::InternalError:
            return "internal parser error";
    }
    return "unknown error";
}

// =============================================================================
// Stream Parser
// =============================================================================

class CborStreamParser
{
public:
    struct Limits
    {
        std::size_t max_depth = 64;
        std::size_t max_string_bytes = 16 * 1024 * 1024;
        std::size_t max_total_bytes = 256 * 1024 * 1024;
        std::size_t max_array_elements = 1024 * 1024;
        std::size_t max_map_pairs = 1024 * 1024;
    };

    struct Stats
    {
        std::size_t bytes_consumed = 0;
        std::size_t current_depth = 0;
        std::size_t max_depth_seen = 0;
        std::size_t values_parsed = 0;
    };

    CborStreamParser() = default;

    explicit CborStreamParser(const Limits& limits)
        : limits_(limits)
    {
    }

    void set_limits(const Limits& limits)
    {
        limits_ = limits;
    }

    const Limits& limits() const
    {
        return limits_;
    }

    void set_max_depth(std::size_t d)
    {
        limits_.max_depth = d;
    }

    void set_max_string_bytes(std::size_t s)
    {
        limits_.max_string_bytes = s;
    }

    void set_max_total_bytes(std::size_t s)
    {
        limits_.max_total_bytes = s;
    }

    void set_max_array_elements(std::size_t n)
    {
        limits_.max_array_elements = n;
    }

    void set_max_map_pairs(std::size_t n)
    {
        limits_.max_map_pairs = n;
    }

    ParseStatus feed(const std::uint8_t* data, std::size_t size)
    {
        for (std::size_t i = 0; i < size; ++i)
        {
            if (stats_.bytes_consumed >= limits_.max_total_bytes)
            {
                error_ = ParseError::MaxTotalSizeExceeded;
                return ParseStatus::Error;
            }

            auto status = process_byte(data[i]);
            ++stats_.bytes_consumed;

            if (status != ParseStatus::NeedMoreData)
            {
                return status;
            }
        }
        return ParseStatus::NeedMoreData;
    }

    template <typename Container>
    ParseStatus feed(const Container& data)
    {
        return feed(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
    }

    ParseError error() const
    {
        return error_;
    }

    const char* error_message() const
    {
        return error_to_string(error_);
    }

    const Stats& stats() const
    {
        return stats_;
    }

    bool is_done() const
    {
        return state_ == State::Done;
    }

    bool has_error() const
    {
        return error_ != ParseError::None;
    }

    const CborValue& result() const
    {
        return root_;
    }

    CborValue take_result()
    {
        return std::move(root_);
    }

    void reset()
    {
        state_ = State::Initial;
        error_ = ParseError::None;
        stats_ = Stats{};
        root_ = CborValue{};
        stack_.clear();
        argument_ = 0;
        arg_bytes_needed_ = 0;
        arg_bytes_read_ = 0;
        content_remaining_ = 0;
        current_major_ = 0;
        current_ai_ = 0;
        byte_buffer_.clear();
        string_buffer_.clear();
        pending_map_key_ = CborValue{};
    }

private:
    enum class State
    {
        Initial,
        ReadingArgument,
        ReadingBytes,
        ReadingText,
        Done,
        Error
    };

    struct Frame
    {
        CborValue* target;
        std::size_t remaining;
        bool is_map;
        bool expecting_value;

        Frame(CborValue* t, std::size_t r, bool m)
            : target(t)
            , remaining(r)
            , is_map(m)
            , expecting_value(false)
        {
        }
    };

    ParseStatus process_byte(std::uint8_t byte)
    {
        switch (state_)
        {
            case State::Initial:
                return process_initial_byte(byte);
            case State::ReadingArgument:
                return process_argument_byte(byte);
            case State::ReadingBytes:
                return process_bytes_content(byte);
            case State::ReadingText:
                return process_text_content(byte);
            case State::Done:
            case State::Error:
                return state_ == State::Done ? ParseStatus::Done : ParseStatus::Error;
        }
        error_ = ParseError::InternalError;
        return ParseStatus::Error;
    }

    ParseStatus process_initial_byte(std::uint8_t byte)
    {
        current_major_ = byte >> 5;
        current_ai_ = byte & 0x1F;

        if (current_ai_ == 31)
        {
            if (current_major_ >= 2 && current_major_ <= 5)
            {
                error_ = ParseError::IndefiniteLengthNotSupported;
                return ParseStatus::Error;
            }
            if (current_major_ == 7)
            {
                error_ = ParseError::InvalidInitialByte;
                return ParseStatus::Error;
            }
        }

        if (current_ai_ >= 28 && current_ai_ <= 30)
        {
            error_ = ParseError::ReservedAdditionalInfo;
            return ParseStatus::Error;
        }

        if (current_ai_ < 24)
        {
            argument_ = current_ai_;
            return process_complete_item();
        }

        switch (current_ai_)
        {
            case 24:
                arg_bytes_needed_ = 1;
                break;
            case 25:
                arg_bytes_needed_ = 2;
                break;
            case 26:
                arg_bytes_needed_ = 4;
                break;
            case 27:
                arg_bytes_needed_ = 8;
                break;
            default:
                error_ = ParseError::InvalidAdditionalInfo;
                return ParseStatus::Error;
        }

        argument_ = 0;
        arg_bytes_read_ = 0;
        state_ = State::ReadingArgument;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus process_argument_byte(std::uint8_t byte)
    {
        argument_ = (argument_ << 8) | byte;
        ++arg_bytes_read_;

        if (arg_bytes_read_ < arg_bytes_needed_)
        {
            return ParseStatus::NeedMoreData;
        }

        return process_complete_item();
    }

    ParseStatus process_complete_item()
    {
        switch (current_major_)
        {
            case 0:
                return emit_value(CborValue(argument_));

            case 1:
                return emit_value(CborValue(static_cast<std::int64_t>(-1) -
                                            static_cast<std::int64_t>(argument_)));

            case 2:
                if (argument_ > limits_.max_string_bytes)
                {
                    error_ = ParseError::MaxStringSizeExceeded;
                    return ParseStatus::Error;
                }
                content_remaining_ = static_cast<std::size_t>(argument_);
                byte_buffer_.clear();
                byte_buffer_.reserve(content_remaining_);
                if (content_remaining_ == 0)
                {
                    return emit_value(CborValue(CborBytes{}));
                }
                state_ = State::ReadingBytes;
                return ParseStatus::NeedMoreData;

            case 3:
                if (argument_ > limits_.max_string_bytes)
                {
                    error_ = ParseError::MaxStringSizeExceeded;
                    return ParseStatus::Error;
                }
                content_remaining_ = static_cast<std::size_t>(argument_);
                string_buffer_.clear();
                string_buffer_.reserve(content_remaining_);
                if (content_remaining_ == 0)
                {
                    return emit_value(CborValue(std::string{}));
                }
                state_ = State::ReadingText;
                return ParseStatus::NeedMoreData;

            case 4:
                return begin_array(static_cast<std::size_t>(argument_));

            case 5:
                return begin_map(static_cast<std::size_t>(argument_));

            case 6:
                return begin_tag(argument_);

            case 7:
                return process_simple_or_float();

            default:
                error_ = ParseError::InvalidInitialByte;
                return ParseStatus::Error;
        }
    }

    ParseStatus process_bytes_content(std::uint8_t byte)
    {
        byte_buffer_.push_back(byte);
        --content_remaining_;

        if (content_remaining_ == 0)
        {
            return emit_value(CborValue(std::move(byte_buffer_)));
        }
        return ParseStatus::NeedMoreData;
    }

    ParseStatus process_text_content(std::uint8_t byte)
    {
        string_buffer_.push_back(static_cast<char>(byte));
        --content_remaining_;

        if (content_remaining_ == 0)
        {
            return emit_value(CborValue(std::move(string_buffer_)));
        }
        return ParseStatus::NeedMoreData;
    }

    ParseStatus process_simple_or_float()
    {
        if (current_ai_ < 24)
        {
            return emit_simple_value(current_ai_);
        }

        switch (current_ai_)
        {
            case 24:
                if (argument_ < 32)
                {
                    error_ = ParseError::InvalidSimpleValue;
                    return ParseStatus::Error;
                }
                return emit_simple_value(static_cast<std::uint8_t>(argument_));

            case 25:
            {
                double val = decode_half(static_cast<std::uint16_t>(argument_));
                return emit_value(CborValue(val));
            }

            case 26:
            {
                float f;
                std::uint32_t bits = static_cast<std::uint32_t>(argument_);
                std::memcpy(&f, &bits, sizeof(f));
                return emit_value(CborValue(static_cast<double>(f)));
            }

            case 27:
            {
                double d;
                std::memcpy(&d, &argument_, sizeof(d));
                return emit_value(CborValue(d));
            }

            default:
                error_ = ParseError::InvalidFloatEncoding;
                return ParseStatus::Error;
        }
    }

    ParseStatus emit_simple_value(std::uint8_t sv)
    {
        switch (sv)
        {
            case 20:
                return emit_value(CborValue(SimpleValue::False));
            case 21:
                return emit_value(CborValue(SimpleValue::True));
            case 22:
                return emit_value(CborValue(SimpleValue::Null));
            case 23:
                return emit_value(CborValue(SimpleValue::Undefined));
            default:
                return emit_value(CborValue(static_cast<SimpleValue>(sv)));
        }
    }

    ParseStatus begin_array(std::size_t count)
    {
        if (count > limits_.max_array_elements)
        {
            error_ = ParseError::MaxArraySizeExceeded;
            return ParseStatus::Error;
        }

        if (stats_.current_depth >= limits_.max_depth)
        {
            error_ = ParseError::MaxDepthExceeded;
            return ParseStatus::Error;
        }

        ++stats_.values_parsed;

        CborValue arr(CborArray{});
        arr.as_array().reserve(count);

        CborValue* target = get_current_target();
        if (target)
        {
            *target = std::move(arr);
        }
        else
        {
            root_ = std::move(arr);
            target = &root_;
        }

        if (count == 0)
        {
            return complete_value();
        }

        stack_.emplace_back(target, count, false);
        ++stats_.current_depth;
        if (stats_.current_depth > stats_.max_depth_seen)
        {
            stats_.max_depth_seen = stats_.current_depth;
        }

        state_ = State::Initial;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus begin_map(std::size_t count)
    {
        if (count > limits_.max_map_pairs)
        {
            error_ = ParseError::MaxMapSizeExceeded;
            return ParseStatus::Error;
        }

        if (stats_.current_depth >= limits_.max_depth)
        {
            error_ = ParseError::MaxDepthExceeded;
            return ParseStatus::Error;
        }

        ++stats_.values_parsed;

        CborValue m(CborMap{});

        CborValue* target = get_current_target();
        if (target)
        {
            *target = std::move(m);
        }
        else
        {
            root_ = std::move(m);
            target = &root_;
        }

        if (count == 0)
        {
            return complete_value();
        }

        stack_.emplace_back(target, count * 2, true);
        ++stats_.current_depth;
        if (stats_.current_depth > stats_.max_depth_seen)
        {
            stats_.max_depth_seen = stats_.current_depth;
        }

        state_ = State::Initial;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus begin_tag(std::uint64_t tag)
    {
        if (stats_.current_depth >= limits_.max_depth)
        {
            error_ = ParseError::MaxDepthExceeded;
            return ParseStatus::Error;
        }

        ++stats_.values_parsed;

        CborTagged tagged;
        tagged.tag = tag;
        tagged.value = std::make_unique<CborValue>();

        CborValue val(std::move(tagged));

        CborValue* target = get_current_target();
        if (target)
        {
            *target = std::move(val);
        }
        else
        {
            root_ = std::move(val);
            target = &root_;
        }

        stack_.emplace_back(target, 1, false);
        ++stats_.current_depth;
        if (stats_.current_depth > stats_.max_depth_seen)
        {
            stats_.max_depth_seen = stats_.current_depth;
        }

        state_ = State::Initial;
        return ParseStatus::NeedMoreData;
    }

    CborValue* get_current_target()
    {
        if (stack_.empty())
        {
            return nullptr;
        }

        Frame& frame = stack_.back();

        if (frame.is_map)
        {
            return nullptr;
        }
        else
        {
            if (frame.target->is_array())
            {
                frame.target->as_array().emplace_back();
                return &frame.target->as_array().back();
            }
            else if (frame.target->is_tagged())
            {
                return frame.target->as_tagged().value.get();
            }
        }
        return nullptr;
    }

    ParseStatus emit_value(CborValue value)
    {
        ++stats_.values_parsed;

        if (stack_.empty())
        {
            root_ = std::move(value);
            state_ = State::Done;
            return ParseStatus::Done;
        }

        Frame& frame = stack_.back();

        if (frame.is_map)
        {
            if (!frame.expecting_value)
            {
                pending_map_key_ = std::move(value);
                frame.expecting_value = true;
                --frame.remaining;
                state_ = State::Initial;
                return ParseStatus::NeedMoreData;
            }
            else
            {
                frame.target->as_map()[std::move(pending_map_key_)] = std::move(value);
                frame.expecting_value = false;
                --frame.remaining;
            }
        }
        else
        {
            if (frame.target->is_array())
            {
                frame.target->as_array().push_back(std::move(value));
            }
            else if (frame.target->is_tagged())
            {
                *frame.target->as_tagged().value = std::move(value);
            }
            --frame.remaining;
        }

        return complete_value();
    }

    ParseStatus complete_value()
    {
        while (!stack_.empty() && stack_.back().remaining == 0)
        {
            stack_.pop_back();
            --stats_.current_depth;

            if (!stack_.empty())
            {
                --stack_.back().remaining;
            }
        }

        if (stack_.empty())
        {
            state_ = State::Done;
            return ParseStatus::Done;
        }

        state_ = State::Initial;
        return ParseStatus::NeedMoreData;
    }

    static double decode_half(std::uint16_t half)
    {
        int sign = (half >> 15) & 1;
        int exp = (half >> 10) & 0x1F;
        int mant = half & 0x3FF;

        double val;
        if (exp == 0)
        {
            val = std::ldexp(static_cast<double>(mant), -24);
        }
        else if (exp == 31)
        {
            val = (mant == 0) ? std::numeric_limits<double>::infinity()
                              : std::numeric_limits<double>::quiet_NaN();
        }
        else
        {
            val = std::ldexp(static_cast<double>(mant + 1024), exp - 25);
        }

        return sign ? -val : val;
    }

    Limits limits_;
    Stats stats_;
    State state_ = State::Initial;
    ParseError error_ = ParseError::None;

    CborValue root_;
    std::vector<Frame> stack_;

    std::uint8_t current_major_ = 0;
    std::uint8_t current_ai_ = 0;
    std::uint64_t argument_ = 0;
    std::size_t arg_bytes_needed_ = 0;
    std::size_t arg_bytes_read_ = 0;

    std::size_t content_remaining_ = 0;
    CborBytes byte_buffer_;
    std::string string_buffer_;

    CborValue pending_map_key_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

inline CborValue parse_cbor(const std::uint8_t* data, std::size_t size)
{
    CborStreamParser parser;
    auto status = parser.feed(data, size);

    if (status == ParseStatus::Error)
    {
        throw std::runtime_error(std::string("CBOR parse error: ") +
                                 parser.error_message());
    }
    if (status == ParseStatus::NeedMoreData)
    {
        throw std::runtime_error("CBOR parse error: incomplete input");
    }

    return parser.take_result();
}

template <typename Container>
inline CborValue parse_cbor(const Container& data)
{
    return parse_cbor(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

inline CborValue parse_cbor_limited(const std::uint8_t* data,
                                    std::size_t size,
                                    const CborStreamParser::Limits& limits)
{
    CborStreamParser parser(limits);
    auto status = parser.feed(data, size);

    if (status == ParseStatus::Error)
    {
        throw std::runtime_error(std::string("CBOR parse error: ") +
                                 parser.error_message());
    }
    if (status == ParseStatus::NeedMoreData)
    {
        throw std::runtime_error("CBOR parse error: incomplete input");
    }

    return parser.take_result();
}

} // namespace cbor_stream

/**
 * @brief Macro to bring CborStreamLite types into local scope
 *
 * @details Brings commonly-used CBOR stream parsing types and functions into
 * local scope without polluting the fat_p root namespace. Safe to use in .cpp
 * files. Avoid using in public headers.
 */
#define FATP_USING_CBOR_STREAM_LITE()                          \
    using fat_p::cbor_stream::CborStreamParser;           \
    using fat_p::cbor_stream::CborValue;                  \
    using fat_p::cbor_stream::CborArray;                  \
    using fat_p::cbor_stream::CborMap;                    \
    using fat_p::cbor_stream::CborBytes;                  \
    using fat_p::cbor_stream::CborTagged;                 \
    using fat_p::cbor_stream::SimpleValue;                \
    using fat_p::cbor_stream::ParseStatus;                \
    using fat_p::cbor_stream::ParseError;                 \
    using fat_p::cbor_stream::parse_cbor;                 \
    using fat_p::cbor_stream::parse_cbor_limited;         \
    using fat_p::cbor_stream::error_to_string

} // namespace fat_p

// Backwards compatibility alias
#ifndef FATP_NO_LEGACY_MACROS
#define USING_CBOR_STREAM_LITE FATP_USING_CBOR_STREAM_LITE
#endif

#pragma once

/*
FATP_META:
  meta_version: 1
  component: CborStreamLite
  file_role: public_header
  path: include/fat_p/CborStreamLite.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for CborStreamLite."
  api_stability: in_work
  related:
    docs_search: "CborStreamLite"
    tests:
      - components/Cbor/tests/test_CborStreamLite.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 1
    defines_unprefixed: 0
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
    ByteString = 2,
    TextString = 3,
    Array = 4,
    Map = 5,
    Tag = 6,
    Simple = 7
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
    False = 20,
    True = 21,
    Null = 22,
    Undefined = 23
};

/**
 * @brief Tagged value wrapper
 */
struct CborTagged
{
    std::uint64_t tag;
    std::unique_ptr<CborValue> value;

    CborTagged();
    CborTagged(std::uint64_t t, std::unique_ptr<CborValue> v);

    // Copy support
    CborTagged(const CborTagged& other);
    CborTagged& operator=(const CborTagged& other);

    // Move support â€” defined after CborValue is complete
    CborTagged(CborTagged&&) noexcept;
    CborTagged& operator=(CborTagged&&) noexcept;

    // Destructor must be defined after CborValue is complete
    ~CborTagged();

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
    using Variant = std::variant<std::monostate,
                                 std::uint64_t,
                                 std::int64_t,
                                 CborBytes,
                                 std::string,
                                 CborArray,
                                 CborMap,
                                 CborTagged,
                                 SimpleValue,
                                 double>;

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

    template <typename T>
        requires (std::is_integral_v<T> && std::is_signed_v<T> && !std::is_same_v<T, std::int64_t>)
    explicit CborValue(T v)
        : data_(static_cast<std::int64_t>(v))
    {
    }

    template <typename T>
        requires (std::is_integral_v<T> && std::is_unsigned_v<T> && !std::is_same_v<T, std::uint64_t> &&
                  !std::is_same_v<T, bool>)
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

inline CborTagged::CborTagged()
    : tag(0)
    , value(nullptr)
{
}

inline CborTagged::CborTagged(std::uint64_t t, std::unique_ptr<CborValue> v)
    : tag(t)
    , value(std::move(v))
{
}

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

inline CborTagged::CborTagged(CborTagged&&) noexcept = default;
inline CborTagged& CborTagged::operator=(CborTagged&&) noexcept = default;
inline CborTagged::~CborTagged() = default;

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
        : mLimits(limits)
    {
    }

    void set_limits(const Limits& limits)
    {
        mLimits = limits;
    }

    const Limits& limits() const
    {
        return mLimits;
    }

    void set_max_depth(std::size_t d)
    {
        mLimits.max_depth = d;
    }

    void set_max_string_bytes(std::size_t s)
    {
        mLimits.max_string_bytes = s;
    }

    void set_max_total_bytes(std::size_t s)
    {
        mLimits.max_total_bytes = s;
    }

    void set_max_array_elements(std::size_t n)
    {
        mLimits.max_array_elements = n;
    }

    void set_max_map_pairs(std::size_t n)
    {
        mLimits.max_map_pairs = n;
    }

    ParseStatus feed(const std::uint8_t* data, std::size_t size)
    {
        for (std::size_t i = 0; i < size; ++i)
        {
            if (mStats.bytes_consumed >= mLimits.max_total_bytes)
            {
                mError = ParseError::MaxTotalSizeExceeded;
                return ParseStatus::Error;
            }

            auto status = process_byte(data[i]);
            ++mStats.bytes_consumed;

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
        return mError;
    }

    const char* error_message() const
    {
        return error_to_string(mError);
    }

    const Stats& stats() const
    {
        return mStats;
    }

    bool is_done() const
    {
        return mState == State::Done;
    }

    bool has_error() const
    {
        return mError != ParseError::None;
    }

    const CborValue& result() const
    {
        return mRoot;
    }

    CborValue take_result()
    {
        return std::move(mRoot);
    }

    void reset()
    {
        mState = State::Initial;
        mError = ParseError::None;
        mStats = Stats{};
        mRoot = CborValue{};
        mStack.clear();
        mArgument = 0;
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
        switch (mState)
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
                return mState == State::Done ? ParseStatus::Done : ParseStatus::Error;
        }
        mError = ParseError::InternalError;
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
                mError = ParseError::IndefiniteLengthNotSupported;
                return ParseStatus::Error;
            }
            if (current_major_ == 7)
            {
                mError = ParseError::InvalidInitialByte;
                return ParseStatus::Error;
            }
        }

        if (current_ai_ >= 28 && current_ai_ <= 30)
        {
            mError = ParseError::ReservedAdditionalInfo;
            return ParseStatus::Error;
        }

        if (current_ai_ < 24)
        {
            mArgument = current_ai_;
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
                mError = ParseError::InvalidAdditionalInfo;
                return ParseStatus::Error;
        }

        mArgument = 0;
        arg_bytes_read_ = 0;
        mState = State::ReadingArgument;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus process_argument_byte(std::uint8_t byte)
    {
        mArgument = (mArgument << 8) | byte;
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
                return emit_value(CborValue(mArgument));

            case 1:
                return emit_value(CborValue(static_cast<std::int64_t>(-1) - static_cast<std::int64_t>(mArgument)));

            case 2:
                if (mArgument > mLimits.max_string_bytes)
                {
                    mError = ParseError::MaxStringSizeExceeded;
                    return ParseStatus::Error;
                }
                content_remaining_ = static_cast<std::size_t>(mArgument);
                byte_buffer_.clear();
                byte_buffer_.reserve(content_remaining_);
                if (content_remaining_ == 0)
                {
                    return emit_value(CborValue(CborBytes{}));
                }
                mState = State::ReadingBytes;
                return ParseStatus::NeedMoreData;

            case 3:
                if (mArgument > mLimits.max_string_bytes)
                {
                    mError = ParseError::MaxStringSizeExceeded;
                    return ParseStatus::Error;
                }
                content_remaining_ = static_cast<std::size_t>(mArgument);
                string_buffer_.clear();
                string_buffer_.reserve(content_remaining_);
                if (content_remaining_ == 0)
                {
                    return emit_value(CborValue(std::string{}));
                }
                mState = State::ReadingText;
                return ParseStatus::NeedMoreData;

            case 4:
                return begin_array(static_cast<std::size_t>(mArgument));

            case 5:
                return begin_map(static_cast<std::size_t>(mArgument));

            case 6:
                return begin_tag(mArgument);

            case 7:
                return process_simple_or_float();

            default:
                mError = ParseError::InvalidInitialByte;
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
                if (mArgument < 32)
                {
                    mError = ParseError::InvalidSimpleValue;
                    return ParseStatus::Error;
                }
                return emit_simple_value(static_cast<std::uint8_t>(mArgument));

            case 25:
            {
                double val = decode_half(static_cast<std::uint16_t>(mArgument));
                return emit_value(CborValue(val));
            }

            case 26:
            {
                float f;
                std::uint32_t bits = static_cast<std::uint32_t>(mArgument);
                std::memcpy(&f, &bits, sizeof(f));
                return emit_value(CborValue(static_cast<double>(f)));
            }

            case 27:
            {
                double d;
                std::memcpy(&d, &mArgument, sizeof(d));
                return emit_value(CborValue(d));
            }

            default:
                mError = ParseError::InvalidFloatEncoding;
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
        if (count > mLimits.max_array_elements)
        {
            mError = ParseError::MaxArraySizeExceeded;
            return ParseStatus::Error;
        }

        if (mStats.current_depth >= mLimits.max_depth)
        {
            mError = ParseError::MaxDepthExceeded;
            return ParseStatus::Error;
        }

        ++mStats.values_parsed;

        CborValue arr(CborArray{});
        arr.as_array().reserve(count);

        CborValue* target = get_current_target();
        if (target)
        {
            *target = std::move(arr);
        }
        else
        {
            mRoot = std::move(arr);
            target = &mRoot;
        }

        if (count == 0)
        {
            return complete_value();
        }

        mStack.emplace_back(target, count, false);
        ++mStats.current_depth;
        if (mStats.current_depth > mStats.max_depth_seen)
        {
            mStats.max_depth_seen = mStats.current_depth;
        }

        mState = State::Initial;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus begin_map(std::size_t count)
    {
        if (count > mLimits.max_map_pairs)
        {
            mError = ParseError::MaxMapSizeExceeded;
            return ParseStatus::Error;
        }

        if (mStats.current_depth >= mLimits.max_depth)
        {
            mError = ParseError::MaxDepthExceeded;
            return ParseStatus::Error;
        }

        ++mStats.values_parsed;

        CborValue m(CborMap{});

        CborValue* target = get_current_target();
        if (target)
        {
            *target = std::move(m);
        }
        else
        {
            mRoot = std::move(m);
            target = &mRoot;
        }

        if (count == 0)
        {
            return complete_value();
        }

        mStack.emplace_back(target, count * 2, true);
        ++mStats.current_depth;
        if (mStats.current_depth > mStats.max_depth_seen)
        {
            mStats.max_depth_seen = mStats.current_depth;
        }

        mState = State::Initial;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus begin_tag(std::uint64_t tag)
    {
        if (mStats.current_depth >= mLimits.max_depth)
        {
            mError = ParseError::MaxDepthExceeded;
            return ParseStatus::Error;
        }

        ++mStats.values_parsed;

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
            mRoot = std::move(val);
            target = &mRoot;
        }

        mStack.emplace_back(target, 1, false);
        ++mStats.current_depth;
        if (mStats.current_depth > mStats.max_depth_seen)
        {
            mStats.max_depth_seen = mStats.current_depth;
        }

        mState = State::Initial;
        return ParseStatus::NeedMoreData;
    }

    CborValue* get_current_target()
    {
        if (mStack.empty())
        {
            return nullptr;
        }

        Frame& frame = mStack.back();

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
        ++mStats.values_parsed;

        if (mStack.empty())
        {
            mRoot = std::move(value);
            mState = State::Done;
            return ParseStatus::Done;
        }

        Frame& frame = mStack.back();

        if (frame.is_map)
        {
            if (!frame.expecting_value)
            {
                pending_map_key_ = std::move(value);
                frame.expecting_value = true;
                --frame.remaining;
                mState = State::Initial;
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
        while (!mStack.empty() && mStack.back().remaining == 0)
        {
            mStack.pop_back();
            --mStats.current_depth;

            if (!mStack.empty())
            {
                --mStack.back().remaining;
            }
        }

        if (mStack.empty())
        {
            mState = State::Done;
            return ParseStatus::Done;
        }

        mState = State::Initial;
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
            val = (mant == 0) ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN();
        }
        else
        {
            val = std::ldexp(static_cast<double>(mant + 1024), exp - 25);
        }

        return sign ? -val : val;
    }

    Limits mLimits;
    Stats mStats;
    State mState = State::Initial;
    ParseError mError = ParseError::None;

    CborValue mRoot;
    std::vector<Frame> mStack;

    std::uint8_t current_major_ = 0;
    std::uint8_t current_ai_ = 0;
    std::uint64_t mArgument = 0;
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
        throw std::runtime_error(std::string("CBOR parse error: ") + parser.error_message());
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

inline CborValue parse_cbor_limited(const std::uint8_t* data, std::size_t size, const CborStreamParser::Limits& limits)
{
    CborStreamParser parser(limits);
    auto status = parser.feed(data, size);

    if (status == ParseStatus::Error)
    {
        throw std::runtime_error(std::string("CBOR parse error: ") + parser.error_message());
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
#define FATP_USING_CBOR_STREAM_LITE()             \
    using fat_p::cbor_stream::CborStreamParser;   \
    using fat_p::cbor_stream::CborValue;          \
    using fat_p::cbor_stream::CborArray;          \
    using fat_p::cbor_stream::CborMap;            \
    using fat_p::cbor_stream::CborBytes;          \
    using fat_p::cbor_stream::CborTagged;         \
    using fat_p::cbor_stream::SimpleValue;        \
    using fat_p::cbor_stream::ParseStatus;        \
    using fat_p::cbor_stream::ParseError;         \
    using fat_p::cbor_stream::parse_cbor;         \
    using fat_p::cbor_stream::parse_cbor_limited; \
    using fat_p::cbor_stream::error_to_string

} // namespace fat_p

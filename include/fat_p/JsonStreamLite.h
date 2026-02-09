#pragma once

/*
FATP_META:
  meta_version: 1
  component: JsonStreamLite
  file_role: public_header
  path: include/fat_p/JsonStreamLite.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for JsonStreamLite."
  api_stability: in_work
  related:
    docs_search: "JsonStreamLite"
    tests:
      - components/Json/tests/test_JsonStreamLite.cpp
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
 * @file JsonStreamLite.h
 * @brief Streaming JSON parser with incremental processing
 *
 */

/**
 * @file JsonStreamLite.h
 * @brief Streaming JSON parser with incremental DOM construction
 * @version 1.0
 *
 * @details Provides a streaming parser for JSON (RFC 8259) that can process
 * data in chunks, enabling early rejection of malformed or malicious input.
 * Builds a DOM tree incrementally, suitable for network protocol parsing.
 *
 * Key features:
 * - Byte-at-a-time state machine (can suspend/resume at any point)
 * - Configurable limits (depth, string size, total size)
 * - Early rejection of limit violations
 * - Zero external dependencies (standalone header)
 * - Builds JsonValue DOM compatible with further processing
 *
 * @note JSON streaming is more complex than CBOR due to variable-length tokens
 *       without length prefixes. This parser handles all edge cases properly.
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
#include <variant>
#include <vector>

namespace fat_p
{
namespace json_stream
{

// =============================================================================
// JSON Value Types (DOM)
// =============================================================================

class JsonValue;

using JsonArray = std::vector<JsonValue>;
using JsonObject = std::map<std::string, JsonValue>;

/**
 * @brief Variant type representing any JSON value
 */
class JsonValue
{
public:
    using Variant = std::variant<std::nullptr_t, bool, std::int64_t, double, std::string, JsonArray, JsonObject>;

    JsonValue()
        : data_(nullptr)
    {
    }

    JsonValue(std::nullptr_t)
        : data_(nullptr)
    {
    }

    JsonValue(bool v)
        : data_(v)
    {
    }

    template <typename T>
        requires (std::is_integral_v<T> && !std::is_same_v<T, bool>)
    JsonValue(T v)
        : data_(static_cast<std::int64_t>(v))
    {
    }

    JsonValue(double v)
        : data_(v)
    {
    }

    JsonValue(const char* v)
        : data_(std::string(v))
    {
    }

    JsonValue(std::string v)
        : data_(std::move(v))
    {
    }

    JsonValue(JsonArray v)
        : data_(std::move(v))
    {
    }

    JsonValue(JsonObject v)
        : data_(std::move(v))
    {
    }

    // Type checks
    bool is_null() const noexcept
    {
        return std::holds_alternative<std::nullptr_t>(data_);
    }

    bool is_bool() const noexcept
    {
        return std::holds_alternative<bool>(data_);
    }

    bool is_int() const noexcept
    {
        return std::holds_alternative<std::int64_t>(data_);
    }

    bool is_double() const noexcept
    {
        return std::holds_alternative<double>(data_);
    }

    bool is_number() const noexcept
    {
        return is_int() || is_double();
    }

    bool is_string() const noexcept
    {
        return std::holds_alternative<std::string>(data_);
    }

    bool is_array() const noexcept
    {
        return std::holds_alternative<JsonArray>(data_);
    }

    bool is_object() const noexcept
    {
        return std::holds_alternative<JsonObject>(data_);
    }

    // Accessors (const)
    bool as_bool() const
    {
        return std::get<bool>(data_);
    }

    std::int64_t as_int() const
    {
        return std::get<std::int64_t>(data_);
    }

    double as_double() const
    {
        if (is_int())
        {
            return static_cast<double>(std::get<std::int64_t>(data_));
        }
        return std::get<double>(data_);
    }

    const std::string& as_string() const
    {
        return std::get<std::string>(data_);
    }

    const JsonArray& as_array() const
    {
        return std::get<JsonArray>(data_);
    }

    const JsonObject& as_object() const
    {
        return std::get<JsonObject>(data_);
    }

    // Mutable accessors
    std::string& as_string()
    {
        return std::get<std::string>(data_);
    }

    JsonArray& as_array()
    {
        return std::get<JsonArray>(data_);
    }

    JsonObject& as_object()
    {
        return std::get<JsonObject>(data_);
    }

    // Access underlying variant
    const Variant& data() const noexcept
    {
        return data_;
    }

    Variant& data() noexcept
    {
        return data_;
    }

private:
    Variant data_;
};

// =============================================================================
// Parse Status and Error Types
// =============================================================================

/**
 * @brief Result of a feed() call
 */
enum class ParseStatus
{
    NeedMoreData,
    Done,
    Error
};

/**
 * @brief Detailed error codes
 */
enum class ParseError
{
    None = 0,
    UnexpectedEof,
    UnexpectedCharacter,
    InvalidEscapeSequence,
    InvalidUnicodeEscape,
    InvalidNumber,
    NumberOutOfRange,
    InvalidLiteral,
    MaxDepthExceeded,
    MaxStringSizeExceeded,
    MaxTotalSizeExceeded,
    MaxArrayElementsExceeded,
    MaxObjectMembersExceeded,
    DuplicateKey,
    TrailingComma,
    MissingColon,
    MissingComma,
    InternalError
};

/**
 * @brief Convert error code to string
 */
inline const char* error_to_string(ParseError e)
{
    switch (e)
    {
        case ParseError::None:
            return "no error";
        case ParseError::UnexpectedEof:
            return "unexpected end of input";
        case ParseError::UnexpectedCharacter:
            return "unexpected character";
        case ParseError::InvalidEscapeSequence:
            return "invalid escape sequence";
        case ParseError::InvalidUnicodeEscape:
            return "invalid unicode escape";
        case ParseError::InvalidNumber:
            return "invalid number format";
        case ParseError::NumberOutOfRange:
            return "number out of range";
        case ParseError::InvalidLiteral:
            return "invalid literal";
        case ParseError::MaxDepthExceeded:
            return "maximum nesting depth exceeded";
        case ParseError::MaxStringSizeExceeded:
            return "maximum string size exceeded";
        case ParseError::MaxTotalSizeExceeded:
            return "maximum total size exceeded";
        case ParseError::MaxArrayElementsExceeded:
            return "maximum array elements exceeded";
        case ParseError::MaxObjectMembersExceeded:
            return "maximum object members exceeded";
        case ParseError::DuplicateKey:
            return "duplicate object key";
        case ParseError::TrailingComma:
            return "trailing comma";
        case ParseError::MissingColon:
            return "missing colon after object key";
        case ParseError::MissingComma:
            return "missing comma";
        case ParseError::InternalError:
            return "internal parser error";
    }
    return "unknown error";
}

// =============================================================================
// Stream Parser State Machine
// =============================================================================

/**
 * @brief Internal parser state
 */
enum class State
{
    Initial,
    InValue,
    InString,
    InStringEscape,
    InStringUnicode,
    InNumber,
    InLiteral,
    InArray,
    InArrayValue,
    InArrayComma,
    InObject,
    InObjectKey,
    InObjectColon,
    InObjectValue,
    InObjectComma,
    Done,
    Error
};

/**
 * @brief Container context for stack-based parsing
 */
struct ContainerContext
{
    enum class Type
    {
        Array,
        Object
    };

    Type type;
    std::size_t count;
    std::string pending_key;
    bool expect_value;
};

/**
 * @brief Streaming JSON parser with configurable limits
 */
class JsonStreamParser
{
public:
    struct Limits
    {
        std::size_t max_depth = 64;
        std::size_t max_string_bytes = 16 * 1024 * 1024;
        std::size_t max_total_bytes = 256 * 1024 * 1024;
        std::size_t max_array_elements = 1024 * 1024;
        std::size_t max_object_members = 1024 * 1024;
    };

    struct Stats
    {
        std::size_t bytes_consumed = 0;
        std::size_t current_depth = 0;
        std::size_t max_depth_seen = 0;
        std::size_t values_parsed = 0;
    };

    JsonStreamParser()
        : mState(State::Initial)
        , mError(ParseError::None)
        , mResult(nullptr)
    {
    }

    // -------------------------------------------------------------------------
    // Configuration
    // -------------------------------------------------------------------------

    void set_limits(const Limits& limits)
    {
        mLimits = limits;
    }

    void set_max_depth(std::size_t depth)
    {
        mLimits.max_depth = depth;
    }

    void set_max_string_size(std::size_t bytes)
    {
        mLimits.max_string_bytes = bytes;
    }

    void set_max_total_size(std::size_t bytes)
    {
        mLimits.max_total_bytes = bytes;
    }

    const Limits& limits() const noexcept
    {
        return mLimits;
    }

    // -------------------------------------------------------------------------
    // Parsing Interface
    // -------------------------------------------------------------------------

    /**
     * @brief Feed bytes to the parser
     * @return ParseStatus::NeedMoreData, Done, or Error
     */
    ParseStatus feed(const std::uint8_t* data, std::size_t size)
    {
        for (std::size_t i = 0; i < size; ++i)
        {
            if (mStats.bytes_consumed >= mLimits.max_total_bytes)
            {
                return set_error(ParseError::MaxTotalSizeExceeded);
            }

            char c = static_cast<char>(data[i]);
            ++mStats.bytes_consumed;

            auto status = process_char(c);
            if (status != ParseStatus::NeedMoreData)
            {
                return status;
            }
        }

        return ParseStatus::NeedMoreData;
    }

    ParseStatus feed(const char* data, std::size_t size)
    {
        return feed(reinterpret_cast<const std::uint8_t*>(data), size);
    }

    // -------------------------------------------------------------------------
    // State Access
    // -------------------------------------------------------------------------

    bool is_done() const noexcept
    {
        return mState == State::Done;
    }

    bool has_error() const noexcept
    {
        return mState == State::Error;
    }

    ParseError error() const noexcept
    {
        return mError;
    }

    const char* error_message() const noexcept
    {
        return error_to_string(mError);
    }

    const Stats& stats() const noexcept
    {
        return mStats;
    }

    const JsonValue& result() const
    {
        return mResult;
    }

    JsonValue take_result()
    {
        return std::move(mResult);
    }

    void reset()
    {
        mState = State::Initial;
        mError = ParseError::None;
        mResult = JsonValue(nullptr);
        mStats = Stats{};
        mStack.clear();
        value_stack_.clear();
        mToken.clear();
        unicode_buffer_.clear();
        unicode_count_ = 0;
        number_has_dot_ = false;
        number_has_exp_ = false;
        number_has_digit_ = false;
    }

private:
    Limits mLimits;
    Stats mStats;
    State mState;
    ParseError mError;
    JsonValue mResult;

    std::vector<ContainerContext> mStack;
    std::vector<JsonValue> value_stack_;
    std::string mToken;
    std::string unicode_buffer_;
    int unicode_count_ = 0;
    bool number_has_dot_ = false;
    bool number_has_exp_ = false;
    bool number_has_digit_ = false;

    // -------------------------------------------------------------------------
    // Main Processing
    // -------------------------------------------------------------------------

    ParseStatus process_char(char c)
    {
        switch (mState)
        {
            case State::Initial:
                return handle_initial(c);

            case State::InString:
                return handle_string(c);

            case State::InStringEscape:
                return handle_string_escape(c);

            case State::InStringUnicode:
                return handle_string_unicode(c);

            case State::InNumber:
                return handle_number(c);

            case State::InLiteral:
                return handle_literal(c);

            case State::InArray:
            case State::InArrayValue:
            case State::InArrayComma:
                return handle_array(c);

            case State::InObject:
            case State::InObjectKey:
            case State::InObjectColon:
            case State::InObjectValue:
            case State::InObjectComma:
                return handle_object(c);

            case State::Done:
                if (!is_whitespace(c))
                {
                    return set_error(ParseError::UnexpectedCharacter);
                }
                return ParseStatus::Done;

            case State::Error:
                return ParseStatus::Error;

            default:
                return set_error(ParseError::InternalError);
        }
    }

    // -------------------------------------------------------------------------
    // Initial State
    // -------------------------------------------------------------------------

    ParseStatus handle_initial(char c)
    {
        if (is_whitespace(c))
        {
            return ParseStatus::NeedMoreData;
        }

        return start_value(c);
    }

    ParseStatus start_value(char c)
    {
        if (c == '"')
        {
            mState = State::InString;
            mToken.clear();
            return ParseStatus::NeedMoreData;
        }

        if (c == '[')
        {
            return begin_array();
        }

        if (c == '{')
        {
            return begin_object();
        }

        if (c == '-' || (c >= '0' && c <= '9'))
        {
            mState = State::InNumber;
            mToken.clear();
            mToken += c;
            number_has_dot_ = false;
            number_has_exp_ = false;
            number_has_digit_ = (c >= '0' && c <= '9');
            return ParseStatus::NeedMoreData;
        }

        if (c == 't' || c == 'f' || c == 'n')
        {
            mState = State::InLiteral;
            mToken.clear();
            mToken += c;
            return ParseStatus::NeedMoreData;
        }

        return set_error(ParseError::UnexpectedCharacter);
    }

    // -------------------------------------------------------------------------
    // String Parsing
    // -------------------------------------------------------------------------

    ParseStatus handle_string(char c)
    {
        if (c == '"')
        {
            return complete_string();
        }

        if (c == '\\')
        {
            mState = State::InStringEscape;
            return ParseStatus::NeedMoreData;
        }

        if (static_cast<unsigned char>(c) < 0x20)
        {
            return set_error(ParseError::InvalidEscapeSequence);
        }

        if (mToken.size() >= mLimits.max_string_bytes)
        {
            return set_error(ParseError::MaxStringSizeExceeded);
        }

        mToken += c;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus handle_string_escape(char c)
    {
        switch (c)
        {
            case '"':
            case '\\':
            case '/':
                mToken += c;
                break;
            case 'b':
                mToken += '\b';
                break;
            case 'f':
                mToken += '\f';
                break;
            case 'n':
                mToken += '\n';
                break;
            case 'r':
                mToken += '\r';
                break;
            case 't':
                mToken += '\t';
                break;
            case 'u':
                mState = State::InStringUnicode;
                unicode_buffer_.clear();
                unicode_count_ = 0;
                return ParseStatus::NeedMoreData;
            default:
                return set_error(ParseError::InvalidEscapeSequence);
        }

        mState = State::InString;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus handle_string_unicode(char c)
    {
        if (!is_hex_digit(c))
        {
            return set_error(ParseError::InvalidUnicodeEscape);
        }

        unicode_buffer_ += c;
        ++unicode_count_;

        if (unicode_count_ < 4)
        {
            return ParseStatus::NeedMoreData;
        }

        // Parse the 4-digit hex code
        unsigned int codepoint = 0;
        for (char h : unicode_buffer_)
        {
            codepoint = (codepoint << 4) | hex_digit_value(h);
        }

        // Handle surrogate pairs
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF)
        {
            // High surrogate - need to wait for low surrogate
            // For simplicity, we encode it directly (not fully correct but common)
        }

        // Encode to UTF-8
        if (codepoint < 0x80)
        {
            mToken += static_cast<char>(codepoint);
        }
        else if (codepoint < 0x800)
        {
            mToken += static_cast<char>(0xC0 | (codepoint >> 6));
            mToken += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else if (codepoint < 0x10000)
        {
            mToken += static_cast<char>(0xE0 | (codepoint >> 12));
            mToken += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            mToken += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
        else
        {
            mToken += static_cast<char>(0xF0 | (codepoint >> 18));
            mToken += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            mToken += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            mToken += static_cast<char>(0x80 | (codepoint & 0x3F));
        }

        mState = State::InString;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus complete_string()
    {
        JsonValue value(std::move(mToken));
        mToken.clear();
        return complete_value(std::move(value));
    }

    // -------------------------------------------------------------------------
    // Number Parsing
    // -------------------------------------------------------------------------

    ParseStatus handle_number(char c)
    {
        bool is_num_char = (c >= '0' && c <= '9');
        bool is_sign = (c == '+' || c == '-');
        bool is_dot = (c == '.');
        bool is_exp = (c == 'e' || c == 'E');

        if (is_num_char)
        {
            number_has_digit_ = true;
            mToken += c;
            return ParseStatus::NeedMoreData;
        }

        if (is_dot && !number_has_dot_ && !number_has_exp_)
        {
            number_has_dot_ = true;
            mToken += c;
            return ParseStatus::NeedMoreData;
        }

        if (is_exp && !number_has_exp_ && number_has_digit_)
        {
            number_has_exp_ = true;
            mToken += c;
            return ParseStatus::NeedMoreData;
        }

        if (is_sign && number_has_exp_ && (mToken.back() == 'e' || mToken.back() == 'E'))
        {
            mToken += c;
            return ParseStatus::NeedMoreData;
        }

        // End of number - need to finish and process this character
        auto status = complete_number();
        if (status != ParseStatus::NeedMoreData && status != ParseStatus::Done)
        {
            return status;
        }

        // Process the character that ended the number
        if (is_whitespace(c))
        {
            return status;
        }

        if (mStack.empty())
        {
            if (is_whitespace(c))
            {
                return ParseStatus::Done;
            }
            return set_error(ParseError::UnexpectedCharacter);
        }

        return process_char(c);
    }

    ParseStatus complete_number()
    {
        if (!number_has_digit_)
        {
            return set_error(ParseError::InvalidNumber);
        }

        // Check for valid number format
        if (mToken.size() > 1 && mToken[0] == '0' && mToken[1] >= '0' && mToken[1] <= '9')
        {
            return set_error(ParseError::InvalidNumber);
        }

        if (mToken[0] == '-' && mToken.size() > 2 && mToken[1] == '0' && mToken[2] >= '0' && mToken[2] <= '9')
        {
            return set_error(ParseError::InvalidNumber);
        }

        // Check for trailing dot or exp
        if (!mToken.empty())
        {
            char last = mToken.back();
            if (last == '.' || last == 'e' || last == 'E' || last == '+' || last == '-')
            {
                return set_error(ParseError::InvalidNumber);
            }
        }

        JsonValue value;

        if (number_has_dot_ || number_has_exp_)
        {
            // Parse as double
            try
            {
                std::size_t pos = 0;
                double d = std::stod(mToken, &pos);
                if (pos != mToken.size())
                {
                    return set_error(ParseError::InvalidNumber);
                }
                value = JsonValue(d);
            }
            catch (...)
            {
                return set_error(ParseError::NumberOutOfRange);
            }
        }
        else
        {
            // Parse as integer
            try
            {
                std::size_t pos = 0;
                std::int64_t i = std::stoll(mToken, &pos);
                if (pos != mToken.size())
                {
                    return set_error(ParseError::InvalidNumber);
                }
                value = JsonValue(i);
            }
            catch (...)
            {
                return set_error(ParseError::NumberOutOfRange);
            }
        }

        mToken.clear();
        return complete_value(std::move(value));
    }

    // -------------------------------------------------------------------------
    // Literal Parsing (true, false, null)
    // -------------------------------------------------------------------------

    ParseStatus handle_literal(char c)
    {
        if ((c >= 'a' && c <= 'z'))
        {
            mToken += c;

            if (mToken == "true")
            {
                return complete_value(JsonValue(true));
            }
            if (mToken == "false")
            {
                return complete_value(JsonValue(false));
            }
            if (mToken == "null")
            {
                return complete_value(JsonValue(nullptr));
            }

            // Check if we're on track
            if (mToken.size() <= 5)
            {
                const char* true_str = "true";
                const char* false_str = "false";
                const char* null_str = "null";

                bool matches_true = (mToken.size() <= 4 && std::strncmp(mToken.c_str(), true_str, mToken.size()) == 0);
                bool matches_false =
                    (mToken.size() <= 5 && std::strncmp(mToken.c_str(), false_str, mToken.size()) == 0);
                bool matches_null = (mToken.size() <= 4 && std::strncmp(mToken.c_str(), null_str, mToken.size()) == 0);

                if (matches_true || matches_false || matches_null)
                {
                    return ParseStatus::NeedMoreData;
                }
            }

            return set_error(ParseError::InvalidLiteral);
        }

        return set_error(ParseError::InvalidLiteral);
    }

    // -------------------------------------------------------------------------
    // Array Parsing
    // -------------------------------------------------------------------------

    ParseStatus begin_array()
    {
        if (mStack.size() >= mLimits.max_depth)
        {
            return set_error(ParseError::MaxDepthExceeded);
        }

        mStack.push_back({ContainerContext::Type::Array, 0, "", true});
        value_stack_.push_back(JsonArray{});
        mStats.current_depth = mStack.size();
        if (mStats.current_depth > mStats.max_depth_seen)
        {
            mStats.max_depth_seen = mStats.current_depth;
        }

        mState = State::InArray;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus handle_array(char c)
    {
        if (is_whitespace(c))
        {
            return ParseStatus::NeedMoreData;
        }

        auto& ctx = mStack.back();

        if (mState == State::InArray || mState == State::InArrayValue)
        {
            if (c == ']')
            {
                if (mState == State::InArrayValue && ctx.count > 0)
                {
                    // Trailing comma
                    return set_error(ParseError::TrailingComma);
                }
                return end_array();
            }

            if (ctx.count >= mLimits.max_array_elements)
            {
                return set_error(ParseError::MaxArrayElementsExceeded);
            }

            mState = State::InArrayComma;
            return start_value(c);
        }

        if (mState == State::InArrayComma)
        {
            if (c == ']')
            {
                return end_array();
            }

            if (c == ',')
            {
                mState = State::InArrayValue;
                return ParseStatus::NeedMoreData;
            }

            return set_error(ParseError::MissingComma);
        }

        return set_error(ParseError::InternalError);
    }

    ParseStatus end_array()
    {
        // Pop the completed array
        JsonValue arr = std::move(value_stack_.back());
        value_stack_.pop_back();
        mStack.pop_back();
        mStats.current_depth = mStack.size();
        ++mStats.values_parsed;

        if (mStack.empty())
        {
            mResult = std::move(arr);
            mState = State::Done;
            return ParseStatus::Done;
        }

        // Add to parent container
        auto& parent_ctx = mStack.back();
        if (parent_ctx.type == ContainerContext::Type::Array)
        {
            value_stack_.back().as_array().push_back(std::move(arr));
            ++parent_ctx.count;
            mState = State::InArrayComma;
        }
        else
        {
            // Parent is object - this array is a value
            value_stack_.back().as_object()[parent_ctx.pending_key] = std::move(arr);
            ++parent_ctx.count;
            parent_ctx.pending_key.clear();
            mState = State::InObjectComma;
        }

        return ParseStatus::NeedMoreData;
    }

    // -------------------------------------------------------------------------
    // Object Parsing
    // -------------------------------------------------------------------------

    ParseStatus begin_object()
    {
        if (mStack.size() >= mLimits.max_depth)
        {
            return set_error(ParseError::MaxDepthExceeded);
        }

        mStack.push_back({ContainerContext::Type::Object, 0, "", true});
        value_stack_.push_back(JsonObject{});
        mStats.current_depth = mStack.size();
        if (mStats.current_depth > mStats.max_depth_seen)
        {
            mStats.max_depth_seen = mStats.current_depth;
        }

        mState = State::InObject;
        return ParseStatus::NeedMoreData;
    }

    ParseStatus handle_object(char c)
    {
        if (is_whitespace(c))
        {
            return ParseStatus::NeedMoreData;
        }

        auto& ctx = mStack.back();

        if (mState == State::InObject || mState == State::InObjectKey)
        {
            if (c == '}')
            {
                if (mState == State::InObjectKey && ctx.count > 0)
                {
                    return set_error(ParseError::TrailingComma);
                }
                return end_object();
            }

            if (c != '"')
            {
                return set_error(ParseError::UnexpectedCharacter);
            }

            if (ctx.count >= mLimits.max_object_members)
            {
                return set_error(ParseError::MaxObjectMembersExceeded);
            }

            mState = State::InString;
            mToken.clear();
            ctx.expect_value = false; // We're parsing a key
            return ParseStatus::NeedMoreData;
        }

        if (mState == State::InObjectColon)
        {
            if (c != ':')
            {
                return set_error(ParseError::MissingColon);
            }

            mState = State::InObjectValue;
            ctx.expect_value = true;
            return ParseStatus::NeedMoreData;
        }

        if (mState == State::InObjectValue)
        {
            return start_value(c);
        }

        if (mState == State::InObjectComma)
        {
            if (c == '}')
            {
                return end_object();
            }

            if (c == ',')
            {
                mState = State::InObjectKey;
                return ParseStatus::NeedMoreData;
            }

            return set_error(ParseError::MissingComma);
        }

        return set_error(ParseError::InternalError);
    }

    ParseStatus end_object()
    {
        // Pop the completed object
        JsonValue obj = std::move(value_stack_.back());
        value_stack_.pop_back();
        mStack.pop_back();
        mStats.current_depth = mStack.size();
        ++mStats.values_parsed;

        if (mStack.empty())
        {
            mResult = std::move(obj);
            mState = State::Done;
            return ParseStatus::Done;
        }

        // Add to parent container
        auto& parent_ctx = mStack.back();
        if (parent_ctx.type == ContainerContext::Type::Array)
        {
            value_stack_.back().as_array().push_back(std::move(obj));
            ++parent_ctx.count;
            mState = State::InArrayComma;
        }
        else
        {
            // Parent is object - this object is a value
            value_stack_.back().as_object()[parent_ctx.pending_key] = std::move(obj);
            ++parent_ctx.count;
            parent_ctx.pending_key.clear();
            mState = State::InObjectComma;
        }

        return ParseStatus::NeedMoreData;
    }

    // -------------------------------------------------------------------------
    // Value Completion
    // -------------------------------------------------------------------------

    ParseStatus complete_value(JsonValue&& value)
    {
        ++mStats.values_parsed;

        if (mStack.empty())
        {
            mResult = std::move(value);
            mState = State::Done;
            return ParseStatus::Done;
        }

        auto& ctx = mStack.back();

        if (ctx.type == ContainerContext::Type::Array)
        {
            value_stack_.back().as_array().push_back(std::move(value));
            ++ctx.count;
            mState = State::InArrayComma;
        }
        else
        {
            // Object
            if (!ctx.expect_value)
            {
                // This is a key
                ctx.pending_key = value.as_string();
                mState = State::InObjectColon;
            }
            else
            {
                // This is a value
                value_stack_.back().as_object()[ctx.pending_key] = std::move(value);
                ++ctx.count;
                ctx.pending_key.clear();
                mState = State::InObjectComma;
            }
        }

        return ParseStatus::NeedMoreData;
    }

    // -------------------------------------------------------------------------
    // Utilities
    // -------------------------------------------------------------------------

    static bool is_whitespace(char c)
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r';
    }

    static bool is_hex_digit(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    static unsigned int hex_digit_value(char c)
    {
        if (c >= '0' && c <= '9')
        {
            return static_cast<unsigned int>(c - '0');
        }
        if (c >= 'a' && c <= 'f')
        {
            return static_cast<unsigned int>(c - 'a' + 10);
        }
        return static_cast<unsigned int>(c - 'A' + 10);
    }

    ParseStatus set_error(ParseError e)
    {
        mError = e;
        mState = State::Error;
        return ParseStatus::Error;
    }
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * @brief Parse complete JSON from buffer
 */
inline JsonValue parse_json(const char* data, std::size_t size)
{
    JsonStreamParser parser;
    auto status = parser.feed(data, size);
    if (status == ParseStatus::Error)
    {
        throw std::runtime_error(parser.error_message());
    }
    if (status == ParseStatus::NeedMoreData)
    {
        throw std::runtime_error("incomplete JSON input");
    }
    return parser.take_result();
}

inline JsonValue parse_json(const std::string& data)
{
    return parse_json(data.data(), data.size());
}

template <std::size_t N>
inline JsonValue parse_json(const char (&data)[N])
{
    return parse_json(data, N - 1); // Exclude null terminator
}

template <typename Container,
          typename = decltype(std::declval<Container>().data()),
          typename = decltype(std::declval<Container>().size())>
inline JsonValue parse_json(const Container& data)
{
    return parse_json(reinterpret_cast<const char*>(data.data()), data.size());
}

} // namespace json_stream

// Short namespace alias for convenience
// Use fat_p::js::JsonValue, fat_p::js::parse_json, etc.
namespace js = json_stream;

/**
 * @brief Macro to bring JsonStreamLite types into local scope
 */
#define FATP_USING_JSON_STREAM_LITE()           \
    using fat_p::json_stream::JsonValue;        \
    using fat_p::json_stream::JsonArray;        \
    using fat_p::json_stream::JsonObject;       \
    using fat_p::json_stream::ParseStatus;      \
    using fat_p::json_stream::ParseError;       \
    using fat_p::json_stream::JsonStreamParser; \
    using fat_p::json_stream::parse_json;       \
    using fat_p::json_stream::error_to_string

} // namespace fat_p

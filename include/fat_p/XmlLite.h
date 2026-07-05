#pragma once

/*
FATP_META:
  meta_version: 1
  component: XmlLite
  file_role: public_header
  path: include/fat_p/XmlLite.h
  namespace: fat_p
  layer: Domain
  summary: "Public header for XmlLite."
  api_stability: in_work
  related:
    docs_search: "XmlLite"
    tests:
      - components/Xml/tests/test_XmlLite.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 55
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file XmlLite.h
 * @brief Lightweight XML library for C++ configuration and parameter management
 *
 * @section overview Overview
 * XmlLite is a C++20 header-only XML parser designed for application
 * configuration files and structured data deserialization. It follows
 * the same design philosophy as JsonLite: zero external dependencies,
 * policy-based error handling, and macro-based struct deserialization.
 *
 * This parser handles elements, attributes, text content, nested structures,
 * and repeated elements. XML namespaces are rejected at parse time (prefixed
 * names and xmlns attributes throw). It does not handle DTDs, CDATA sections,
 * processing instructions, or entity references beyond the five predefined
 * XML entities (&amp; &lt; &gt; &apos; &quot;).
 *
 * Mixed content (text interleaved with child elements) is not preserved in
 * document order. Adjacent text chunks are trimmed and joined with spaces in
 * the parent element's text field; children are stored separately. This is
 * intentional for config XML where elements hold either text or children, not
 * both in sequence.
 *
 * Enum class types deserialize from integer element text via the underlying
 * type (e.g. `<mode>2</mode>`), or from string tokens when
 * `FATP_XML_ENUM_STRING_POLICY` is defined (e.g. `<mode>On</mode>`).
 * Enums with a string policy require string tokens; numeric fallback applies
 * only to enums without `FATP_XML_ENUM_STRING_POLICY`. Integer enum
 * deserialization checks underlying-type range only, not declared enumerator
 * membership — use a string policy for strict token validation.
 *
 * Scalar `from_xml` requires leaf elements (no child elements). Repeated
 * child elements use `from_xml(parent, childTag, vector)` or `xml_all`.
 * `FATP_XML_DEFINE_TYPE` maps one child element per scalar/nested field;
 * vector fields are not supported by the macro.
 *
 * Value-returning `from_xml<T>(node)` requires `T` to be default-constructible.
 *
 * @section example Basic Example
 * @code{.cpp}
 * #include "XmlLite.h"
 *
 * struct Target {
 *     double centerX;
 *     double centerY;
 *     double sigma;
 * };
 * FATP_XML_DEFINE_TYPE(Target, centerX, centerY, sigma)
 *
 * int main() {
 *     auto root = fat_p::parse_xml_file("config.xml");
 *     auto targets = fat_p::xml_all<Target>(root, "targets", "target");
 *     return 0;
 * }
 * @endcode
 */

#include <algorithm>
#include <charconv>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <sstream>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fat_p
{

namespace xml_detail
{

using SourceLocation = std::source_location;

[[nodiscard]] inline bool isXmlWhitespace(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r';
}

[[nodiscard]] inline bool isAsciiAlpha(char ch)
{
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
}

[[nodiscard]] inline bool isAsciiDigit(char ch)
{
    return ch >= '0' && ch <= '9';
}

inline std::string_view trim(std::string_view sv)
{
    while (!sv.empty() && isXmlWhitespace(sv.front())) sv.remove_prefix(1);
    while (!sv.empty() && isXmlWhitespace(sv.back())) sv.remove_suffix(1);
    return sv;
}

template <typename... Args>
[[noreturn]] inline void enforce_fail(SourceLocation loc, Args&&... args)
{
    std::ostringstream oss;
    oss << "XML Error at " << loc.file_name() << ":" << loc.line()
        << " in " << loc.function_name();
    if constexpr (sizeof...(args) > 0)
    {
        oss << " - ";
        ((oss << args << " "), ...);
    }
    throw std::runtime_error(oss.str());
}

} // namespace xml_detail

} // namespace fat_p

#define FATP_XML_ENFORCE(condition, ...)                                     \
    do                                                                      \
    {                                                                        \
        if (!(condition))                                                    \
        {                                                                    \
            ::fat_p::xml_detail::enforce_fail(                               \
                std::source_location::current(),                             \
                "condition:", #condition __VA_OPT__(,) __VA_ARGS__);         \
        }                                                                    \
    } while (0)

namespace fat_p
{

// ============================================================
// XmlNode — tree representation of an XML document
// ============================================================

struct XmlNode
{
    std::string tag;
    std::string text;
    std::map<std::string, std::string> attributes;
    std::vector<XmlNode> children;

    // ---- Query API ----

    /// Find first child with given tag. Returns nullptr if not found.
    [[nodiscard]] const XmlNode* child(const std::string& childTag) const
    {
        for (const auto& ch : children)
            if (ch.tag == childTag) return &ch;
        return nullptr;
    }

    /// Find first child with given tag. Throws if not found.
    [[nodiscard]] const XmlNode& require(const std::string& childTag) const
    {
        const XmlNode* node = child(childTag);
        FATP_XML_ENFORCE(node != nullptr, "missing required element:", childTag);
        return *node;
    }

    /// Get all children with given tag.
    [[nodiscard]] std::vector<const XmlNode*> all(const std::string& childTag) const
    {
        std::vector<const XmlNode*> result;
        for (const auto& ch : children)
            if (ch.tag == childTag) result.push_back(&ch);
        return result;
    }

    /// Check if a child with given tag exists.
    [[nodiscard]] bool has(const std::string& childTag) const
    {
        return child(childTag) != nullptr;
    }

    /// Walk a dotted path like "options.A" → require("options").require("A").
    [[nodiscard]] const XmlNode& path(const std::string& dottedPath) const
    {
        FATP_XML_ENFORCE(!dottedPath.empty(), "empty XML path");
        const XmlNode* cur = this;
        std::string::size_type start = 0;
        while (start < dottedPath.size()) {
            auto dot = dottedPath.find('.', start);
            auto segment = dottedPath.substr(start, dot - start);
            FATP_XML_ENFORCE(!segment.empty(), "empty XML path segment:", dottedPath);
            cur = &cur->require(segment);
            start = (dot == std::string::npos) ? dottedPath.size() : dot + 1;
        }
        return *cur;
    }

    /// Check if a dotted path exists without throwing.
    [[nodiscard]] bool hasPath(const std::string& dottedPath) const
    {
        if (dottedPath.empty()) return false;
        const XmlNode* cur = this;
        std::string::size_type start = 0;
        while (start < dottedPath.size()) {
            auto dot = dottedPath.find('.', start);
            auto segment = dottedPath.substr(start, dot - start);
            if (segment.empty()) return false;
            cur = cur->child(segment);
            if (!cur) return false;
            start = (dot == std::string::npos) ? dottedPath.size() : dot + 1;
        }
        return true;
    }

    /// Get text content, trimmed.
    [[nodiscard]] std::string_view trimmedText() const
    {
        return xml_detail::trim(text);
    }

    /// Get attribute value. Returns empty optional if not found.
    [[nodiscard]] std::optional<std::string> attr(const std::string& name) const
    {
        auto it = attributes.find(name);
        if (it == attributes.end()) return std::nullopt;
        return it->second;
    }
};

// ============================================================
// XML Parser
// ============================================================

namespace xml_detail
{

inline void enforce_leaf_text_node(const XmlNode& node, const char* targetType)
{
    FATP_XML_ENFORCE(node.children.empty(),
                     "element has child elements during scalar conversion:",
                     node.tag,
                     "target type:",
                     targetType);
}

inline const XmlNode* unique_child_or_null(const XmlNode& node,
                                           std::string_view childTag)
{
    const XmlNode* found = nullptr;
    for (const auto& ch : node.children)
    {
        if (ch.tag != childTag) continue;

        FATP_XML_ENFORCE(found == nullptr,
                         "duplicate element:", childTag,
                         "inside:", node.tag);
        found = &ch;
    }
    return found;
}

inline const XmlNode& require_unique_child(const XmlNode& node,
                                           std::string_view childTag)
{
    const XmlNode* found = unique_child_or_null(node, childTag);
    FATP_XML_ENFORCE(found != nullptr, "missing required element:", childTag);
    return *found;
}

inline void enforce_no_namespace_prefix(char nextChar, const char* kind, const std::string& name)
{
    if (nextChar == ':')
        FATP_XML_ENFORCE(false, "namespaces not supported:", kind, name);
}

inline void enforce_no_xmlns_attribute(const std::string& attrName)
{
    if (attrName == "xmlns")
        FATP_XML_ENFORCE(false, "namespaces not supported: xmlns attribute");
}

[[nodiscard]] inline bool isXmlNameStartChar(char ch)
{
    return isAsciiAlpha(ch) || ch == '_';
}

[[nodiscard]] inline bool isXmlNameChar(char ch)
{
    return isAsciiAlpha(ch) || isAsciiDigit(ch) || ch == '_' || ch == '-' || ch == '.';
}

class XmlParser
{
public:
    explicit XmlParser(std::string_view input) : mInput{input}, mPos{0} {}

    XmlNode parse()
    {
        skipBom();
        skipProlog();
        XmlNode root = parseElement();
        skipComments();
        skipWhitespace();
        FATP_XML_ENFORCE(atEnd(), "unexpected trailing content at position", mPos);
        return root;
    }

private:
    std::string_view mInput;
    std::size_t mPos;

    [[nodiscard]] char peek() const { return mPos < mInput.size() ? mInput[mPos] : '\0'; }

    char advance()
    {
        FATP_XML_ENFORCE(!atEnd(), "unexpected end of XML at position", mPos);
        return mInput[mPos++];
    }

    template <typename... Args>
    void expect(char expected, Args&&... args)
    {
        FATP_XML_ENFORCE(!atEnd(), "unexpected end of XML at position", mPos);
        char actual = advance();
        FATP_XML_ENFORCE(actual == expected, std::forward<Args>(args)...);
    }

    [[nodiscard]] bool atEnd() const { return mPos >= mInput.size(); }

    void skipWhitespace()
    {
        while (!atEnd() && isXmlWhitespace(mInput[mPos]))
            ++mPos;
    }

    void skipBom()
    {
        if (mPos == 0 && mInput.size() >= 3 &&
            static_cast<unsigned char>(mInput[0]) == 0xEF &&
            static_cast<unsigned char>(mInput[1]) == 0xBB &&
            static_cast<unsigned char>(mInput[2]) == 0xBF)
        {
            mPos = 3;
        }
    }

    // Skip XML declaration (<?xml ... ?>) and comments (<!-- ... -->)
    void skipProlog()
    {
        bool sawXmlDecl = false;
        bool sawComment = false;

        while (true)
        {
            skipWhitespace();
            if (atEnd()) break;

            if (mInput.substr(mPos, 5) == "<?xml")
            {
                FATP_XML_ENFORCE(!sawComment, "XML declaration after comment");
                FATP_XML_ENFORCE(!sawXmlDecl, "repeated XML declaration");

                if (mPos + 5 < mInput.size())
                {
                    char next = mInput[mPos + 5];
                    if (next == '?')
                        FATP_XML_ENFORCE(false, "empty XML declaration");
                    if (next != '?' && !isXmlWhitespace(next))
                        FATP_XML_ENFORCE(false,
                                         "processing instructions not supported at position",
                                         mPos);
                }

                auto endPos = mInput.find("?>", mPos);
                FATP_XML_ENFORCE(endPos != std::string_view::npos, "unterminated XML declaration");
                mPos = endPos + 2;
                sawXmlDecl = true;
                continue;
            }

            if (mInput.substr(mPos, 2) == "<?")
                FATP_XML_ENFORCE(false, "processing instructions not supported at position", mPos);

            if (mInput.substr(mPos, 4) == "<!--")
            {
                auto endPos = mInput.find("-->", mPos);
                FATP_XML_ENFORCE(endPos != std::string_view::npos, "unterminated comment");
                mPos = endPos + 3;
                sawComment = true;
                continue;
            }

            break;
        }
    }

    void skipComments()
    {
        while (true)
        {
            skipWhitespace();
            if (mInput.substr(mPos, 4) == "<!--")
            {
                auto endPos = mInput.find("-->", mPos);
                FATP_XML_ENFORCE(endPos != std::string_view::npos, "unterminated comment");
                mPos = endPos + 3;
                continue;
            }
            break;
        }
    }

    std::string parseName()
    {
        FATP_XML_ENFORCE(!atEnd(), "expected name at position", mPos);
        FATP_XML_ENFORCE(isXmlNameStartChar(peek()), "invalid name start at position", mPos);
        std::size_t start = mPos;
        ++mPos;
        while (!atEnd() && isXmlNameChar(mInput[mPos]))
            ++mPos;
        return std::string{mInput.substr(start, mPos - start)};
    }

    std::string parseQuotedString()
    {
        char quote = advance();
        FATP_XML_ENFORCE(quote == '"' || quote == '\'', "expected quote");
        std::string result;
        while (!atEnd() && peek() != quote)
        {
            FATP_XML_ENFORCE(peek() != '<',
                             "raw '<' not allowed in attribute value at position",
                             mPos);

            if (peek() == '&')
                result += parseEntity();
            else
                result += advance();
        }
        FATP_XML_ENFORCE(!atEnd(), "unterminated string");
        advance(); // closing quote
        return result;
    }

    std::string parseEntity()
    {
        advance(); // skip &
        std::size_t start = mPos;
        while (!atEnd() && peek() != ';') ++mPos;
        FATP_XML_ENFORCE(!atEnd(), "unterminated entity reference");
        std::string_view entity = mInput.substr(start, mPos - start);
        advance(); // skip ;

        if (entity == "amp") return "&";
        if (entity == "lt") return "<";
        if (entity == "gt") return ">";
        if (entity == "apos") return "'";
        if (entity == "quot") return "\"";

        FATP_XML_ENFORCE(false, "unknown entity:", std::string(entity));
        return {};
    }

    std::string parseTextContent(const std::string& closingTag)
    {
        std::string result;
        std::string closeStr = "</" + closingTag;
        while (!atEnd())
        {
            if (peek() == '<')
            {
                // Check if this is the closing tag or a child element
                if (mInput.substr(mPos, closeStr.size()) == closeStr)
                    break;
                if (mInput.substr(mPos, 4) == "<!--")
                {
                    auto endPos = mInput.find("-->", mPos);
                    FATP_XML_ENFORCE(endPos != std::string_view::npos, "unterminated comment");
                    mPos = endPos + 3;
                    continue;
                }
                break; // child element
            }
            if (peek() == '&')
            {
                result += parseEntity();
            }
            else
            {
                result += advance();
            }
        }
        return result;
    }

    XmlNode parseElement()
    {
        skipComments();
        FATP_XML_ENFORCE(peek() == '<', "expected '<' at position", mPos);
        advance(); // skip <

        XmlNode node;
        node.tag = parseName();
        enforce_no_namespace_prefix(peek(), "prefixed element name", node.tag);

        // Parse attributes
        while (true)
        {
            skipWhitespace();
            if (peek() == '/' || peek() == '>') break;
            std::string attrName = parseName();
            enforce_no_namespace_prefix(peek(), "prefixed attribute name", attrName);
            enforce_no_xmlns_attribute(attrName);
            skipWhitespace();
            expect('=', "expected '=' after attribute name");
            skipWhitespace();
            std::string attrValue = parseQuotedString();
            FATP_XML_ENFORCE(!node.attributes.contains(attrName),
                             "duplicate attribute:", attrName);
            node.attributes.emplace(std::move(attrName), std::move(attrValue));
        }

        // Self-closing tag
        if (peek() == '/')
        {
            advance(); // skip /
            expect('>', "expected '>' after '/'");
            return node;
        }

        expect('>', "expected '>'");

        // Parse content: mixed text and child elements
        bool closed = false;
        while (true)
        {
            // Collect text before next tag
            std::string textChunk = parseTextContent(node.tag);
            if (!textChunk.empty())
            {
                auto trimmed = trim(textChunk);
                if (!trimmed.empty())
                {
                    if (!node.text.empty()) node.text += ' ';
                    node.text += trimmed;
                }
            }

            skipComments();
            FATP_XML_ENFORCE(!atEnd(), "unterminated element:", node.tag);

            // Check for closing tag
            std::string closeStr = "</" + node.tag;
            if (mInput.substr(mPos, closeStr.size()) == closeStr)
            {
                mPos += closeStr.size();
                skipWhitespace();
                expect('>', "expected '>' in closing tag for", node.tag);
                closed = true;
                break;
            }

            // Must be a child element
            if (peek() == '<')
            {
                node.children.push_back(parseElement());
            }
            else
            {
                FATP_XML_ENFORCE(false, "unexpected content at position", mPos);
            }
        }

        FATP_XML_ENFORCE(closed, "unterminated element:", node.tag);
        return node;
    }
};

} // namespace xml_detail

// ============================================================
// Public parse API
// ============================================================

/// Parse XML from a string.
[[nodiscard]] inline XmlNode parse_xml(std::string_view xml)
{
    xml_detail::XmlParser parser{xml};
    return parser.parse();
}

/// Parse XML from a file.
[[nodiscard]] inline XmlNode parse_xml_file(const std::string& filename)
{
    std::ifstream file{filename, std::ios::binary};
    FATP_XML_ENFORCE(file.is_open(), "cannot open file:", filename);
    std::ostringstream ss;
    ss << file.rdbuf();
    FATP_XML_ENFORCE(file.good() || file.eof(),
                     "error while reading file:", filename);
    return parse_xml(ss.str());
}

// ============================================================
// Value extraction — from_xml overloads
// ============================================================

/// Extract text content as string.
inline void from_xml(const XmlNode& node, std::string& value)
{
    xml_detail::enforce_leaf_text_node(node, "string");
    value = std::string{node.trimmedText()};
}

/// Extract text content as double.
inline void from_xml(const XmlNode& node, double& value)
{
    xml_detail::enforce_leaf_text_node(node, "double");
    auto sv = node.trimmedText();
    FATP_XML_ENFORCE(!sv.empty(), "empty element for numeric conversion:", node.tag);
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    FATP_XML_ENFORCE(ec == std::errc{} && ptr == sv.data() + sv.size(),
                     "invalid double in element:", node.tag);
    FATP_XML_ENFORCE(std::isfinite(value),
                     "non-finite double in element:", node.tag);
}

/// Extract text content as int.
inline void from_xml(const XmlNode& node, int& value)
{
    xml_detail::enforce_leaf_text_node(node, "int");
    auto sv = node.trimmedText();
    FATP_XML_ENFORCE(!sv.empty(), "empty element for numeric conversion:", node.tag);
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    FATP_XML_ENFORCE(ec == std::errc{} && ptr == sv.data() + sv.size(),
                     "invalid int in element:", node.tag);
}

/// Extract text content as int64_t.
inline void from_xml(const XmlNode& node, std::int64_t& value)
{
    xml_detail::enforce_leaf_text_node(node, "int64_t");
    auto sv = node.trimmedText();
    FATP_XML_ENFORCE(!sv.empty(), "empty element for numeric conversion:", node.tag);
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);
    FATP_XML_ENFORCE(ec == std::errc{} && ptr == sv.data() + sv.size(),
                     "invalid int64 in element:", node.tag);
}

/// Extract text content as std::size_t.
inline void from_xml(const XmlNode& node, std::size_t& value)
{
    xml_detail::enforce_leaf_text_node(node, "size_t");
    auto sv = node.trimmedText();
    FATP_XML_ENFORCE(!sv.empty(), "empty element for numeric conversion:", node.tag);
    unsigned long long temp = 0;
    auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), temp);
    FATP_XML_ENFORCE(ec == std::errc{} && ptr == sv.data() + sv.size(),
                     "invalid size_t in element:", node.tag);
    FATP_XML_ENFORCE(temp <= static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max()),
                     "size_t overflow in element:", node.tag);
    value = static_cast<std::size_t>(temp);
}

/// Extract text content as bool.
inline void from_xml(const XmlNode& node, bool& value)
{
    xml_detail::enforce_leaf_text_node(node, "bool");
    auto sv = node.trimmedText();
    if (sv == "true" || sv == "1") { value = true; return; }
    if (sv == "false" || sv == "0") { value = false; return; }
    FATP_XML_ENFORCE(false, "invalid bool in element:", node.tag, "got:", std::string(sv));
}

namespace xml_detail
{

template <typename E>
struct XmlEnumStringPolicy;

template <typename E>
concept xml_string_enum =
    std::is_enum_v<E> &&
    requires(std::string_view sv)
    {
        { XmlEnumStringPolicy<E>::from_string(sv) } -> std::same_as<E>;
    };

} // namespace xml_detail

/// Extract text content as enum (string form: requires FATP_XML_ENUM_STRING_POLICY).
template <xml_detail::xml_string_enum E>
inline void from_xml(const XmlNode& node, E& value)
{
    xml_detail::enforce_leaf_text_node(node, "enum");
    const auto sv = node.trimmedText();
    FATP_XML_ENFORCE(!sv.empty(), "empty element for enum conversion:", node.tag);

    try
    {
        value = xml_detail::XmlEnumStringPolicy<E>::from_string(sv);
    }
    catch (const std::exception& e)
    {
        FATP_XML_ENFORCE(false,
                         "invalid enum in element:", node.tag,
                         "value:", std::string(sv),
                         "error:", e.what());
    }
}

/// Extract text content as enum (integer form: underlying numeric value).
template <typename E>
    requires std::is_enum_v<E> && (!xml_detail::xml_string_enum<E>)
inline void from_xml(const XmlNode& node, E& value)
{
    xml_detail::enforce_leaf_text_node(node, "enum");
    using U = std::underlying_type_t<E>;

    if constexpr (std::is_signed_v<U>)
    {
        std::int64_t raw{};
        from_xml(node, raw);
        FATP_XML_ENFORCE(raw >= static_cast<std::int64_t>(std::numeric_limits<U>::min()) &&
                             raw <= static_cast<std::int64_t>(std::numeric_limits<U>::max()),
                         "enum value out of range in element:", node.tag);
        value = static_cast<E>(static_cast<U>(raw));
    }
    else
    {
        auto sv = node.trimmedText();
        FATP_XML_ENFORCE(!sv.empty(), "empty element for enum conversion:", node.tag);

        unsigned long long raw = 0;
        auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), raw);
        FATP_XML_ENFORCE(ec == std::errc{} && ptr == sv.data() + sv.size(),
                         "invalid enum in element:", node.tag);
        FATP_XML_ENFORCE(raw <= static_cast<unsigned long long>(std::numeric_limits<U>::max()),
                         "enum value out of range in element:", node.tag);

        value = static_cast<E>(static_cast<U>(raw));
    }
}

/// Value-returning from_xml.
template <typename T>
[[nodiscard]] inline T from_xml(const XmlNode& node)
{
    T value{};
    from_xml(node, value);
    return value;
}

/// Extract optional from a node directly (node present → value set).
template <typename T>
inline void from_xml(const XmlNode& node, std::optional<T>& value)
{
    T temp{};
    from_xml(node, temp);
    value = temp;
}

/// Extract optional by child tag — returns nullopt if child absent.
template <typename T>
inline void from_xml(const XmlNode& parent, const std::string& childTag,
                     std::optional<T>& value)
{
    const XmlNode* ch = parent.child(childTag);
    if (ch)
    {
        T temp{};
        from_xml(*ch, temp);
        value = temp;
    }
    else
    {
        value = std::nullopt;
    }
}

/// Extract a vector of T from repeated child elements.
template <typename T>
inline void from_xml(const XmlNode& parent, const std::string& childTag,
                     std::vector<T>& value)
{
    value.clear();
    for (const auto& ch : parent.children)
    {
        if (ch.tag == childTag)
        {
            T temp{};
            from_xml(ch, temp);
            value.push_back(std::move(temp));
        }
    }
}

/// Convenience: get all children of a given tag as a vector of T.
template <typename T>
[[nodiscard]] inline std::vector<T> xml_all(const XmlNode& parent,
                                             const std::string& wrapperTag,
                                             const std::string& itemTag)
{
    const XmlNode& wrapper = parent.require(wrapperTag);
    std::vector<T> result;
    from_xml(wrapper, itemTag, result);
    return result;
}

// ============================================================
// Automatic struct deserialization macros
// ============================================================
//
// FATP_XML_DEFINE_TYPE(Type, field1, field2, ...)
//   Generates from_xml(const XmlNode&, Type&) that reads each
//   field from exactly one child element with the same name. All fields required.
//
// FATP_XML_DEFINE_TYPE_OPTIONAL(Type, field1, field2, ...)
//   Same but missing fields are silently skipped (struct defaults kept).
//   At most one child per field; duplicates throw.
//
// Vector/repeated fields are not supported by these macros. Use
// from_xml(parent, childTag, vector) or xml_all(parent, wrapperTag, itemTag).
//
// Field deserialization uses from_xml_adl so user-defined types outside
// namespace fat_p resolve via ADL (nested structs, optional<T>, etc.).

namespace xml_detail
{

template <typename T>
inline void from_xml_adl(const XmlNode& node, T& value)
{
    using ::fat_p::from_xml;
    from_xml(node, value);
}

} // namespace xml_detail

// ---- Internal macro machinery ----

#define FATP_XML_FROM_FIELD_REQUIRED(field)                                  \
    {                                                                        \
        const ::fat_p::XmlNode& _node =                                      \
            ::fat_p::xml_detail::require_unique_child(node, std::string_view(#field)); \
        ::fat_p::xml_detail::from_xml_adl(_node, value.field);               \
    }

#define FATP_XML_FROM_FIELD_OPTIONAL(field)                                  \
    {                                                                        \
        const ::fat_p::XmlNode* _node =                                      \
            ::fat_p::xml_detail::unique_child_or_null(node, std::string_view(#field)); \
        if (_node) ::fat_p::xml_detail::from_xml_adl(*_node, value.field);   \
    }

// Variadic expansion (reuse the FOR_EACH pattern)
#define FATP_XML_EXPAND(...) __VA_ARGS__
#define FATP_XML_APPLY_1(m, a) m(a)
#define FATP_XML_APPLY_2(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_1(m, __VA_ARGS__))
#define FATP_XML_APPLY_3(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_2(m, __VA_ARGS__))
#define FATP_XML_APPLY_4(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_3(m, __VA_ARGS__))
#define FATP_XML_APPLY_5(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_4(m, __VA_ARGS__))
#define FATP_XML_APPLY_6(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_5(m, __VA_ARGS__))
#define FATP_XML_APPLY_7(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_6(m, __VA_ARGS__))
#define FATP_XML_APPLY_8(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_7(m, __VA_ARGS__))
#define FATP_XML_APPLY_9(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_8(m, __VA_ARGS__))
#define FATP_XML_APPLY_10(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_9(m, __VA_ARGS__))
#define FATP_XML_APPLY_11(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_10(m, __VA_ARGS__))
#define FATP_XML_APPLY_12(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_11(m, __VA_ARGS__))
#define FATP_XML_APPLY_13(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_12(m, __VA_ARGS__))
#define FATP_XML_APPLY_14(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_13(m, __VA_ARGS__))
#define FATP_XML_APPLY_15(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_14(m, __VA_ARGS__))
#define FATP_XML_APPLY_16(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_15(m, __VA_ARGS__))
#define FATP_XML_APPLY_17(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_16(m, __VA_ARGS__))
#define FATP_XML_APPLY_18(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_17(m, __VA_ARGS__))
#define FATP_XML_APPLY_19(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_18(m, __VA_ARGS__))
#define FATP_XML_APPLY_20(m, a, ...) m(a) FATP_XML_EXPAND(FATP_XML_APPLY_19(m, __VA_ARGS__))

#define FATP_XML_ARG_COUNT_IMPL(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,_17,_18,_19,_20,N,...) N
#define FATP_XML_ARG_COUNT(...) FATP_XML_EXPAND(FATP_XML_ARG_COUNT_IMPL(__VA_ARGS__,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1))

#define FATP_XML_CAT_IMPL(a, b) a##b
#define FATP_XML_CAT(a, b) FATP_XML_CAT_IMPL(a, b)

#define FATP_XML_FOR_EACH(macro, ...) \
    FATP_XML_EXPAND(FATP_XML_CAT(FATP_XML_APPLY_, FATP_XML_ARG_COUNT(__VA_ARGS__))(macro, __VA_ARGS__))

// ---- Public macros ----

#define FATP_XML_DEFINE_TYPE(Type, ...)                                      \
    inline void from_xml(const ::fat_p::XmlNode& node, Type& value)          \
    {                                                                        \
        FATP_XML_FOR_EACH(FATP_XML_FROM_FIELD_REQUIRED, __VA_ARGS__)         \
    }

#define FATP_XML_DEFINE_TYPE_OPTIONAL(Type, ...)                             \
    inline void from_xml(const ::fat_p::XmlNode& node, Type& value)          \
    {                                                                        \
        FATP_XML_FOR_EACH(FATP_XML_FROM_FIELD_OPTIONAL, __VA_ARGS__)         \
    }

} // namespace fat_p

// ============================================================================
// FATP_XML_ENUM_STRING_POLICY — XML-local string enum deserialization
// ============================================================================
//
// Call at global/file scope only — NOT inside any namespace block.
// The macro opens namespace fat_p::xml_detail relative to the call site.
// At file scope that is ::fat_p::xml_detail (correct). Inside a user namespace
// it becomes app::fat_p::xml_detail (wrong). Inside fat_p::testing::xmllite → MSVC C2888.
// GCC also rejects qualified out-of-namespace specializations
// (struct ::fat_p::xml_detail::XmlEnumStringPolicy<...>), so file scope is required.
//
//   enum class Mode { Off, On };
//   FATP_XML_ENUM_STRING_POLICY(Mode, Off, On)
//
// For enums declared in a user namespace, pass the qualified enum type:
//
//   namespace app { enum class Mode { Off, On }; }
//   FATP_XML_ENUM_STRING_POLICY(app::Mode, Off, On)

#define FATP_XML_ENUM_STRING_CASE(EnumType, enumerator)                     \
    if (sv == #enumerator) return EnumType::enumerator;

#define FATP_XML_ENUM_STRING_APPLY_1(m, EnumType, a) m(EnumType, a)
#define FATP_XML_ENUM_STRING_APPLY_2(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_1(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_3(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_2(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_4(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_3(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_5(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_4(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_6(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_5(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_7(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_6(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_8(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_7(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_9(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_8(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_10(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_9(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_11(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_10(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_12(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_11(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_13(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_12(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_14(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_13(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_15(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_14(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_16(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_15(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_17(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_16(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_18(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_17(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_19(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_18(m, EnumType, __VA_ARGS__))
#define FATP_XML_ENUM_STRING_APPLY_20(m, EnumType, a, ...) m(EnumType, a) FATP_XML_EXPAND(FATP_XML_ENUM_STRING_APPLY_19(m, EnumType, __VA_ARGS__))

#define FATP_XML_ENUM_STRING_FOR_EACH(macro, EnumType, ...) \
    FATP_XML_EXPAND(FATP_XML_CAT(FATP_XML_ENUM_STRING_APPLY_, FATP_XML_ARG_COUNT(__VA_ARGS__))(macro, EnumType, __VA_ARGS__))

#define FATP_XML_ENUM_STRING_POLICY(EnumType, ...)                             \
    namespace fat_p                                                            \
    {                                                                          \
    namespace xml_detail                                                       \
    {                                                                          \
    template <>                                                                \
    struct XmlEnumStringPolicy<EnumType>                                       \
    {                                                                          \
        static EnumType from_string(std::string_view sv)                       \
        {                                                                      \
            FATP_XML_ENUM_STRING_FOR_EACH(FATP_XML_ENUM_STRING_CASE, EnumType, __VA_ARGS__) \
            FATP_XML_ENFORCE(false, "invalid XML enum token:", std::string(sv)); \
            return EnumType{};                                                 \
        }                                                                      \
    };                                                                         \
    }                                                                          \
    }

#ifndef FATP_ENUM_STRING_POLICY
#define FATP_ENUM_STRING_POLICY(EnumType, ...) \
    FATP_XML_ENUM_STRING_POLICY(EnumType, __VA_ARGS__)
#endif

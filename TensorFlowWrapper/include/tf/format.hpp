// tf/format.hpp
// Small compatibility wrapper for std::format.
//
// Goal:
// - Keep using std::format when the standard library provides it.
// - Provide a fallback for toolchains where <format> is missing or incomplete
//   (common on some libstdc++ versions), without forcing callers to remove
//   std::format-style call sites.
//
// This is intentionally minimal: it supports only the formatting patterns used
// in this wrapper's error messages.

#pragma once

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

// Prefer a feature-test when available.
#if defined(__cpp_lib_format) && (__cpp_lib_format >= 201907L)
    #include <format>
    #define TFWRAP_HAS_STD_FORMAT 1
#else
    #define TFWRAP_HAS_STD_FORMAT 0
#endif

namespace tf::detail {

// -----------------------------------------------------------------------------
// format() - std::format when available; otherwise a conservative fallback.
//
// Fallback behavior:
// - If std::format is unavailable, we keep the original format string visible
//   and append all arguments (space-separated) after a " |" marker.
// - This is intentionally conservative (it never tries to interpret full
//   std::format syntax).
// -----------------------------------------------------------------------------

#if TFWRAP_HAS_STD_FORMAT

template<class... Args>
[[nodiscard]] inline std::string format(std::format_string<Args...> fmt, Args&&... args)
{
    return std::format(fmt, std::forward<Args>(args)...);
}

#else

namespace detail_impl {
inline void append_one(std::ostringstream& os, std::string_view s) { os << s; }
inline void append_one(std::ostringstream& os, const char* s) { os << (s ? s : ""); }

template<class T>
inline void append_one(std::ostringstream& os, const T& v)
{
    if constexpr (std::is_same_v<T, std::string>) {
        os << v;
    } else {
        os << v;
    }
}

template<class... Args>
inline void append_all(std::ostringstream& os, Args&&... args)
{
    bool first = true;
    auto append_sep = [&]() {
        if (!first) os << ' ';
        first = false;
    };

    ((append_sep(), append_one(os, std::forward<Args>(args))), ...);
}

// Conservative fallback: keep fmt visible and append args.
template<class... Args>
inline std::string braces_replace(std::string_view fmt, Args&&... args)
{
    std::ostringstream os;
    os << fmt;
    if constexpr (sizeof...(Args) > 0) {
        os << " |";
        append_all(os, std::forward<Args>(args)...);
    }
    return os.str();
}
} // namespace detail_impl

template<class... Args>
[[nodiscard]] inline std::string format(std::string_view fmt, Args&&... args)
{
    // Conservative: keep the original format string visible and append args.
    // This guarantees diagnostics are not lost even without std::format.
    return detail_impl::braces_replace(fmt, std::forward<Args>(args)...);
}

#endif

} // namespace tf::detail

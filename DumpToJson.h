#pragma once
#include "EqualityComparisons.h"  // Reuse traits/dispatcher framework
#include <sstream>
#include <iomanip>  // For precision
#include <cctype>   // For escape
#include <utility>  // For forwarding

namespace fat_p {
    // --- JSON Policies ---
    struct StandardJsonPolicy {
        static constexpr bool pretty_print = false;
        static constexpr int numeric_precision = 6;
    };

    // --- JSON Dispatcher (Adapts EqualDispatcher for Output) ---
    template <typename T, typename Policy = StandardJsonPolicy>
    struct JsonDispatcher;

    // Scalar Helpers
    namespace detail {
        template <typename Os>
        void escape_string(Os& os, const std::string& s) {
            os << '"';
            for (char c : s) {
                switch (c) {
                    case '"': os << '\\' << '"'; break;
                    case '\\': os << '\\' << '\\'; break;
                    case '\b': os << '\\' << 'b'; break;
                    case '\f': os << '\\' << 'f'; break;
                    case '\n': os << '\\' << 'n'; break;
                    case '\r': os << '\\' << 'r'; break;
                    case '\t': os << '\\' << 't'; break;
                    default: os << c;
                }
            }
            os << '"';
        }

        template <typename Os, typename V>
        void dump_scalar(Os& os, const V& v) {
            if constexpr (std::is_same_v<std::decay_t<V>, std::string>) {
                detail::escape_string(os, v);
            } else if constexpr (std::is_same_v<std::decay_t<V>, bool>) {
                os << (v ? "true" : "false");
            } else if constexpr (std::is_floating_point_v<std::decay_t<V>>) {
                os << std::fixed << std::setprecision(Policy::numeric_precision) << v;
            } else if constexpr (std::is_arithmetic_v<std::decay_t<V>>) {
                os << v;
            } else {
                os << "null";  // Fallback
            }
        }
    }

    // Base JsonDispatcher (Scalars)
    template <typename T, typename Policy>
    struct JsonDispatcher<T, Policy> {
        template <typename Os>
        static void dump(Os& os, const T& obj) {
            if constexpr (std::is_same_v<T, std::nullptr_t> || std::is_null_pointer_v<T>) {
                os << "null";
            } else {
                detail::dump_scalar(os, obj);
            }
        }
    };

    // Pair Specialization (Uses IsPair trait from EqualityComparisons)
    template <typename T1, typename T2, typename Policy>
    struct JsonDispatcher<std::pair<T1, T2>, Policy> {
        template <typename Os>
        static void dump(Os& os, const std::pair<T1, T2>& p) {
            os << '{';
            JsonDispatcher<T1, Policy>::dump(os, p.first); os << ':';
            if constexpr (Policy::pretty_print) os << ' ';
            JsonDispatcher<T2, Policy>::dump(os, p.second);
            os << '}';
        }
    };

    // Tuple Specialization (Uses IsTuple + index_sequence from EqualityComparisons)
    template <typename... Ts, typename Policy>
    struct JsonDispatcher<std::tuple<Ts...>, Policy> {
        template <typename Os>
        static void dump(Os& os, const std::tuple<Ts...>& tup) {
            os << '[';
            std::apply([&](const auto&... elems) {
                (JsonDispatcher<std::decay_t<decltype(elems)>, Policy>::dump(os, elems), ...);
            }, tup);
            os << ']';
        }
    };

    // Iterable Specialization (Uses IsIterable trait)
    template <typename T, typename Policy>
    struct JsonDispatcher<T, Policy> : std::enable_if_t<IsIterable<T>::value, JsonDispatcher<T, Policy>> {
        template <typename Os>
        static void dump(Os& os, const T& cont) {
            os << '[';
            bool first = true;
            for (const auto& elem : cont) {
                if (!first) os << ',';
                if constexpr (Policy::pretty_print) os << ' ';
                JsonDispatcher<ContainerValueT<T>, Policy>::dump(os, elem);
                first = false;
            }
            os << ']';
        }
    };

    // --- Public JsonDumper Class ---
    template <typename T, typename Policy = StandardJsonPolicy>
    class JsonDumper {
    public:
        /**
         * @brief Dumps the object to a string.
         * @param obj The object to serialize.
         * @param pretty Optional pretty-print (indents/ spaces).
         * @return std::string The JSON representation.
         */
        [[nodiscard]] static std::string dump(const T& obj, bool pretty = Policy::pretty_print) {
            std::ostringstream oss;
            if (pretty) oss << std::setw(2);  // Basic indent sim
            JsonDispatcher<std::decay_t<T>, std::conditional_t<pretty, PrettyJsonPolicy, Policy>>::dump(oss, obj);
            return oss.str();
        }

        /**
         * @brief Dumps to ostream.
         */
        template <typename Os>
        static void dump_to(Os& os, const T& obj, bool pretty = Policy::pretty_print) {
            using P = std::conditional_t<pretty, PrettyJsonPolicy, Policy>;
            JsonDispatcher<std::decay_t<T>, P>::dump(os, obj);
        }
    };

    // Pretty variant (add indents – simple, no full formatter)
    struct PrettyJsonPolicy : StandardJsonPolicy { static constexpr bool pretty_print = true; };
}

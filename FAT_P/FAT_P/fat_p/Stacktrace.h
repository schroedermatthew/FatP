// Stacktrace.h

#pragma once
/*
FATP_META:
  meta_version: 1
  component: Stacktrace
  file_role: public_header
  path: fat_p/Stacktrace.h
  namespace: fat_p
  summary: "Public header for Stacktrace."
  api_stability: in_work
  related:
    docs_search: "Stacktrace"
    tests:
      - tests/test_Stacktrace.cpp
  hygiene:
    pragma_once: false
    include_guard: true
    defines_total: 1
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

#include <string>
#include <vector>

// Placeholder for Boost.Stacktrace or custom impl; assume single-header polyfill
// For real use, include <boost/stacktrace.hpp> if allowing Boost, else custom unwind

namespace fat_p {

struct StackFrame {
    std::string function;
    std::string file;
    size_t line;
};

class Stacktrace {
private:
    std::vector<StackFrame> frames_;

public:
    static Stacktrace current(size_t skip = 1) {
        Stacktrace st;
        // Emulate with __builtin_return_address or libunwind; simplified placeholder
        st.frames_.push_back({"current_function", "file.cpp", 42});  // Replace with actual unwind
        return st;
    }

    const std::vector<StackFrame>& frames() const { return frames_; }

    std::string to_string() const {
        std::string result;
        for (const auto& f : frames_) {
            result += f.file + ":" + std::to_string(f.line) + " in " + f.function + "\n";
        }
        return result;
    }
};

// Integration with enforce: e.g., enforce(condition) << Stacktrace::current();

}  // namespace fat_p

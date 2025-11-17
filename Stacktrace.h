// Stacktrace.h
#ifndef FATP_STACKTRACE_H
#define FATP_STACKTRACE_H

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

#endif  // FATP_STACKTRACE_H
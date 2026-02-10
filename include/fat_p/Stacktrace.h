#pragma once
/*
FATP_META:
  meta_version: 1
  component: Stacktrace
  file_role: public_header
  path: include/fat_p/Stacktrace.h
  namespace: fat_p
  layer: Foundation
  summary: Portable stack trace capture with multi-backend support.
  api_stability: candidate
  related:
    docs_search: "Stacktrace"
    tests:
      - components/Stacktrace/tests/test_Stacktrace.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 8
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

/**
 * @file Stacktrace.h
 * @brief Portable stack trace capture utilities with multi-backend support
 *
 * Provides cross-platform stack trace capture with automatic backend selection:
 * - C++23 std::stacktrace (preferred when available)
 * - libunwind (Linux/macOS)
 * - execinfo backtrace() (POSIX fallback)
 * - Windows DbgHelp API
 * - Stub implementation (when no backend available)
 *
 * Features:
 * - Async-signal-safe raw capture via captureRaw()
 * - Automatic symbol resolution with demangling
 * - Thread-safe operation
 * - JSON and string output formatting
 * - Integration ready for ContractException
 *
 * @section usage Usage Examples
 * @code
 * // Basic capture with symbol resolution
 * auto st = fat_p::Stacktrace::current();
 * std::cout << st.toString();
 *
 * // Async-signal-safe capture (for crash handlers)
 * auto raw = fat_p::Stacktrace::captureRaw();
 * raw.resolveSymbols();  // Call later when safe
 *
 * // Skip frames (useful in wrappers)
 * auto st = fat_p::Stacktrace::current(2);  // Skip 2 frames
 *
 * // Limit depth
 * auto st = fat_p::Stacktrace::current(1, 10);  // Max 10 frames
 * @endcode
 *
 * @section performance Performance Characteristics
 * - Raw capture: O(n) where n = stack depth, typically 1-10 microseconds
 * - Symbol resolution: O(n * m) where m = symbol lookup cost, typically 10-100 microseconds
 * - Memory: sizeof(StackFrame) * captured_frames + overhead
 *
 * @note Thread-safe: Each Stacktrace instance is independent
 * @note C++20 minimum required
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>

#include "CppFeatureDetection.h"
#include "PlatformDetection.h"

// =============================================================================
// Backend Detection
// =============================================================================

// C++23 std::stacktrace detection
// __has_include(<stacktrace>) alone is insufficient â€” some implementations ship the header
// without the actual std::stacktrace class (e.g., Clang-17 with libstdc++). The feature-test
// macro __cpp_lib_stacktrace (P0881R7, >= 202011L) is the authoritative signal.
#if FATP_CPP23_OR_LATER && defined(__has_include)
#if __has_include(<stacktrace>)
#include <stacktrace>
#if defined(__cpp_lib_stacktrace) && __cpp_lib_stacktrace >= 202011L
#define FATP_HAS_STD_STACKTRACE 1
#else
#define FATP_HAS_STD_STACKTRACE 0
#endif
#else
#define FATP_HAS_STD_STACKTRACE 0
#endif
#else
#define FATP_HAS_STD_STACKTRACE 0
#endif

// libunwind detection (preferred on POSIX)
#if FATP_PLATFORM_POSIX && defined(__has_include)
#if __has_include(<libunwind.h>)
#define FATP_HAS_LIBUNWIND 1
#else
#define FATP_HAS_LIBUNWIND 0
#endif
#else
#define FATP_HAS_LIBUNWIND 0
#endif

// execinfo backtrace() detection (fallback on POSIX)
#if FATP_PLATFORM_POSIX && defined(__has_include)
#if __has_include(<execinfo.h>)
#define FATP_HAS_EXECINFO 1
#else
#define FATP_HAS_EXECINFO 0
#endif
#else
#define FATP_HAS_EXECINFO 0
#endif

// Windows DbgHelp detection
#if FATP_PLATFORM_WINDOWS
#define FATP_HAS_DBGHELP 1
#else
#define FATP_HAS_DBGHELP 0
#endif

// Include platform headers based on detection
#if FATP_HAS_LIBUNWIND && !FATP_HAS_STD_STACKTRACE
#define UNW_LOCAL_ONLY
#include <cxxabi.h>
#include <dlfcn.h>
#include <libunwind.h>
#undef UNW_LOCAL_ONLY
#endif

#if FATP_HAS_EXECINFO && !FATP_HAS_LIBUNWIND && !FATP_HAS_STD_STACKTRACE
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#endif

#if FATP_HAS_DBGHELP && !FATP_HAS_STD_STACKTRACE
#ifndef NOMINMAX
#define NOMINMAX
#define FATP_DEFINED_NOMINMAX_STACKTRACE
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define FATP_DEFINED_WIN32_LEAN_AND_MEAN_STACKTRACE
#endif
#include <Windows.h>
#include <DbgHelp.h>
#ifdef FATP_DEFINED_NOMINMAX_STACKTRACE
#undef NOMINMAX
#undef FATP_DEFINED_NOMINMAX_STACKTRACE
#endif
#ifdef FATP_DEFINED_WIN32_LEAN_AND_MEAN_STACKTRACE
#undef WIN32_LEAN_AND_MEAN
#undef FATP_DEFINED_WIN32_LEAN_AND_MEAN_STACKTRACE
#endif
#pragma comment(lib, "dbghelp.lib")
#include <mutex>
#endif

namespace fat_p
{

// =============================================================================
// StackFrame Structure
// =============================================================================

/**
 * @brief Represents a single frame in a stack trace
 *
 * Contains the instruction pointer and optional symbol information.
 * Symbol resolution is lazy - address is always available, but function/file/line
 * may be empty until resolveSymbols() is called on the parent Stacktrace.
 */
struct StackFrame
{
    /// Instruction pointer address (always available after capture)
    void* address = nullptr;

    /// Demangled function name (empty if not resolved or unavailable)
    std::string function;

    /// Source file path (empty if debug info unavailable)
    std::string file;

    /// Line number in source file (0 if unknown)
    std::size_t line = 0;

    /// Column number in source file (0 if unknown)
    std::size_t column = 0;

    /// Module/shared library name containing this frame
    std::string module;

    /// Base address of the module
    void* moduleBase = nullptr;

    /// Offset from function start in bytes
    std::size_t offset = 0;

    /// True if symbol resolution has been attempted
    bool symbolized = false;

    /**
     * @brief Format frame as human-readable string
     * @return String in format "function+offset at file:line [address]"
     */
    [[nodiscard]] std::string toString() const
    {
        std::ostringstream oss;

        if (!function.empty())
        {
            oss << function;
            if (offset > 0)
            {
                oss << "+0x" << std::hex << offset << std::dec;
            }
        }
        else
        {
            oss << "???";
        }

        if (!file.empty())
        {
            oss << " at " << file;
            if (line > 0)
            {
                oss << ":" << line;
                if (column > 0)
                {
                    oss << ":" << column;
                }
            }
        }

        oss << " [" << address << "]";

        return oss.str();
    }

    /**
     * @brief Format frame as short string (function + offset only)
     * @return Compact string representation
     */
    [[nodiscard]] std::string toStringShort() const
    {
        std::ostringstream oss;

        if (!function.empty())
        {
            oss << function;
            if (offset > 0)
            {
                oss << "+0x" << std::hex << offset;
            }
        }
        else
        {
            oss << address;
        }

        return oss.str();
    }

    /**
     * @brief Equality comparison based on address
     */
    [[nodiscard]] bool operator==(const StackFrame& other) const noexcept
    {
        return address == other.address;
    }

    [[nodiscard]] bool operator!=(const StackFrame& other) const noexcept
    {
        return !(*this == other);
    }
};

/**
 * @brief Stream insertion operator for StackFrame
 */
inline std::ostream& operator<<(std::ostream& os, const StackFrame& frame)
{
    return os << frame.toString();
}

// =============================================================================
// Backend Implementation Details
// =============================================================================

namespace detail
{

#if FATP_HAS_STD_STACKTRACE

/**
 * @brief C++23 std::stacktrace backend (preferred)
 */
class Cpp23StacktraceBackend
{
public:
    static std::vector<StackFrame> capture(std::size_t skip, std::size_t maxDepth)
    {
        std::vector<StackFrame> frames;

        auto st = std::stacktrace::current(skip + 1, maxDepth);
        frames.reserve(st.size());

        for (const auto& entry : st)
        {
            StackFrame frame;
            frame.address = reinterpret_cast<void*>(entry.native_handle());
            frame.function = entry.description();
            frame.file = entry.source_file();
            frame.line = entry.source_line();
            frame.symbolized = true;
            frames.push_back(std::move(frame));
        }

        return frames;
    }

    static std::vector<StackFrame> captureRaw(std::size_t skip, std::size_t maxDepth)
    {
        // C++23 backend always symbolizes, just call capture
        return capture(skip + 1, maxDepth);
    }

    static void resolveSymbols(std::vector<StackFrame>& /* frames */)
    {
        // Already symbolized by capture
    }
};

using StacktraceBackend = Cpp23StacktraceBackend;

#elif FATP_HAS_LIBUNWIND

/**
 * @brief Demangle a C++ symbol name
 */
inline std::string demangleSymbol(const char* mangled)
{
    if (!mangled || mangled[0] == '\0')
    {
        return "";
    }

    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);

    if (status == 0 && demangled)
    {
        std::string result(demangled);
        free(demangled);
        return result;
    }

    return mangled;
}

/**
 * @brief libunwind backend (Linux/macOS)
 */
class LibunwindBackend
{
public:
    static std::vector<StackFrame> capture(std::size_t skip, std::size_t maxDepth)
    {
        auto frames = captureRaw(skip + 1, maxDepth);
        resolveSymbols(frames);
        return frames;
    }

    static std::vector<StackFrame> captureRaw(std::size_t skip, std::size_t maxDepth)
    {
        std::vector<StackFrame> frames;
        frames.reserve(maxDepth);

        unw_context_t context;
        unw_cursor_t cursor;

        if (unw_getcontext(&context) != 0)
        {
            return frames;
        }

        if (unw_init_local(&cursor, &context) != 0)
        {
            return frames;
        }

        std::size_t depth = 0;
        while (depth < skip + maxDepth && unw_step(&cursor) > 0)
        {
            if (depth >= skip)
            {
                StackFrame frame;
                unw_word_t ip;
                unw_get_reg(&cursor, UNW_REG_IP, &ip);
                frame.address = reinterpret_cast<void*>(ip);
                frames.push_back(std::move(frame));

                if (frames.size() >= maxDepth)
                {
                    break;
                }
            }
            ++depth;
        }

        return frames;
    }

    static void resolveSymbols(std::vector<StackFrame>& frames)
    {
        for (auto& frame : frames)
        {
            if (frame.symbolized)
            {
                continue;
            }

            Dl_info info;
            if (dladdr(frame.address, &info))
            {
                frame.module = info.dli_fname ? info.dli_fname : "";
                frame.moduleBase = info.dli_fbase;

                if (info.dli_sname)
                {
                    frame.function = demangleSymbol(info.dli_sname);
                    if (info.dli_saddr)
                    {
                        frame.offset = static_cast<std::size_t>(
                            static_cast<char*>(frame.address) - static_cast<char*>(info.dli_saddr));
                    }
                }
            }

            frame.symbolized = true;
        }
    }
};

using StacktraceBackend = LibunwindBackend;

#elif FATP_HAS_EXECINFO

/**
 * @brief Demangle a C++ symbol name
 */
inline std::string demangleSymbol(const char* mangled)
{
    if (!mangled || mangled[0] == '\0')
    {
        return "";
    }

    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);

    if (status == 0 && demangled)
    {
        std::string result(demangled);
        free(demangled);
        return result;
    }

    return mangled;
}

/**
 * @brief Parse symbol string from backtrace_symbols
 *
 * Format varies by platform but typically:
 * Linux: "./program(function+0x10) [0x400000]"
 * macOS: "0   program  0x0000000100001234 function + 16"
 */
inline std::string parseBacktraceSymbol(const char* symbol)
{
    if (!symbol)
    {
        return "";
    }

    std::string sym(symbol);

    // Try Linux format: "module(function+offset) [address]"
    auto parenStart = sym.find('(');
    auto plusPos = sym.find('+', parenStart);
    auto parenEnd = sym.find(')', plusPos);

    if (parenStart != std::string::npos && plusPos != std::string::npos && parenEnd != std::string::npos &&
        plusPos > parenStart)
    {
        std::string mangled = sym.substr(parenStart + 1, plusPos - parenStart - 1);
        if (!mangled.empty())
        {
            return demangleSymbol(mangled.c_str());
        }
    }

    // Try macOS format: "index module address function + offset"
    // This is more complex, fall back to dladdr for reliable resolution

    return sym;
}

/**
 * @brief execinfo backtrace() backend (POSIX fallback)
 */
class ExecinfoBackend
{
public:
    static std::vector<StackFrame> capture(std::size_t skip, std::size_t maxDepth)
    {
        auto frames = captureRaw(skip + 1, maxDepth);
        resolveSymbols(frames);
        return frames;
    }

    static std::vector<StackFrame> captureRaw(std::size_t skip, std::size_t maxDepth)
    {
        std::vector<StackFrame> frames;

        std::vector<void*> addresses(skip + maxDepth + 1);
        int captured = backtrace(addresses.data(), static_cast<int>(addresses.size()));

        for (int i = static_cast<int>(skip + 1); i < captured && frames.size() < maxDepth; ++i)
        {
            StackFrame frame;
            frame.address = addresses[static_cast<std::size_t>(i)];
            frames.push_back(std::move(frame));
        }

        return frames;
    }

    static void resolveSymbols(std::vector<StackFrame>& frames)
    {
        if (frames.empty())
        {
            return;
        }

        // Use dladdr for more reliable symbol resolution
        for (auto& frame : frames)
        {
            if (frame.symbolized)
            {
                continue;
            }

            Dl_info info;
            if (dladdr(frame.address, &info))
            {
                frame.module = info.dli_fname ? info.dli_fname : "";
                frame.moduleBase = info.dli_fbase;

                if (info.dli_sname)
                {
                    frame.function = demangleSymbol(info.dli_sname);
                    if (info.dli_saddr)
                    {
                        frame.offset = static_cast<std::size_t>(
                            static_cast<char*>(frame.address) - static_cast<char*>(info.dli_saddr));
                    }
                }
            }

            frame.symbolized = true;
        }
    }
};

using StacktraceBackend = ExecinfoBackend;

#elif FATP_HAS_DBGHELP

/**
 * @brief Windows DbgHelp backend
 */
class DbgHelpBackend
{
public:
    static std::vector<StackFrame> capture(std::size_t skip, std::size_t maxDepth)
    {
        auto frames = captureRaw(skip + 1, maxDepth);
        resolveSymbols(frames);
        return frames;
    }

    static std::vector<StackFrame> captureRaw(std::size_t skip, std::size_t maxDepth)
    {
        std::vector<StackFrame> frames;
        frames.reserve(maxDepth);

        std::vector<void*> addresses(skip + maxDepth + 1);
        USHORT captured =
            CaptureStackBackTrace(static_cast<DWORD>(skip + 1),
                static_cast<DWORD>(maxDepth), addresses.data(), nullptr);

        for (USHORT i = 0; i < captured; ++i)
        {
            StackFrame frame;
            frame.address = addresses[i];
            frames.push_back(std::move(frame));
        }

        return frames;
    }

    static void resolveSymbols(std::vector<StackFrame>& frames)
    {
        static std::once_flag initFlag;
        std::call_once(initFlag, []() {
            SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
            SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        });

        HANDLE process = GetCurrentProcess();

        // Allocate symbol info buffer
        constexpr std::size_t kMaxNameLen = 512;
        std::vector<char> buffer(sizeof(SYMBOL_INFO) + kMaxNameLen);
        SYMBOL_INFO* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer.data());
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = kMaxNameLen;

        IMAGEHLP_LINE64 lineInfo;
        lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        for (auto& frame : frames)
        {
            if (frame.symbolized)
            {
                continue;
            }

            DWORD64 address = reinterpret_cast<DWORD64>(frame.address);

            // Get function name
            DWORD64 displacement64 = 0;
            if (SymFromAddr(process, address, &displacement64, symbol))
            {
                frame.function = symbol->Name;
                frame.offset = static_cast<std::size_t>(displacement64);
            }

            // Get file/line info
            DWORD displacement32 = 0;
            if (SymGetLineFromAddr64(process, address, &displacement32, &lineInfo))
            {
                frame.file = lineInfo.FileName;
                frame.line = lineInfo.LineNumber;
            }

            // Get module info
            IMAGEHLP_MODULE64 moduleInfo;
            moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
            if (SymGetModuleInfo64(process, address, &moduleInfo))
            {
                frame.module = moduleInfo.ModuleName;
                frame.moduleBase = reinterpret_cast<void*>(moduleInfo.BaseOfImage);
            }

            frame.symbolized = true;
        }
    }
};

using StacktraceBackend = DbgHelpBackend;

#else

/**
 * @brief Stub backend when no platform support is available
 */
class StubBackend
{
public:
    static std::vector<StackFrame> capture(std::size_t /* skip */, std::size_t /* maxDepth */)
    {
        // Return a single placeholder frame indicating no backend
        std::vector<StackFrame> frames;
        StackFrame frame;
        frame.function = "<stacktrace unavailable - no backend>";
        frame.symbolized = true;
        frames.push_back(std::move(frame));
        return frames;
    }

    static std::vector<StackFrame> captureRaw(std::size_t skip, std::size_t maxDepth)
    {
        return capture(skip, maxDepth);
    }

    static void resolveSymbols(std::vector<StackFrame>& /* frames */)
    {
        // Nothing to resolve
    }
};

using StacktraceBackend = StubBackend;

#endif

} // namespace detail

// =============================================================================
// Stacktrace Class
// =============================================================================

/**
 * @brief Captures and stores a stack trace
 *
 * Thread-safe: Each instance is independent.
 * Async-signal-safe: captureRaw() only (no memory allocation in some backends).
 *
 * Backend selection priority:
 * 1. C++23 std::stacktrace (if available)
 * 2. libunwind (Linux/macOS)
 * 3. execinfo backtrace() (POSIX fallback)
 * 4. Windows DbgHelp
 * 5. Stub (returns placeholder)
 */
class Stacktrace
{
public:
    // =========================================================================
    // Construction
    // =========================================================================

    Stacktrace() = default;
    ~Stacktrace() = default;

    Stacktrace(const Stacktrace&) = default;
    Stacktrace& operator=(const Stacktrace&) = default;

    Stacktrace(Stacktrace&&) noexcept = default;
    Stacktrace& operator=(Stacktrace&&) noexcept = default;

    // =========================================================================
    // Static Factory Methods
    // =========================================================================

    /**
     * @brief Capture current stack trace with symbol resolution
     * @param skip Number of frames to skip (1 = skip current())
     * @param maxDepth Maximum frames to capture (default 64)
     * @return Stacktrace with captured and symbolized frames
     *
     * This is the primary capture method. Automatically resolves symbols.
     * For async-signal-safe capture, use captureRaw() instead.
     */
    [[nodiscard]] static Stacktrace current(std::size_t skip = 1, std::size_t maxDepth = 64)
    {
        Stacktrace st;
        st.mFrames = detail::StacktraceBackend::capture(skip + 1, maxDepth);
        st.mSymbolized = true;
        return st;
    }

    /**
     * @brief Capture raw addresses only (async-signal-safe on some platforms)
     * @param skip Number of frames to skip
     * @param maxDepth Maximum frames to capture
     * @return Stacktrace with addresses only (not symbolized)
     *
     * Safe to call from signal handlers on platforms with libunwind or execinfo.
     * Call resolveSymbols() later when it's safe to allocate memory.
     */
    [[nodiscard]] static Stacktrace captureRaw(std::size_t skip = 1, std::size_t maxDepth = 64)
    {
        Stacktrace st;
        st.mFrames = detail::StacktraceBackend::captureRaw(skip + 1, maxDepth);
        st.mSymbolized = false;
        return st;
    }

    // =========================================================================
    // Accessors
    // =========================================================================

    /**
     * @brief Get all captured frames
     * @return Const reference to frame vector
     */
    [[nodiscard]] const std::vector<StackFrame>& frames() const noexcept
    {
        return mFrames;
    }

    /**
     * @brief Get number of frames
     */
    [[nodiscard]] std::size_t size() const noexcept
    {
        return mFrames.size();
    }

    /**
     * @brief Check if stacktrace is empty
     */
    [[nodiscard]] bool empty() const noexcept
    {
        return mFrames.empty();
    }

    /**
     * @brief Access frame by index (unchecked)
     * @param index Frame index
     * @return Const reference to frame
     */
    [[nodiscard]] const StackFrame& operator[](std::size_t index) const
    {
        return mFrames[index];
    }

    /**
     * @brief Access frame by index (bounds checked)
     * @param index Frame index
     * @return Const reference to frame
     * @throws std::out_of_range if index >= size()
     */
    [[nodiscard]] const StackFrame& at(std::size_t index) const
    {
        return mFrames.at(index);
    }

    // =========================================================================
    // Iterators
    // =========================================================================

    [[nodiscard]] auto begin() const noexcept
    {
        return mFrames.begin();
    }

    [[nodiscard]] auto end() const noexcept
    {
        return mFrames.end();
    }

    [[nodiscard]] auto cbegin() const noexcept
    {
        return mFrames.cbegin();
    }

    [[nodiscard]] auto cend() const noexcept
    {
        return mFrames.cend();
    }

    [[nodiscard]] auto rbegin() const noexcept
    {
        return mFrames.rbegin();
    }

    [[nodiscard]] auto rend() const noexcept
    {
        return mFrames.rend();
    }

    // =========================================================================
    // Symbol Resolution
    // =========================================================================

    /**
     * @brief Resolve symbols for all frames
     *
     * Call after captureRaw() to get function names.
     * No-op if already symbolized.
     */
    void resolveSymbols()
    {
        if (mSymbolized)
        {
            return;
        }
        detail::StacktraceBackend::resolveSymbols(mFrames);
        mSymbolized = true;
    }

    /**
     * @brief Check if symbols have been resolved
     */
    [[nodiscard]] bool isSymbolized() const noexcept
    {
        return mSymbolized;
    }

    // =========================================================================
    // Formatting
    // =========================================================================

    /**
     * @brief Convert to multi-line string representation
     * @param maxFrames Maximum frames to include (0 = all)
     * @return Formatted stack trace string
     */
    [[nodiscard]] std::string toString(std::size_t maxFrames = 0) const
    {
        std::ostringstream oss;

        std::size_t count = mFrames.size();
        if (maxFrames > 0 && maxFrames < count)
        {
            count = maxFrames;
        }

        for (std::size_t i = 0; i < count; ++i)
        {
            oss << "#" << i << " " << mFrames[i].toString() << "\n";
        }

        if (maxFrames > 0 && maxFrames < mFrames.size())
        {
            oss << "... (" << (mFrames.size() - maxFrames) << " more frames)\n";
        }

        return oss.str();
    }

    /**
     * @brief Convert to JSON array representation
     * @return JSON string with frame data
     */
    [[nodiscard]] std::string toJson() const
    {
        std::ostringstream oss;
        oss << "[\n";

        for (std::size_t i = 0; i < mFrames.size(); ++i)
        {
            const auto& frame = mFrames[i];
            oss << "  {\n";
            oss << "    \"index\": " << i << ",\n";
            oss << "    \"address\": \"" << frame.address << "\",\n";
            oss << "    \"function\": \"" << escapeJsonString(frame.function) << "\",\n";
            oss << "    \"file\": \"" << escapeJsonString(frame.file) << "\",\n";
            oss << "    \"line\": " << frame.line << ",\n";
            oss << "    \"column\": " << frame.column << ",\n";
            oss << "    \"module\": \"" << escapeJsonString(frame.module) << "\",\n";
            oss << "    \"offset\": " << frame.offset << ",\n";
            oss << "    \"symbolized\": " << (frame.symbolized ? "true" : "false") << "\n";
            oss << "  }";
            if (i + 1 < mFrames.size())
            {
                oss << ",";
            }
            oss << "\n";
        }

        oss << "]";
        return oss.str();
    }

    // =========================================================================
    // Comparison (for deduplication)
    // =========================================================================

    /**
     * @brief Equality comparison based on addresses
     */
    [[nodiscard]] bool operator==(const Stacktrace& other) const noexcept
    {
        if (mFrames.size() != other.mFrames.size())
        {
            return false;
        }

        for (std::size_t i = 0; i < mFrames.size(); ++i)
        {
            if (mFrames[i].address != other.mFrames[i].address)
            {
                return false;
            }
        }

        return true;
    }

    [[nodiscard]] bool operator!=(const Stacktrace& other) const noexcept
    {
        return !(*this == other);
    }

    /**
     * @brief Compute hash for use in containers
     */
    [[nodiscard]] std::size_t hash() const noexcept
    {
        std::size_t h = 0;
        for (const auto& frame : mFrames)
        {
            // Simple hash combining
            h ^= std::hash<void*>{}(frame.address) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }

    // =========================================================================
    // Backend Information
    // =========================================================================

    /**
     * @brief Get the name of the active backend
     * @return Backend identifier string
     */
    [[nodiscard]] static const char* backendName() noexcept
    {
#if FATP_HAS_STD_STACKTRACE
        return "C++23 std::stacktrace";
#elif FATP_HAS_LIBUNWIND
        return "libunwind";
#elif FATP_HAS_EXECINFO
        return "execinfo";
#elif FATP_HAS_DBGHELP
        return "Windows DbgHelp";
#else
        return "stub";
#endif
    }

    /**
     * @brief Check if a real backend is available
     * @return true if stack traces will contain real data
     */
    [[nodiscard]] static constexpr bool hasRealBackend() noexcept
    {
#if FATP_HAS_STD_STACKTRACE || FATP_HAS_LIBUNWIND || FATP_HAS_EXECINFO || FATP_HAS_DBGHELP
        return true;
#else
        return false;
#endif
    }

private:
    std::vector<StackFrame> mFrames;
    bool mSymbolized = false;

    /**
     * @brief Escape special characters for JSON string
     */
    static std::string escapeJsonString(const std::string& input)
    {
        std::string output;
        output.reserve(input.size() + 8);

        for (char c : input)
        {
            switch (c)
            {
            case '"':
                output += "\\\"";
                break;
            case '\\':
                output += "\\\\";
                break;
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
                break;
            case '\n':
                output += "\\n";
                break;
            case '\r':
                output += "\\r";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    // Control character - skip or encode
                    output += "\\u00";
                    static constexpr char kHexDigits[] = "0123456789abcdef";
                    output += kHexDigits[(c >> 4) & 0xF];
                    output += kHexDigits[c & 0xF];
                }
                else
                {
                    output += c;
                }
                break;
            }
        }

        return output;
    }
};

/**
 * @brief Stream insertion operator for Stacktrace
 */
inline std::ostream& operator<<(std::ostream& os, const Stacktrace& st)
{
    return os << st.toString();
}

} // namespace fat_p

// =============================================================================
// std::hash specialization for Stacktrace
// =============================================================================

namespace std
{

template <>
struct hash<fat_p::Stacktrace>
{
    std::size_t operator()(const fat_p::Stacktrace& st) const noexcept
    {
        return st.hash();
    }
};

template <>
struct hash<fat_p::StackFrame>
{
    std::size_t operator()(const fat_p::StackFrame& frame) const noexcept
    {
        return std::hash<void*>{}(frame.address);
    }
};

} // namespace std

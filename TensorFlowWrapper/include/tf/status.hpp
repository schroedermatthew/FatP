// tf/status.hpp
// RAII wrapper for TF_Status with exception-based error handling
//
// Fixes applied:
// - P0: TF_Status always deleted (no leaks on success or error path)
// - P3: Human-readable error code names
// - V5: string_view safety fix (makes owned copy for null-termination)

#pragma once

#include <new>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

extern "C" {
#include <tensorflow/c/c_api.h>
}

#include "tf/format.hpp"

namespace tf {

// ============================================================================
// Status - RAII wrapper for TF_Status*
// ============================================================================

class Status {
public:
    /// Create a new status (initially OK)
    Status() : st_(TF_NewStatus()) {
        if (!st_) throw std::bad_alloc();
    }

    ~Status() {
        if (st_) TF_DeleteStatus(st_);
    }

    // Non-copyable
    Status(const Status&) = delete;
    Status& operator=(const Status&) = delete;

    // Movable
    Status(Status&& other) noexcept : st_(other.st_) {
        other.st_ = nullptr;
    }

    Status& operator=(Status&& other) noexcept {
        if (this != &other) {
            if (st_) TF_DeleteStatus(st_);
            st_ = other.st_;
            other.st_ = nullptr;
        }
        return *this;
    }

    // ─────────────────────────────────────────────────────────────────
    // Accessors
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] TF_Status* get() noexcept { return st_; }
    [[nodiscard]] const TF_Status* get() const noexcept { return st_; }
    [[nodiscard]] TF_Code code() const noexcept { return TF_GetCode(st_); }
    [[nodiscard]] const char* code_name() const noexcept { return code_to_string(code()); }
    [[nodiscard]] const char* message() const noexcept { return TF_Message(st_); }
    [[nodiscard]] bool ok() const noexcept { return code() == TF_OK; }
    [[nodiscard]] explicit operator bool() const noexcept { return ok(); }
    [[nodiscard]] bool operator!() const noexcept { return !ok(); }

    // ─────────────────────────────────────────────────────────────────
    // Mutation
    // ─────────────────────────────────────────────────────────────────

    void reset() noexcept {
        TF_SetStatus(st_, TF_OK, "");
    }

    void set(TF_Code code, const char* msg = "") noexcept {
        TF_SetStatus(st_, code, msg ? msg : "");
    }

    void set(TF_Code code, const std::string& msg) noexcept {
        TF_SetStatus(st_, code, msg.c_str());
    }

    /// Set status with string_view - makes owned copy for null-termination safety
    void set(TF_Code code, std::string_view msg) {
        if (msg.empty()) {
            TF_SetStatus(st_, code, "");
            return;
        }
        std::string tmp(msg);
        TF_SetStatus(st_, code, tmp.c_str());
    }

    // ─────────────────────────────────────────────────────────────────
    // Error handling
    // ─────────────────────────────────────────────────────────────────

    void throw_if_error(
        std::string_view context = "",
        std::source_location loc = std::source_location::current()) const
    {
        if (ok()) return;

        std::string msg;
        if (context.empty()) {
            msg = tf::detail::format("[TF_{}] at {}:{} in {}: {}",
                code_name(), loc.file_name(), loc.line(),
                loc.function_name(), message());
        } else {
            msg = tf::detail::format("[TF_{}] {} at {}:{}: {}",
                code_name(), context, loc.file_name(), loc.line(), message());
        }
        throw std::runtime_error(std::move(msg));
    }

    void ThrowIfNotOK(
        std::string_view context = "",
        std::source_location loc = std::source_location::current()) const
    {
        throw_if_error(context, loc);
    }

    // ─────────────────────────────────────────────────────────────────
    // Static helpers
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] static constexpr const char* code_to_string(TF_Code code) noexcept {
        switch (code) {
            case TF_OK:                  return "OK";
            case TF_CANCELLED:           return "CANCELLED";
            case TF_UNKNOWN:             return "UNKNOWN";
            case TF_INVALID_ARGUMENT:    return "INVALID_ARGUMENT";
            case TF_DEADLINE_EXCEEDED:   return "DEADLINE_EXCEEDED";
            case TF_NOT_FOUND:           return "NOT_FOUND";
            case TF_ALREADY_EXISTS:      return "ALREADY_EXISTS";
            case TF_PERMISSION_DENIED:   return "PERMISSION_DENIED";
            case TF_UNAUTHENTICATED:     return "UNAUTHENTICATED";
            case TF_RESOURCE_EXHAUSTED:  return "RESOURCE_EXHAUSTED";
            case TF_FAILED_PRECONDITION: return "FAILED_PRECONDITION";
            case TF_ABORTED:             return "ABORTED";
            case TF_OUT_OF_RANGE:        return "OUT_OF_RANGE";
            case TF_UNIMPLEMENTED:       return "UNIMPLEMENTED";
            case TF_INTERNAL:            return "INTERNAL";
            case TF_UNAVAILABLE:         return "UNAVAILABLE";
            case TF_DATA_LOSS:           return "DATA_LOSS";
            default:                     return "UNKNOWN_CODE";
        }
    }

private:
    TF_Status* st_{nullptr};
};

// ============================================================================
// Free function helpers
// ============================================================================

inline void throw_if_error(
    const Status& st,
    std::string_view context = "",
    std::source_location loc = std::source_location::current())
{
    st.throw_if_error(context, loc);
}

inline void consume_status(
    TF_Status* st,
    std::string_view context = "",
    std::source_location loc = std::source_location::current())
{
    if (!st) return;

    const bool is_ok = TF_GetCode(st) == TF_OK;
    const char* msg = TF_Message(st);
    
    if (!is_ok) {
        std::string error = tf::detail::format("{} at {}:{}: {}",
            context, loc.file_name(), loc.line(), msg);
        TF_DeleteStatus(st);
        throw std::runtime_error(std::move(error));
    }
    
    TF_DeleteStatus(st);
}

} // namespace tf

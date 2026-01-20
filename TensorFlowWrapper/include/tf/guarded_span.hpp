// tf/guarded_span.hpp
// Thread-safe view into data - holds lock for entire view lifetime
//
// MERGED IMPLEMENTATION - Best of ChatGPT + Claude:
// - ChatGPT: Clean core implementation, [[no_unique_address]]
// - Claude: Full STL iterator support, at() with bounds checking
//
// This is the KEY ABSTRACTION that makes thread-safety claims HONEST:
// The lock is held for the ENTIRE lifetime of the view, not just
// during pointer retrieval.

#pragma once

#include <cstddef>
#include <iterator>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace tf {

// ============================================================================
// GuardedSpan - A span that holds a lock guard for its lifetime
// ============================================================================

template<class T, class Guard>
class GuardedSpan {
public:
    // ─────────────────────────────────────────────────────────────────
    // STL-compatible type aliases
    // ─────────────────────────────────────────────────────────────────
    
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // ─────────────────────────────────────────────────────────────────
    // Construction
    // ─────────────────────────────────────────────────────────────────

    /// Construct from span and guard (takes ownership of guard)
    GuardedSpan(std::span<T> s, Guard g) noexcept(std::is_nothrow_move_constructible_v<Guard>)
        : span_(s), guard_(std::move(g)) {}

    /// Non-copyable (can't duplicate the lock)
    GuardedSpan(const GuardedSpan&) = delete;
    GuardedSpan& operator=(const GuardedSpan&) = delete;

    /// Movable (transfers lock ownership)
    GuardedSpan(GuardedSpan&&) noexcept = default;
    GuardedSpan& operator=(GuardedSpan&&) noexcept = default;

    /// Destructor releases the lock
    ~GuardedSpan() = default;

    // ─────────────────────────────────────────────────────────────────
    // Element access
    // ─────────────────────────────────────────────────────────────────

    /// Unchecked element access
    [[nodiscard]] constexpr reference operator[](size_type i) const noexcept {
        return span_[i];
    }

    /// Checked element access (throws std::out_of_range)
    [[nodiscard]] constexpr reference at(size_type i) const {
        if (i >= size()) {
            throw std::out_of_range("GuardedSpan::at: index out of range");
        }
        return span_[i];
    }

    [[nodiscard]] constexpr reference front() const noexcept { return span_.front(); }
    [[nodiscard]] constexpr reference back() const noexcept { return span_.back(); }
    [[nodiscard]] constexpr pointer data() const noexcept { return span_.data(); }

    // ─────────────────────────────────────────────────────────────────
    // Span access
    // ─────────────────────────────────────────────────────────────────

    /// Get the underlying span
    [[nodiscard]] constexpr std::span<T> span() const noexcept { return span_; }
    
    /// Implicit conversion to span (for algorithms)
    [[nodiscard]] constexpr operator std::span<T>() const noexcept { return span_; }

    // ─────────────────────────────────────────────────────────────────
    // Iterators
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr iterator begin() const noexcept { return span_.data(); }
    [[nodiscard]] constexpr iterator end() const noexcept { return span_.data() + span_.size(); }
    [[nodiscard]] constexpr const_iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] constexpr const_iterator cend() const noexcept { return end(); }

    [[nodiscard]] constexpr reverse_iterator rbegin() const noexcept {
        return reverse_iterator(end());
    }
    [[nodiscard]] constexpr reverse_iterator rend() const noexcept {
        return reverse_iterator(begin());
    }
    [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
        return const_reverse_iterator(cend());
    }
    [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
        return const_reverse_iterator(cbegin());
    }

    // ─────────────────────────────────────────────────────────────────
    // Capacity
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr size_type size() const noexcept { return span_.size(); }
    [[nodiscard]] constexpr size_type size_bytes() const noexcept { return span_.size_bytes(); }
    [[nodiscard]] constexpr bool empty() const noexcept { return span_.empty(); }

    // ─────────────────────────────────────────────────────────────────
    // Subviews (return raw spans - guard still held by this object)
    // ─────────────────────────────────────────────────────────────────

    [[nodiscard]] constexpr std::span<T> first(size_type count) const noexcept {
        return span_.first(count);
    }

    [[nodiscard]] constexpr std::span<T> last(size_type count) const noexcept {
        return span_.last(count);
    }

    [[nodiscard]] constexpr std::span<T> subspan(
        size_type offset, 
        size_type count = std::dynamic_extent) const noexcept 
    {
        return span_.subspan(offset, count);
    }

private:
    std::span<T> span_;
    [[no_unique_address]] Guard guard_;  // Zero-cost for NoLock::guard
};

// Deduction guide
template<class T, class Guard>
GuardedSpan(std::span<T>, Guard) -> GuardedSpan<T, Guard>;

} // namespace tf

/**
 * @file ContractException.h
 * @brief Defines the base exception classes for the policy-based contract
 * enforcement system.
 *
 * This file introduces ContractViolationError, which allows custom
 * contract exceptions to inherit from standard C++ exception types
 * (std::logic_error, std::runtime_error, std::bad_alloc) based on the context
 * of the failure.
 * 
 * @section usage Usage Examples:
 * @code
 * // Throwing specific contract errors:
 * throw LogicContractError("Precondition failed: x > 0");
 * throw RuntimeContractError("File not found");
 * throw AllocContractError("Stack overflow in allocator");
 * throw OutOfRangeContractError("Index out of bounds: " + std::to_string(idx));
 * 
 * // Polymorphic catching (catch any contract violation):
 * try {
 *     some_contract_enforced_code();
 * } catch (const ContractViolationBase& e) {
 *     std::cerr << "Contract violation (" << e.category() << "): " << e.message();
 * }
 * 
 * // Specific catching:
 * try {
 *     allocate_memory();
 * } catch (const AllocContractError& e) {
 *     // Handle allocation contract failure
 * } catch (const std::bad_alloc&) {
 *     // Also catches AllocContractError (standard compliance)
 * }
 * @endcode
 * 
 * @section integration Library Integration:
 * - enforce.h: Policy-based enforcement macros (LogicRaiser, RuntimeRaiser)
 * - Expected.h: Non-throwing alternative via enforce_expected()
 * - CheckedArithmetic.h: Overflow/underflow detection
 * - Factory.h: Error reporting for factory patterns
 * 
 * @section performance Performance:
 * - Zero overhead when exceptions not thrown
 * - Throw time: ~1-5 µs (dominated by stack unwinding)
 * - Memory overhead: +8 bytes per instance (vtable pointer)
 * 
 * @note Thread-safe: Each exception is independent, safe for concurrent throwing
 * @note C++17 minimum required for std::is_base_of_v and std::is_constructible_v
 */
#pragma once

#include <stdexcept>
#include <string>
#include <type_traits>  // Required for static_assert traits

namespace fat_p {

// =============================================================================
// Polymorphic Base for All Contract Violations
// =============================================================================

class ContractViolationBase {
public:
    virtual ~ContractViolationBase() noexcept = default;
    
    virtual const char* category() const noexcept = 0;
    
    virtual const char* message() const noexcept = 0;
};

// =============================================================================
// Core Templated Exception Structure
// =============================================================================

template <typename T>
class ContractViolationError : public T, public ContractViolationBase {
    static_assert(std::is_base_of_v<std::exception, T>,
        "T must inherit from std::exception.");
    static_assert(std::is_constructible_v<T, const std::string&>,
        "T must be constructible from const std::string&.");

public:
    explicit ContractViolationError(const std::string& message)
        : T("Contract Violation: " + message) {}

    virtual ~ContractViolationError() noexcept = default;
    
    const char* category() const noexcept override {
        if constexpr (std::is_base_of_v<std::logic_error, T>) {
            return "Logic";
        } else if constexpr (std::is_base_of_v<std::runtime_error, T>) {
            return "Runtime";
        } else {
            return "Unknown";
        }
    }
    
    const char* message() const noexcept override {
        return this->what();
    }
};

// =============================================================================
// Type Aliases for Common Use Cases
// =============================================================================

using LogicContractError = ContractViolationError<std::logic_error>;

using RuntimeContractError = ContractViolationError<std::runtime_error>;

using DomainContractError = ContractViolationError<std::domain_error>;

using OutOfRangeContractError = ContractViolationError<std::out_of_range>;

using InvalidArgumentContractError = ContractViolationError<std::invalid_argument>;

using OverflowContractError = ContractViolationError<std::overflow_error>;

using UnderflowContractError = ContractViolationError<std::underflow_error>;

// =============================================================================
// Specialized Allocation Contract Error
// =============================================================================

class AllocContractError : public std::bad_alloc, public ContractViolationBase {
private:
    std::string full_message_;

public:
    explicit AllocContractError(const std::string& message)
        : full_message_("Contract Violation: Bad Allocation: " + message) {}

    virtual ~AllocContractError() noexcept = default;

    const char* what() const noexcept override {
        return full_message_.c_str();
    }

    const char* category() const noexcept override {
        return "Allocation";
    }
    
    const char* message() const noexcept override {
        return full_message_.c_str();
    }
};

} // namespace fat_p
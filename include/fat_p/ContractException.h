#pragma once

/*
FATP_META:
  meta_version: 1
  component: ContractException
  file_role: public_header
  path: include/fat_p/ContractException.h
  namespace: fat_p
  layer: Foundation
  summary: "Public header for ContractException."
  api_stability: in_work
  related:
    docs_search: "ContractException"
    tests:
      - components/ContractException/tests/test_ContractException.cpp
      - components/Enforce/tests/test_Enforce.cpp
  hygiene:
    pragma_once: true
    include_guard: false
    defines_total: 0
    defines_unprefixed: 0
    undefs_total: 0
    includes_windows_h: false
  generated:
    by: fatp-meta-tool
    mode: autogen
*/

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
 *
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
 *     std::cerr << e;  // Uses operator<<
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
 * - Throw time: ~1-5 us (dominated by stack unwinding)
 * - Memory overhead: +8 bytes per instance (vtable pointer)
 *
 * @note Thread-safe: Each exception is independent, safe for concurrent throwing
 * @note C++20 minimum required for std::is_base_of_v and std::is_constructible_v
 */

#include <ostream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace fat_p
{

// =============================================================================
// Polymorphic Base for All Contract Violations
// =============================================================================

/**
 * @brief Abstract base class for all contract violation exceptions.
 *
 * Enables unified handling of contract violations regardless of which
 * standard exception type they inherit from. Use this in catch blocks
 * when you want to handle any contract violation uniformly.
 *
 * @note This class does not inherit from std::exception to avoid the
 *       diamond inheritance problem. Use dynamic_cast if you need
 *       std::exception access from a ContractViolationBase reference.
 */
class ContractViolationBase
{
public:
    virtual ~ContractViolationBase() noexcept = default;

    ContractViolationBase() noexcept = default;
    ContractViolationBase(const ContractViolationBase&) noexcept = default;
    ContractViolationBase(ContractViolationBase&&) noexcept = default;
    ContractViolationBase& operator=(const ContractViolationBase&) noexcept = default;
    ContractViolationBase& operator=(ContractViolationBase&&) noexcept = default;

    /**
     * @brief Returns the violation category.
     * @return "Logic", "Runtime", "Allocation", or "Unknown"
     */
    virtual const char* category() const noexcept = 0;

    /**
     * @brief Returns the full error message.
     * @return The complete message including "Contract Violation:" prefix
     */
    virtual const char* message() const noexcept = 0;
};

/**
 * @brief Stream insertion operator for ContractViolationBase.
 *
 * Formats output as: [Category] Message
 *
 * @code
 * catch (const fat_p::ContractViolationBase& e) {
 *     std::cerr << e << '\n';
 *     // Output: [Logic] Contract Violation: Precondition failed
 * }
 * @endcode
 */
inline std::ostream& operator<<(std::ostream& os, const ContractViolationBase& e)
{
    return os << "[" << e.category() << "] " << e.message();
}

// =============================================================================
// Core Templated Exception Structure
// =============================================================================

/**
 * @brief Template for creating contract exceptions from standard exception types.
 *
 * Uses dual inheritance to provide both standard exception compatibility
 * and unified contract violation handling.
 *
 * @tparam T Base exception type (must inherit from std::exception and be
 *           constructible from const std::string&)
 */
template <typename T>
class ContractViolationError : public T, public ContractViolationBase
{
    static_assert(std::is_base_of_v<std::exception, T>, "T must inherit from std::exception.");
    static_assert(std::is_constructible_v<T, const std::string&>,
                  "T must be constructible from const std::string& "
                  "(use AllocContractError for std::bad_alloc).");

public:
    /**
     * @brief Constructs a contract violation exception.
     * @param message The error message (will be prefixed with "Contract Violation: ")
     */
    explicit ContractViolationError(const std::string& message)
        : T("Contract Violation: " + message)
    {
    }

    virtual ~ContractViolationError() noexcept = default;

    ContractViolationError(const ContractViolationError&) = default;
    ContractViolationError& operator=(const ContractViolationError&) = default;

    ContractViolationError(ContractViolationError&&) noexcept = default;
    ContractViolationError& operator=(ContractViolationError&&) noexcept = default;

    /**
     * @brief Returns the violation category based on the base exception type.
     * @return "Logic" for std::logic_error derivatives,
     *         "Runtime" for std::runtime_error derivatives,
     *         "Unknown" otherwise
     */
    const char* category() const noexcept override
    {
        if constexpr (std::is_base_of_v<std::logic_error, T>)
        {
            return "Logic";
        }
        else if constexpr (std::is_base_of_v<std::runtime_error, T>)
        {
            return "Runtime";
        }
        else
        {
            return "Unknown";
        }
    }

    /**
     * @brief Returns the full error message.
     * @return Same as what()
     */
    const char* message() const noexcept override
    {
        return this->what();
    }
};

// =============================================================================
// Type Aliases for Common Use Cases
// =============================================================================

/** @brief Contract exception for preconditions, invariants, programmer errors */
using LogicContractError = ContractViolationError<std::logic_error>;

/** @brief Contract exception for runtime/environmental failures */
using RuntimeContractError = ContractViolationError<std::runtime_error>;

/** @brief Contract exception for mathematical domain errors */
using DomainContractError = ContractViolationError<std::domain_error>;

/** @brief Contract exception for index/bounds violations */
using OutOfRangeContractError = ContractViolationError<std::out_of_range>;

/** @brief Contract exception for invalid function arguments */
using InvalidArgumentContractError = ContractViolationError<std::invalid_argument>;

/** @brief Contract exception for arithmetic overflow */
using OverflowContractError = ContractViolationError<std::overflow_error>;

/** @brief Contract exception for arithmetic underflow */
using UnderflowContractError = ContractViolationError<std::underflow_error>;

// =============================================================================
// Specialized Allocation Contract Error
// =============================================================================

/**
 * @brief Contract exception for allocation failures.
 *
 * Inherits from std::bad_alloc for standard compatibility. Unlike other
 * contract exceptions, this stores its own message string because
 * std::bad_alloc does not accept a string constructor argument.
 *
 * @note The constructor is designed to be resilient to OOM conditions.
 *       If message construction fails, a static fallback message is used.
 */
class AllocContractError : public std::bad_alloc, public ContractViolationBase
{
private:
    std::string mFullMessage;

    static constexpr const char* FALLBACK_MESSAGE = "Contract Violation: Bad Allocation (message construction failed)";

public:
    /**
     * @brief Constructs an allocation contract error.
     * @param message The error message (will be prefixed with
     *                "Contract Violation: Bad Allocation: ")
     *
     * @note If string construction throws (e.g., during OOM), a static
     *       fallback message is used to avoid std::terminate.
     */
    explicit AllocContractError(const std::string& message) noexcept
    {
        try
        {
            mFullMessage = "Contract Violation: Bad Allocation: " + message;
        }
        catch (...)
        {
            // OOM during message construction - use fallback
            mFullMessage.clear();
        }
    }

    virtual ~AllocContractError() noexcept = default;

    AllocContractError(const AllocContractError&) = default;
    AllocContractError& operator=(const AllocContractError&) = default;

    AllocContractError(AllocContractError&&) noexcept = default;
    AllocContractError& operator=(AllocContractError&&) noexcept = default;

    const char* what() const noexcept override
    {
        return mFullMessage.empty() ? FALLBACK_MESSAGE : mFullMessage.c_str();
    }

    const char* category() const noexcept override
    {
        return "Allocation";
    }

    const char* message() const noexcept override
    {
        return what();
    }
};

} // namespace fat_p

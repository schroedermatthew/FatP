/**
 * @file IdGenerator.h  
 * @brief Policy-based unique identifier generator with type-safe IDs and recycling.
 * @version 2.0
 */

#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <deque>
#include <type_traits>
#include <stdexcept>
#include <mutex>

#include "Expected.h"
#include "StrongId.h"
#include "ConcurrencyPolicies.h"

namespace fat_p {

    // =============================================================================
    // Error Types
    // =============================================================================

    enum class IdError {
        Overflow,
        InvalidRelease,
        Corruption,
        AlreadyInUse,
        NotInitialized
    };

    class IdGeneratorException : public std::runtime_error {
    public:
        explicit IdGeneratorException(IdError error, const std::string& message = "")
            : std::runtime_error(format_message(error, message)), error_(error) {}

        IdError error() const noexcept { return error_; }

    private:
        IdError error_;

        static std::string format_message(IdError error, const std::string& msg) {
            std::string base = "IdGenerator error: ";
            switch (error) {
            case IdError::Overflow: base += "ID overflow"; break;
            case IdError::InvalidRelease: base += "Invalid ID release"; break;
            case IdError::Corruption: base += "State corruption"; break;
            case IdError::AlreadyInUse: base += "ID already in use"; break;
            case IdError::NotInitialized: base += "Not initialized"; break;
            }
            return msg.empty() ? base : base + " - " + msg;
        }
    };

    // =============================================================================
    // Helper to extract underlying integral type
    // =============================================================================

    namespace detail {
        template <typename T, typename = void>
        struct extract_value_type {
            using type = typename T::value_type;
        };

        template <typename T>
        struct extract_value_type<T, std::enable_if_t<std::is_integral_v<T>>> {
            using type = T;
        };
    }

    template <typename T>
    using underlying_id_type_t = typename detail::extract_value_type<T>::type;

    // =============================================================================
    // IdAllocationPolicy
    // =============================================================================

    template <typename IdType = uint64_t>
    class SequentialAllocationPolicy {
        static_assert(std::is_integral_v<IdType>, "IdType must be integral");

    public:
        explicit SequentialAllocationPolicy(IdType base_id = 0)
            : base_id_(base_id), next_id_(base_id) {}

        std::optional<IdType> next_id(IdType max_id, bool first_call = false) noexcept {
            IdType candidate;
            
            if (first_call || next_id_ > max_id) {
                // Use next_id_ for fresh generation or first call
                candidate = next_id_;
            } else {
                // Continue from max_id + 1
                if (max_id == std::numeric_limits<IdType>::max()) {
                    return std::nullopt;  // Can't go past max
                }
                candidate = max_id + 1;
            }
            
            // Update next_id_ for next call
            if (candidate == std::numeric_limits<IdType>::max()) {
                next_id_ = candidate;  // Stay at max
            } else {
                next_id_ = candidate + 1;
            }
            
            return candidate;
        }

        void reset(IdType base_id = 0) noexcept {
            base_id_ = base_id;
            next_id_ = base_id;
        }

    private:
        IdType base_id_;
        IdType next_id_;
    };

    template <typename IdType = uint64_t>
    class RandomAllocationPolicy {
        static_assert(std::is_integral_v<IdType>, "IdType must be integral");

    public:
        explicit RandomAllocationPolicy(IdType = 0)
            : rng_(std::random_device{}()), dist_(1, std::numeric_limits<IdType>::max()) {}

        std::optional<IdType> next_id(IdType, bool = false) noexcept {
            try {
                return dist_(rng_);
            }
            catch (...) {
                return std::nullopt;
            }
        }

        void reset(IdType = 0) noexcept {
            rng_.seed(std::random_device{}());
        }

    private:
        std::mt19937_64 rng_;
        std::uniform_int_distribution<IdType> dist_;
    };

    // =============================================================================
    // RecyclingPolicy
    // =============================================================================

    template <typename IdType = uint64_t>
    class ImmediateRecyclingPolicy {
    public:
        std::optional<IdType> get_recycled() noexcept {
            if (recycled_.empty()) return std::nullopt;
            IdType id = recycled_.front();
            recycled_.pop_front();
            return id;
        }

        void add_recycled(IdType id) noexcept {
            recycled_.push_back(id);
        }

        size_t recycled_count() const noexcept {
            return recycled_.size();
        }

        void clear() noexcept {
            recycled_.clear();
        }

    private:
        std::deque<IdType> recycled_;
    };

    template <typename IdType = uint64_t>
    class NoRecyclingPolicy {
    public:
        std::optional<IdType> get_recycled() noexcept { return std::nullopt; }
        void add_recycled(IdType) noexcept {}
        size_t recycled_count() const noexcept { return 0; }
        void clear() noexcept {}
    };

    // =============================================================================
    // ErrorPolicy
    // =============================================================================

    template <typename IdType, typename ErrorType = IdError>
    class ExpectedErrorPolicy {
    public:
        using result_type = Expected<IdType, ErrorType>;
        using void_result_type = Expected<void, ErrorType>;

        static result_type report_success(IdType id) noexcept {
            return id;
        }

        static result_type report_error(ErrorType error) noexcept {
            return make_unexpected(error);
        }
    };

    // =============================================================================
    // IdGenerator
    // =============================================================================

    template <
        typename IdType_,
        typename AllocationPolicy = SequentialAllocationPolicy<underlying_id_type_t<IdType_>>,
        typename RecyclingPolicy = ImmediateRecyclingPolicy<underlying_id_type_t<IdType_>>,
        typename ErrorPolicy = ExpectedErrorPolicy<IdType_, IdError>,
        typename ConcurrencyPolicy = SingleThreadedPolicy
    >
    class IdGenerator
        : private AllocationPolicy
        , private RecyclingPolicy
        , private ConcurrencyPolicy
    {
    public:
        using id_type = IdType_;
        using result_type = typename ErrorPolicy::result_type;
        using underlying_type = underlying_id_type_t<IdType_>;

        // =============================================================================
        // Construction
        // =============================================================================

        explicit IdGenerator(underlying_type base_id = 0)
            : AllocationPolicy(base_id)
            , RecyclingPolicy()
            , ConcurrencyPolicy()
            , base_id_(base_id)
            , ids_in_use_()
        {}

        ~IdGenerator() = default;

        IdGenerator(const IdGenerator&) = delete;
        IdGenerator& operator=(const IdGenerator&) = delete;
        IdGenerator(IdGenerator&&) noexcept = default;
        IdGenerator& operator=(IdGenerator&&) noexcept = default;

        // =============================================================================
        // ID Generation and Release
        // =============================================================================

        result_type generate() {
            typename ConcurrencyPolicy::LockGuard lock(mutex_);

            // Try recycled IDs first
            if (auto recycled = RecyclingPolicy::get_recycled()) {
                underlying_type raw_id = *recycled;
                ids_in_use_.insert(raw_id);
                
                if constexpr (std::is_same_v<IdType_, underlying_type>) {
                    return ErrorPolicy::report_success(raw_id);
                }
                else {
                    return ErrorPolicy::report_success(IdType_(raw_id));
                }
            }

            // Generate new ID
            bool is_first = ids_in_use_.empty();
            underlying_type max_id = is_first ? 0 : *ids_in_use_.rbegin();
            auto new_id_opt = AllocationPolicy::next_id(max_id, is_first);

            if (!new_id_opt) {
                return ErrorPolicy::report_error(IdError::Overflow);
            }

            underlying_type raw_id = *new_id_opt;

            // Check for collision
            if (ids_in_use_.count(raw_id) > 0) {
                return ErrorPolicy::report_error(IdError::AlreadyInUse);
            }

            ids_in_use_.insert(raw_id);

            if constexpr (std::is_same_v<IdType_, underlying_type>) {
                return ErrorPolicy::report_success(raw_id);
            }
            else {
                return ErrorPolicy::report_success(IdType_(raw_id));
            }
        }

        Expected<void, IdError> release(IdType_ id) noexcept {
            typename ConcurrencyPolicy::LockGuard lock(mutex_);

            underlying_type raw_id;
            if constexpr (std::is_same_v<IdType_, underlying_type>) {
                raw_id = id;
            }
            else {
                raw_id = id.get();
            }

            if (ids_in_use_.erase(raw_id) == 0) {
                return make_unexpected(IdError::InvalidRelease);
            }

            RecyclingPolicy::add_recycled(raw_id);
            return {};
        }

        // =============================================================================
        // Query Operations
        // =============================================================================

        bool is_active(IdType_ id) const noexcept {
            typename ConcurrencyPolicy::LockGuard lock(mutex_);
            underlying_type raw_id;
            if constexpr (std::is_same_v<IdType_, underlying_type>) {
                raw_id = id;
            }
            else {
                raw_id = id.get();
            }
            return ids_in_use_.count(raw_id) > 0;
        }

        size_t active_count() const noexcept {
            typename ConcurrencyPolicy::LockGuard lock(mutex_);
            return ids_in_use_.size();
        }

        size_t recycled_count() const noexcept {
            typename ConcurrencyPolicy::LockGuard lock(mutex_);
            return RecyclingPolicy::recycled_count();
        }

        void reset() noexcept {
            typename ConcurrencyPolicy::LockGuard lock(mutex_);
            ids_in_use_.clear();
            RecyclingPolicy::clear();
            AllocationPolicy::reset(base_id_);
        }

        // =============================================================================
        // RAII Helper
        // =============================================================================

        class IdGuard {
        public:
            explicit IdGuard(IdGenerator& gen, IdType_ id)
                : generator_(&gen), id_(id), owns_(true) {}

            ~IdGuard() {
                if (owns_) {
                    (void)generator_->release(id_);
                }
            }

            IdGuard(const IdGuard&) = delete;
            IdGuard& operator=(const IdGuard&) = delete;

            IdGuard(IdGuard&& other) noexcept
                : generator_(other.generator_), id_(other.id_), owns_(other.owns_) {
                other.owns_ = false;
            }

            IdGuard& operator=(IdGuard&& other) noexcept {
                if (this != &other) {
                    if (owns_) (void)generator_->release(id_);
                    generator_ = other.generator_;
                    id_ = other.id_;
                    owns_ = other.owns_;
                    other.owns_ = false;
                }
                return *this;
            }

            IdType_ get() const noexcept { return id_; }
            IdType_ operator*() const noexcept { return id_; }

            void release_ownership() noexcept { owns_ = false; }

        private:
            IdGenerator* generator_;
            IdType_ id_;
            bool owns_;
        };

        Expected<IdGuard, IdError> scoped_id() {
            auto result = generate();
            if constexpr (std::is_same_v<result_type, Expected<IdType_, IdError>>) {
                if (!result) return make_unexpected(result.error());
                return IdGuard(*this, *result);
            }
            else {
                return IdGuard(*this, result);
            }
        }

    private:
        underlying_type base_id_;
        std::set<underlying_type> ids_in_use_;
        mutable std::mutex mutex_;
    };

    // =============================================================================
    // Convenience Aliases
    // =============================================================================

    template <typename IdType = uint64_t>
    using SimpleIdGenerator = IdGenerator<IdType,
        SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
        ImmediateRecyclingPolicy<underlying_id_type_t<IdType>>,
        ExpectedErrorPolicy<IdType, IdError>,
        SingleThreadedPolicy>;

    template <typename IdType = uint64_t>
    using ThreadSafeIdGenerator = IdGenerator<IdType,
        SequentialAllocationPolicy<underlying_id_type_t<IdType>>,
        ImmediateRecyclingPolicy<underlying_id_type_t<IdType>>,
        ExpectedErrorPolicy<IdType, IdError>,
        MutexSynchronizationPolicy>;

    template <typename IdType = uint64_t>
    using RandomIdGenerator = IdGenerator<IdType,
        RandomAllocationPolicy<underlying_id_type_t<IdType>>,
        NoRecyclingPolicy<underlying_id_type_t<IdType>>,
        ExpectedErrorPolicy<IdType, IdError>,
        SingleThreadedPolicy>;

} // namespace fat_p

/**
 * @file scope_guard.hpp
 * @brief RAII scope guard for automatic cleanup actions
 *
 * Provides a mechanism to execute cleanup code when a scope exits,
 * regardless of whether the exit is normal or due to an exception.
 * Useful for C library interop and resource cleanup.
 */

#ifndef COMMON_SCOPE_GUARD_HPP
#define COMMON_SCOPE_GUARD_HPP

#include <utility>
#include <type_traits>

namespace speeduino {

/**
 * @class ScopeGuard
 * @brief Executes a callable when the guard goes out of scope
 *
 * @tparam Func The callable type to execute on scope exit
 *
 * Example usage:
 * @code
 *   void* ptr = malloc(100);
 *   auto guard = makeScopeGuard([ptr]() { free(ptr); });
 *   // ... code that might throw or return early ...
 *   // ptr is automatically freed when scope exits
 * @endcode
 */
template<typename Func>
class ScopeGuard {
public:
    /**
     * @brief Construct a scope guard
     * @param func The callable to execute on scope exit
     */
    explicit ScopeGuard(Func&& func) noexcept(std::is_nothrow_move_constructible_v<Func>)
        : func_(std::move(func))
        , active_(true) {}

    /**
     * @brief Construct a scope guard (copy)
     * @param func The callable to copy and execute on scope exit
     */
    explicit ScopeGuard(const Func& func) noexcept(std::is_nothrow_copy_constructible_v<Func>)
        : func_(func)
        , active_(true) {}

    /**
     * @brief Destructor - executes the cleanup function if still active
     */
    ~ScopeGuard() noexcept {
        if (active_) {
            try {
                func_();
            } catch (...) {
                // Destructors must not throw
            }
        }
    }

    // Move constructor
    ScopeGuard(ScopeGuard&& other) noexcept(std::is_nothrow_move_constructible_v<Func>)
        : func_(std::move(other.func_))
        , active_(std::exchange(other.active_, false)) {}

    // Non-copyable
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    ScopeGuard& operator=(ScopeGuard&&) = delete;

    /**
     * @brief Dismiss the guard - cleanup will not be executed
     *
     * Call this when you want to prevent the cleanup action,
     * typically when the operation succeeded and cleanup is not needed.
     */
    void dismiss() noexcept {
        active_ = false;
    }

    /**
     * @brief Check if the guard is still active
     * @return true if cleanup will be executed on destruction
     */
    [[nodiscard]] bool isActive() const noexcept {
        return active_;
    }

private:
    Func func_;
    bool active_;
};

/**
 * @brief Factory function to create a ScopeGuard with type deduction
 * @tparam Func The callable type (deduced)
 * @param func The cleanup function to execute
 * @return A ScopeGuard managing the cleanup function
 *
 * Example:
 * @code
 *   auto guard = makeScopeGuard([]() { cleanup(); });
 * @endcode
 */
template<typename Func>
[[nodiscard]] auto makeScopeGuard(Func&& func) {
    return ScopeGuard<std::decay_t<Func>>(std::forward<Func>(func));
}

/**
 * @class ScopeExit
 * @brief Simplified scope guard that always executes (no dismiss option)
 *
 * Lighter weight alternative when you always want cleanup to happen.
 */
template<typename Func>
class ScopeExit {
public:
    explicit ScopeExit(Func&& func) noexcept(std::is_nothrow_move_constructible_v<Func>)
        : func_(std::move(func)) {}

    explicit ScopeExit(const Func& func) noexcept(std::is_nothrow_copy_constructible_v<Func>)
        : func_(func) {}

    ~ScopeExit() noexcept {
        try {
            func_();
        } catch (...) {
            // Destructors must not throw
        }
    }

    // Non-copyable, non-movable
    ScopeExit(const ScopeExit&) = delete;
    ScopeExit& operator=(const ScopeExit&) = delete;
    ScopeExit(ScopeExit&&) = delete;
    ScopeExit& operator=(ScopeExit&&) = delete;

private:
    Func func_;
};

/**
 * @brief Factory function for ScopeExit
 */
template<typename Func>
[[nodiscard]] auto makeScopeExit(Func&& func) {
    return ScopeExit<std::decay_t<Func>>(std::forward<Func>(func));
}

/**
 * @class ScopeFail
 * @brief Scope guard that only executes on exception (scope exit via exception)
 *
 * Uses uncaught_exceptions() to detect if an exception is in flight.
 */
template<typename Func>
class ScopeFail {
public:
    explicit ScopeFail(Func&& func) noexcept(std::is_nothrow_move_constructible_v<Func>)
        : func_(std::move(func))
        , uncaughtOnCreation_(std::uncaught_exceptions()) {}

    ~ScopeFail() noexcept {
        if (std::uncaught_exceptions() > uncaughtOnCreation_) {
            try {
                func_();
            } catch (...) {
                // Destructors must not throw
            }
        }
    }

    // Non-copyable, non-movable
    ScopeFail(const ScopeFail&) = delete;
    ScopeFail& operator=(const ScopeFail&) = delete;
    ScopeFail(ScopeFail&&) = delete;
    ScopeFail& operator=(ScopeFail&&) = delete;

private:
    Func func_;
    int uncaughtOnCreation_;
};

/**
 * @brief Factory function for ScopeFail
 */
template<typename Func>
[[nodiscard]] auto makeScopeFail(Func&& func) {
    return ScopeFail<std::decay_t<Func>>(std::forward<Func>(func));
}

/**
 * @class ScopeSuccess
 * @brief Scope guard that only executes on normal exit (no exception)
 *
 * Opposite of ScopeFail - executes only when no exception is thrown.
 */
template<typename Func>
class ScopeSuccess {
public:
    explicit ScopeSuccess(Func&& func) noexcept(std::is_nothrow_move_constructible_v<Func>)
        : func_(std::move(func))
        , uncaughtOnCreation_(std::uncaught_exceptions()) {}

    ~ScopeSuccess() noexcept {
        if (std::uncaught_exceptions() <= uncaughtOnCreation_) {
            try {
                func_();
            } catch (...) {
                // Destructors must not throw
            }
        }
    }

    // Non-copyable, non-movable
    ScopeSuccess(const ScopeSuccess&) = delete;
    ScopeSuccess& operator=(const ScopeSuccess&) = delete;
    ScopeSuccess(ScopeSuccess&&) = delete;
    ScopeSuccess& operator=(ScopeSuccess&&) = delete;

private:
    Func func_;
    int uncaughtOnCreation_;
};

/**
 * @brief Factory function for ScopeSuccess
 */
template<typename Func>
[[nodiscard]] auto makeScopeSuccess(Func&& func) {
    return ScopeSuccess<std::decay_t<Func>>(std::forward<Func>(func));
}

} // namespace speeduino

// Convenience macros for anonymous scope guards
#define SPEEDUINO_CONCAT_IMPL(a, b) a##b
#define SPEEDUINO_CONCAT(a, b) SPEEDUINO_CONCAT_IMPL(a, b)
#define SPEEDUINO_UNIQUE_NAME(prefix) SPEEDUINO_CONCAT(prefix, __LINE__)

/**
 * @def SCOPE_EXIT
 * @brief Create an anonymous scope guard that always executes
 *
 * Usage:
 * @code
 *   SCOPE_EXIT { cleanup(); };
 * @endcode
 */
#define SCOPE_EXIT \
    auto SPEEDUINO_UNIQUE_NAME(scopeExit_) = ::speeduino::makeScopeExit([&]()

/**
 * @def SCOPE_FAIL
 * @brief Create an anonymous scope guard that executes on exception
 */
#define SCOPE_FAIL \
    auto SPEEDUINO_UNIQUE_NAME(scopeFail_) = ::speeduino::makeScopeFail([&]()

/**
 * @def SCOPE_SUCCESS
 * @brief Create an anonymous scope guard that executes on normal exit
 */
#define SCOPE_SUCCESS \
    auto SPEEDUINO_UNIQUE_NAME(scopeSuccess_) = ::speeduino::makeScopeSuccess([&]()

#endif // COMMON_SCOPE_GUARD_HPP

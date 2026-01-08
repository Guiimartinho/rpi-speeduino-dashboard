/**
 * @file expected.hpp
 * @brief Error handling without exceptions using Expected<T, E>
 *
 * Provides a type-safe way to return either a value or an error,
 * similar to Rust's Result type or C++23's std::expected.
 * Useful for embedded systems where exceptions may be disabled.
 */

#ifndef COMMON_EXPECTED_HPP
#define COMMON_EXPECTED_HPP

#include <variant>
#include <string>
#include <utility>
#include <type_traits>
#include <cassert>

namespace speeduino {

/**
 * @brief Tag type to indicate an unexpected (error) value
 */
template<typename E>
class Unexpected {
public:
    explicit Unexpected(const E& error) : error_(error) {}
    explicit Unexpected(E&& error) : error_(std::move(error)) {}

    [[nodiscard]] const E& value() const& noexcept { return error_; }
    [[nodiscard]] E& value() & noexcept { return error_; }
    [[nodiscard]] E&& value() && noexcept { return std::move(error_); }

private:
    E error_;
};

/**
 * @brief Factory function for Unexpected
 */
template<typename E>
[[nodiscard]] Unexpected<std::decay_t<E>> makeUnexpected(E&& error) {
    return Unexpected<std::decay_t<E>>(std::forward<E>(error));
}

/**
 * @class Expected
 * @brief A type that holds either a value of type T or an error of type E
 *
 * @tparam T The success value type
 * @tparam E The error type (default: std::string)
 *
 * Example usage:
 * @code
 *   Expected<int, std::string> divide(int a, int b) {
 *       if (b == 0) {
 *           return makeUnexpected(std::string("Division by zero"));
 *       }
 *       return a / b;
 *   }
 *
 *   auto result = divide(10, 2);
 *   if (result) {
 *       std::cout << "Result: " << result.value() << std::endl;
 *   } else {
 *       std::cout << "Error: " << result.error() << std::endl;
 *   }
 * @endcode
 */
template<typename T, typename E = std::string>
class Expected {
public:
    using value_type = T;
    using error_type = E;

    // Constructors for success value
    Expected(const T& value) : data_(value) {}
    Expected(T&& value) : data_(std::move(value)) {}

    // Constructor for error value
    Expected(const Unexpected<E>& error) : data_(error.value()) {}
    Expected(Unexpected<E>&& error) : data_(std::move(error).value()) {}

    // Check if contains a value
    [[nodiscard]] bool hasValue() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    // Boolean conversion
    explicit operator bool() const noexcept {
        return hasValue();
    }

    // Access the value (undefined if !hasValue())
    [[nodiscard]] T& value() & {
        assert(hasValue() && "Accessing value of Expected in error state");
        return std::get<T>(data_);
    }

    [[nodiscard]] const T& value() const& {
        assert(hasValue() && "Accessing value of Expected in error state");
        return std::get<T>(data_);
    }

    [[nodiscard]] T&& value() && {
        assert(hasValue() && "Accessing value of Expected in error state");
        return std::get<T>(std::move(data_));
    }

    // Access the error (undefined if hasValue())
    [[nodiscard]] E& error() & {
        assert(!hasValue() && "Accessing error of Expected in value state");
        return std::get<E>(data_);
    }

    [[nodiscard]] const E& error() const& {
        assert(!hasValue() && "Accessing error of Expected in value state");
        return std::get<E>(data_);
    }

    [[nodiscard]] E&& error() && {
        assert(!hasValue() && "Accessing error of Expected in value state");
        return std::get<E>(std::move(data_));
    }

    // Get value or default
    [[nodiscard]] T valueOr(T&& defaultValue) const& {
        return hasValue() ? value() : std::forward<T>(defaultValue);
    }

    [[nodiscard]] T valueOr(T&& defaultValue) && {
        return hasValue() ? std::move(value()) : std::forward<T>(defaultValue);
    }

    // Pointer-like access
    [[nodiscard]] T* operator->() {
        return &value();
    }

    [[nodiscard]] const T* operator->() const {
        return &value();
    }

    [[nodiscard]] T& operator*() & {
        return value();
    }

    [[nodiscard]] const T& operator*() const& {
        return value();
    }

    [[nodiscard]] T&& operator*() && {
        return std::move(value());
    }

    /**
     * @brief Transform the value using a function
     * @tparam F Function type (T -> U)
     * @param func The transformation function
     * @return Expected<U, E> with transformed value or original error
     */
    template<typename F>
    [[nodiscard]] auto map(F&& func) const& -> Expected<std::invoke_result_t<F, const T&>, E> {
        using U = std::invoke_result_t<F, const T&>;
        if (hasValue()) {
            return Expected<U, E>(func(value()));
        }
        return Expected<U, E>(makeUnexpected(error()));
    }

    template<typename F>
    [[nodiscard]] auto map(F&& func) && -> Expected<std::invoke_result_t<F, T&&>, E> {
        using U = std::invoke_result_t<F, T&&>;
        if (hasValue()) {
            return Expected<U, E>(func(std::move(value())));
        }
        return Expected<U, E>(makeUnexpected(std::move(error())));
    }

    /**
     * @brief Transform the error using a function
     * @tparam F Function type (E -> E2)
     * @param func The transformation function
     * @return Expected<T, E2> with original value or transformed error
     */
    template<typename F>
    [[nodiscard]] auto mapError(F&& func) const& -> Expected<T, std::invoke_result_t<F, const E&>> {
        using E2 = std::invoke_result_t<F, const E&>;
        if (hasValue()) {
            return Expected<T, E2>(value());
        }
        return Expected<T, E2>(makeUnexpected(func(error())));
    }

    /**
     * @brief Chain operations that return Expected
     * @tparam F Function type (T -> Expected<U, E>)
     * @param func The chaining function
     * @return The result of func if has value, otherwise propagate error
     */
    template<typename F>
    [[nodiscard]] auto andThen(F&& func) const& -> std::invoke_result_t<F, const T&> {
        if (hasValue()) {
            return func(value());
        }
        return makeUnexpected(error());
    }

    template<typename F>
    [[nodiscard]] auto andThen(F&& func) && -> std::invoke_result_t<F, T&&> {
        if (hasValue()) {
            return func(std::move(value()));
        }
        return makeUnexpected(std::move(error()));
    }

    /**
     * @brief Handle error case
     * @tparam F Function type (E -> Expected<T, E>)
     * @param func The error handling function
     * @return Original value or result of error handler
     */
    template<typename F>
    [[nodiscard]] auto orElse(F&& func) const& -> Expected<T, E> {
        if (hasValue()) {
            return *this;
        }
        return func(error());
    }

private:
    std::variant<T, E> data_;
};

/**
 * @brief Specialization for void value type
 *
 * Represents an operation that either succeeds (no value) or fails with error.
 */
template<typename E>
class Expected<void, E> {
public:
    using value_type = void;
    using error_type = E;

    // Success constructor
    Expected() : hasValue_(true) {}

    // Error constructor
    Expected(const Unexpected<E>& error)
        : error_(error.value())
        , hasValue_(false) {}

    Expected(Unexpected<E>&& error)
        : error_(std::move(error).value())
        , hasValue_(false) {}

    [[nodiscard]] bool hasValue() const noexcept {
        return hasValue_;
    }

    explicit operator bool() const noexcept {
        return hasValue_;
    }

    void value() const {
        assert(hasValue() && "Accessing value of Expected<void> in error state");
    }

    [[nodiscard]] E& error() & {
        assert(!hasValue() && "Accessing error of Expected<void> in value state");
        return error_;
    }

    [[nodiscard]] const E& error() const& {
        assert(!hasValue() && "Accessing error of Expected<void> in value state");
        return error_;
    }

private:
    E error_{};
    bool hasValue_;
};

/**
 * @brief Common error codes for embedded systems
 */
enum class ErrorCode : uint8_t {
    Success = 0,
    InvalidArgument,
    NotFound,
    PermissionDenied,
    Timeout,
    ResourceBusy,
    IoError,
    OutOfMemory,
    NotInitialized,
    AlreadyExists,
    NotSupported,
    InternalError
};

/**
 * @brief Convert ErrorCode to string
 */
inline const char* errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::Success:          return "Success";
        case ErrorCode::InvalidArgument:  return "Invalid argument";
        case ErrorCode::NotFound:         return "Not found";
        case ErrorCode::PermissionDenied: return "Permission denied";
        case ErrorCode::Timeout:          return "Timeout";
        case ErrorCode::ResourceBusy:     return "Resource busy";
        case ErrorCode::IoError:          return "I/O error";
        case ErrorCode::OutOfMemory:      return "Out of memory";
        case ErrorCode::NotInitialized:   return "Not initialized";
        case ErrorCode::AlreadyExists:    return "Already exists";
        case ErrorCode::NotSupported:     return "Not supported";
        case ErrorCode::InternalError:    return "Internal error";
        default:                          return "Unknown error";
    }
}

/**
 * @brief Rich error type with code and message
 */
struct Error {
    ErrorCode code = ErrorCode::InternalError;
    std::string message;

    Error() = default;
    Error(ErrorCode c, std::string msg = "")
        : code(c), message(std::move(msg)) {}

    explicit Error(const std::string& msg)
        : code(ErrorCode::InternalError), message(msg) {}

    [[nodiscard]] std::string toString() const {
        if (message.empty()) {
            return errorCodeToString(code);
        }
        return std::string(errorCodeToString(code)) + ": " + message;
    }
};

/**
 * @brief Convenience type alias for Expected with Error
 */
template<typename T>
using Result = Expected<T, Error>;

/**
 * @brief Convenience type alias for void Result
 */
using VoidResult = Expected<void, Error>;

} // namespace speeduino

#endif // COMMON_EXPECTED_HPP

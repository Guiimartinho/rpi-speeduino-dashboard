/**
 * @file test_expected.cpp
 * @brief Unit tests for Expected<T, E> error handling
 */

#include <gtest/gtest.h>
#include "common/expected.hpp"
#include <string>

using namespace speeduino;

TEST(ExpectedTest, ValueConstruction) {
    Expected<int, std::string> result(42);

    EXPECT_TRUE(result.hasValue());
    EXPECT_FALSE(result.hasError());
    EXPECT_EQ(result.value(), 42);
}

TEST(ExpectedTest, ErrorConstruction) {
    Expected<int, std::string> result(Unexpected<std::string>("error"));

    EXPECT_FALSE(result.hasValue());
    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error(), "error");
}

TEST(ExpectedTest, MakeExpected) {
    auto result = makeExpected<int, std::string>(42);

    EXPECT_TRUE(result.hasValue());
    EXPECT_EQ(result.value(), 42);
}

TEST(ExpectedTest, MakeUnexpected) {
    auto result = makeUnexpected<int, std::string>("failure");

    EXPECT_TRUE(result.hasError());
    EXPECT_EQ(result.error(), "failure");
}

TEST(ExpectedTest, ValueOr) {
    Expected<int, std::string> success(42);
    Expected<int, std::string> failure(Unexpected<std::string>("error"));

    EXPECT_EQ(success.valueOr(0), 42);
    EXPECT_EQ(failure.valueOr(0), 0);
}

TEST(ExpectedTest, AndThen) {
    auto doubler = [](int x) -> Expected<int, std::string> {
        return Expected<int, std::string>(x * 2);
    };

    Expected<int, std::string> success(21);
    Expected<int, std::string> failure(Unexpected<std::string>("error"));

    auto doubled = success.andThen(doubler);
    auto stillFailed = failure.andThen(doubler);

    EXPECT_TRUE(doubled.hasValue());
    EXPECT_EQ(doubled.value(), 42);

    EXPECT_TRUE(stillFailed.hasError());
    EXPECT_EQ(stillFailed.error(), "error");
}

TEST(ExpectedTest, Map) {
    Expected<int, std::string> success(21);
    Expected<int, std::string> failure(Unexpected<std::string>("error"));

    auto doubled = success.map([](int x) { return x * 2; });
    auto stillFailed = failure.map([](int x) { return x * 2; });

    EXPECT_TRUE(doubled.hasValue());
    EXPECT_EQ(doubled.value(), 42);

    EXPECT_TRUE(stillFailed.hasError());
}

TEST(ExpectedTest, OrElse) {
    Expected<int, std::string> success(42);
    Expected<int, std::string> failure(Unexpected<std::string>("error"));

    auto recovered = failure.orElse([](const std::string&) {
        return Expected<int, std::string>(0);
    });

    auto unchanged = success.orElse([](const std::string&) {
        return Expected<int, std::string>(0);
    });

    EXPECT_TRUE(recovered.hasValue());
    EXPECT_EQ(recovered.value(), 0);

    EXPECT_TRUE(unchanged.hasValue());
    EXPECT_EQ(unchanged.value(), 42);
}

TEST(ExpectedTest, BoolConversion) {
    Expected<int, std::string> success(42);
    Expected<int, std::string> failure(Unexpected<std::string>("error"));

    EXPECT_TRUE(static_cast<bool>(success));
    EXPECT_FALSE(static_cast<bool>(failure));
}

TEST(ExpectedTest, ResultAlias) {
    Result<int> success = makeExpected<int, Error>(42);
    Result<int> failure = makeUnexpected<int, Error>(Error{ErrorCode::IoError, "test"});

    EXPECT_TRUE(success.hasValue());
    EXPECT_TRUE(failure.hasError());
    EXPECT_EQ(failure.error().code, ErrorCode::IoError);
}

TEST(ExpectedTest, VoidResultAlias) {
    VoidResult success = makeExpected<void, Error>();
    VoidResult failure = makeUnexpected<void, Error>(Error{ErrorCode::InvalidArgument, "test"});

    EXPECT_TRUE(success.hasValue());
    EXPECT_TRUE(failure.hasError());
}

TEST(ExpectedTest, MoveSemantics) {
    Expected<std::unique_ptr<int>, std::string> result(
        std::make_unique<int>(42));

    EXPECT_TRUE(result.hasValue());

    auto ptr = std::move(result).value();
    EXPECT_EQ(*ptr, 42);
}

TEST(ExpectedTest, ErrorCodes) {
    EXPECT_STREQ(errorCodeToString(ErrorCode::Success), "Success");
    EXPECT_STREQ(errorCodeToString(ErrorCode::InvalidArgument), "InvalidArgument");
    EXPECT_STREQ(errorCodeToString(ErrorCode::IoError), "IoError");
    EXPECT_STREQ(errorCodeToString(ErrorCode::Timeout), "Timeout");
}

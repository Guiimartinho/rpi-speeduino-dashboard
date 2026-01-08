/**
 * @file test_can_error_handler.cpp
 * @brief Unit tests for CAN error handling
 */

#include <gtest/gtest.h>
#include "can_service/can_error_handler.hpp"
#include <thread>
#include <chrono>

using namespace speeduino;

TEST(CanBusStateTest, StateToString) {
    EXPECT_STREQ(canBusStateToString(CanBusState::ErrorActive), "ErrorActive");
    EXPECT_STREQ(canBusStateToString(CanBusState::ErrorWarning), "ErrorWarning");
    EXPECT_STREQ(canBusStateToString(CanBusState::ErrorPassive), "ErrorPassive");
    EXPECT_STREQ(canBusStateToString(CanBusState::BusOff), "BusOff");
}

TEST(CanErrorTypeTest, TypeToString) {
    EXPECT_STREQ(canErrorTypeToString(CanErrorType::None), "None");
    EXPECT_STREQ(canErrorTypeToString(CanErrorType::TxTimeout), "TxTimeout");
    EXPECT_STREQ(canErrorTypeToString(CanErrorType::BusOff), "BusOff");
    EXPECT_STREQ(canErrorTypeToString(CanErrorType::NoAck), "NoAck");
}

TEST(CanErrorHandlerTest, InitialState) {
    CanErrorHandler handler;

    EXPECT_EQ(handler.currentState(), CanBusState::ErrorActive);
    EXPECT_FALSE(handler.hasError());
    EXPECT_TRUE(handler.isOperational());
}

TEST(CanErrorHandlerTest, GetCounters) {
    CanErrorHandler handler;

    auto counters = handler.getCounters();
    EXPECT_EQ(counters.txErrorCount, 0);
    EXPECT_EQ(counters.rxErrorCount, 0);
}

TEST(CanErrorHandlerTest, GetStats) {
    CanErrorHandler handler;

    auto stats = handler.getStats();
    EXPECT_EQ(stats.totalErrors, 0);
    EXPECT_EQ(stats.busOffEvents, 0);
    EXPECT_EQ(stats.txTimeouts, 0);
}

TEST(CanErrorHandlerTest, ResetStats) {
    CanErrorHandler handler;

    // Process would normally modify stats, but we can test reset
    handler.resetStats();

    auto stats = handler.getStats();
    EXPECT_EQ(stats.totalErrors, 0);
}

TEST(CanErrorHandlerTest, TimeSinceLastError) {
    CanErrorHandler handler;

    // No errors yet, should return max
    auto timeSince = handler.timeSinceLastError();
    EXPECT_EQ(timeSince, std::chrono::milliseconds::max());
}

TEST(CanErrorHandlerTest, GetStatusString) {
    CanErrorHandler handler;

    auto status = handler.getStatusString();
    EXPECT_FALSE(status.empty());
    EXPECT_NE(status.find("ErrorActive"), std::string::npos);
}

TEST(CanErrorHandlerTest, SetBusStateCallback) {
    CanErrorHandler handler;
    bool callbackCalled = false;

    handler.setBusStateCallback([&callbackCalled](CanBusState, CanBusState) {
        callbackCalled = true;
    });

    // Callback would be called when state changes via processErrorFrame
    EXPECT_FALSE(callbackCalled);  // Not called yet
}

TEST(CanErrorHandlerTest, SetErrorEventCallback) {
    CanErrorHandler handler;
    bool callbackCalled = false;

    handler.setErrorEventCallback([&callbackCalled](const CanErrorEvent&) {
        callbackCalled = true;
    });

    // Callback would be called on error events
    EXPECT_FALSE(callbackCalled);  // Not called yet
}

TEST(CanBusMonitorTest, InitialMetrics) {
    CanBusMonitor monitor;
    CanErrorHandler handler;

    auto metrics = monitor.getMetrics(handler);

    EXPECT_EQ(metrics.state, CanBusState::ErrorActive);
    EXPECT_EQ(metrics.totalFrames, 0);
    EXPECT_EQ(metrics.totalErrors, 0);
    EXPECT_TRUE(metrics.isHealthy);
}

TEST(CanBusMonitorTest, RecordFrame) {
    CanBusMonitor monitor;
    CanErrorHandler handler;

    monitor.recordFrame(false);  // Normal frame
    monitor.recordFrame(false);
    monitor.recordFrame(true);   // Error frame

    auto metrics = monitor.getMetrics(handler);

    EXPECT_EQ(metrics.totalFrames, 3);
    EXPECT_EQ(metrics.totalErrors, 1);
}

TEST(CanBusMonitorTest, Reset) {
    CanBusMonitor monitor;
    CanErrorHandler handler;

    monitor.recordFrame(false);
    monitor.recordFrame(true);

    monitor.reset();

    auto metrics = monitor.getMetrics(handler);

    EXPECT_EQ(metrics.totalFrames, 0);
    EXPECT_EQ(metrics.totalErrors, 0);
}

TEST(CanBusMonitorTest, FrameRate) {
    CanBusMonitor monitor;
    CanErrorHandler handler;

    // Record some frames
    for (int i = 0; i < 100; i++) {
        monitor.recordFrame(false);
    }

    // Wait a bit for non-zero elapsed time
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto metrics = monitor.getMetrics(handler);

    EXPECT_EQ(metrics.totalFrames, 100);
    EXPECT_GT(metrics.frameRate, 0.0f);
}

TEST(CanErrorCountersTest, DefaultValues) {
    CanErrorCounters counters;

    EXPECT_EQ(counters.txErrorCount, 0);
    EXPECT_EQ(counters.rxErrorCount, 0);
}

TEST(CanErrorEventTest, DefaultValues) {
    CanErrorEvent event;

    EXPECT_EQ(event.type, CanErrorType::None);
    EXPECT_EQ(event.state, CanBusState::ErrorActive);
    EXPECT_EQ(event.arbitrationLostBit, 0);
    EXPECT_TRUE(event.description.empty());
}

#ifdef __linux__
// Linux-specific tests would go here for actual error frame processing
// These require a real or virtual CAN interface

TEST(CanErrorHandlerTest, EnableErrorFrames_InvalidFd) {
    CanErrorHandler handler;

    // Invalid socket should fail gracefully
    bool result = handler.enableErrorFrames(-1);
    EXPECT_FALSE(result);
}
#endif

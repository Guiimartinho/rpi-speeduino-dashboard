/**
 * @file test_can_error_handler.cpp
 * @brief Unit tests for CAN error handling
 */

#include <gtest/gtest.h>
#include "can_service/can_error_handler.hpp"
#include <thread>
#include <chrono>
#include <atomic>
#include <vector>
#include <mutex>

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

#ifdef __linux__
// ═══════════════════════════════════════════════════════════════════════════════
// Callback Execution Tests - Verify callbacks are actually invoked
// ISO 26262 ASIL-B: Critical callback verification
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CanErrorHandlerCallbackTest, ErrorEventCallbackInvoked) {
    CanErrorHandler handler;
    std::atomic<int> callbackCount{0};
    CanErrorEvent lastEvent;
    std::mutex eventMutex;

    handler.setErrorEventCallback([&](const CanErrorEvent& event) {
        callbackCount++;
        std::lock_guard<std::mutex> lock(eventMutex);
        lastEvent = event;
    });

    // Simulate TX timeout error frame
    // CAN_ERR_TX_TIMEOUT = 0x00000001
    uint32_t canId = 0x20000001;  // CAN_ERR_FLAG | CAN_ERR_TX_TIMEOUT
    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    handler.processErrorFrame(canId, data);

    EXPECT_GE(callbackCount.load(), 1);

    std::lock_guard<std::mutex> lock(eventMutex);
    EXPECT_EQ(lastEvent.type, CanErrorType::TxTimeout);
}

TEST(CanErrorHandlerCallbackTest, BusStateCallbackInvoked) {
    CanErrorHandler handler;
    std::atomic<int> callbackCount{0};
    std::atomic<CanBusState> newState{CanBusState::ErrorActive};

    handler.setBusStateCallback([&](CanBusState old, CanBusState newS) {
        callbackCount++;
        newState = newS;
        (void)old;  // Suppress unused warning
    });

    // Simulate bus-off error frame
    // CAN_ERR_BUSOFF = 0x00000040
    uint32_t canId = 0x20000040;  // CAN_ERR_FLAG | CAN_ERR_BUSOFF
    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    handler.processErrorFrame(canId, data);

    EXPECT_GE(callbackCount.load(), 1);
    EXPECT_EQ(newState.load(), CanBusState::BusOff);
}

TEST(CanErrorHandlerCallbackTest, ControllerErrorWarningState) {
    CanErrorHandler handler;
    std::atomic<int> stateCallbackCount{0};

    handler.setBusStateCallback([&](CanBusState, CanBusState) {
        stateCallbackCount++;
    });

    // Simulate controller RX warning error
    // CAN_ERR_CRTL = 0x00000004
    uint32_t canId = 0x20000004;  // CAN_ERR_FLAG | CAN_ERR_CRTL
    // CAN_ERR_CRTL_RX_WARNING = 0x04 in data[1]
    uint8_t data[8] = {0, 0x04, 0, 0, 0, 0, 0, 0};

    handler.processErrorFrame(canId, data);

    EXPECT_GE(stateCallbackCount.load(), 1);
    EXPECT_EQ(handler.currentState(), CanBusState::ErrorWarning);
}

TEST(CanErrorHandlerCallbackTest, ControllerErrorPassiveState) {
    CanErrorHandler handler;

    // Simulate controller TX passive error
    // CAN_ERR_CRTL = 0x00000004
    uint32_t canId = 0x20000004;  // CAN_ERR_FLAG | CAN_ERR_CRTL
    // CAN_ERR_CRTL_TX_PASSIVE = 0x20 in data[1]
    uint8_t data[8] = {0, 0x20, 0, 0, 0, 0, 0, 0};

    handler.processErrorFrame(canId, data);

    EXPECT_EQ(handler.currentState(), CanBusState::ErrorPassive);
}

TEST(CanErrorHandlerCallbackTest, ControllerRestartedRecovery) {
    CanErrorHandler handler;

    // First, set to bus-off
    uint32_t busOffId = 0x20000040;  // CAN_ERR_FLAG | CAN_ERR_BUSOFF
    uint8_t busOffData[8] = {0};
    handler.processErrorFrame(busOffId, busOffData);

    EXPECT_EQ(handler.currentState(), CanBusState::BusOff);

    // Now simulate controller restarted
    // CAN_ERR_RESTARTED = 0x00000100
    uint32_t restartId = 0x20000100;  // CAN_ERR_FLAG | CAN_ERR_RESTARTED
    uint8_t restartData[8] = {0};

    handler.processErrorFrame(restartId, restartData);

    // Should be back to ErrorActive after restart
    EXPECT_EQ(handler.currentState(), CanBusState::ErrorActive);
}

TEST(CanErrorHandlerCallbackTest, ArbitrationLostCallback) {
    CanErrorHandler handler;
    std::atomic<int> callbackCount{0};
    std::atomic<uint32_t> lastArbitrationBit{0};

    handler.setErrorEventCallback([&](const CanErrorEvent& event) {
        callbackCount++;
        if (event.type == CanErrorType::ArbitrationLost) {
            lastArbitrationBit = event.arbitrationLostBit;
        }
    });

    // CAN_ERR_LOSTARB = 0x00000002
    uint32_t canId = 0x20000002;  // CAN_ERR_FLAG | CAN_ERR_LOSTARB
    // Arbitration lost at bit 5 (stored in data[0])
    uint8_t data[8] = {5, 0, 0, 0, 0, 0, 0, 0};

    handler.processErrorFrame(canId, data);

    EXPECT_GE(callbackCount.load(), 1);
    EXPECT_EQ(lastArbitrationBit.load(), 5U);
}

TEST(CanErrorHandlerCallbackTest, ProtocolErrorCallback) {
    CanErrorHandler handler;
    std::atomic<int> callbackCount{0};

    handler.setErrorEventCallback([&](const CanErrorEvent& event) {
        callbackCount++;
        if (event.type == CanErrorType::ProtocolViolation) {
            // Verify description is populated
            EXPECT_FALSE(event.description.empty());
        }
    });

    // CAN_ERR_PROT = 0x00000008
    uint32_t canId = 0x20000008;  // CAN_ERR_FLAG | CAN_ERR_PROT
    // Protocol error type in data[2], location in data[3]
    uint8_t data[8] = {0, 0, 0x02, 0x03, 0, 0, 0, 0};  // CAN_ERR_PROT_FORM at CRC_SEQ

    handler.processErrorFrame(canId, data);

    EXPECT_GE(callbackCount.load(), 1);
}

TEST(CanErrorHandlerCallbackTest, NoAckErrorCallback) {
    CanErrorHandler handler;
    std::atomic<int> callbackCount{0};
    std::atomic<CanErrorType> lastType{CanErrorType::None};

    handler.setErrorEventCallback([&](const CanErrorEvent& event) {
        callbackCount++;
        lastType = event.type;
    });

    // CAN_ERR_ACK = 0x00000020
    uint32_t canId = 0x20000020;  // CAN_ERR_FLAG | CAN_ERR_ACK
    uint8_t data[8] = {0};

    handler.processErrorFrame(canId, data);

    EXPECT_GE(callbackCount.load(), 1);
    EXPECT_EQ(lastType.load(), CanErrorType::NoAck);
}

TEST(CanErrorHandlerCallbackTest, ErrorCountersUpdated) {
    CanErrorHandler handler;

    // Simulate error frame with counters in data[6] and data[7]
    uint32_t canId = 0x20000001;  // CAN_ERR_FLAG | CAN_ERR_TX_TIMEOUT
    uint8_t data[8] = {0, 0, 0, 0, 0, 0, 50, 30};  // TEC=50, REC=30

    handler.processErrorFrame(canId, data);

    auto counters = handler.getCounters();
    EXPECT_EQ(counters.txErrorCount, 50);
    EXPECT_EQ(counters.rxErrorCount, 30);
}

TEST(CanErrorHandlerCallbackTest, StatsIncrementedOnError) {
    CanErrorHandler handler;

    auto statsBefore = handler.getStats();

    uint32_t canId = 0x20000001;  // CAN_ERR_FLAG | CAN_ERR_TX_TIMEOUT
    uint8_t data[8] = {0};

    handler.processErrorFrame(canId, data);
    handler.processErrorFrame(canId, data);
    handler.processErrorFrame(canId, data);

    auto statsAfter = handler.getStats();

    EXPECT_EQ(statsAfter.totalErrors, statsBefore.totalErrors + 3);
    EXPECT_EQ(statsAfter.txTimeouts, statsBefore.txTimeouts + 3);
}

TEST(CanErrorHandlerCallbackTest, TimeSinceLastErrorUpdated) {
    CanErrorHandler handler;

    // Initially, no errors
    EXPECT_EQ(handler.timeSinceLastError(), std::chrono::milliseconds::max());

    uint32_t canId = 0x20000001;  // CAN_ERR_FLAG | CAN_ERR_TX_TIMEOUT
    uint8_t data[8] = {0};

    handler.processErrorFrame(canId, data);

    // Now should have a recent timestamp
    auto timeSince = handler.timeSinceLastError();
    EXPECT_LT(timeSince, std::chrono::milliseconds(1000));
}

TEST(CanErrorHandlerCallbackTest, ConcurrentCallbackExecution) {
    CanErrorHandler handler;
    std::atomic<int> callbackCount{0};

    handler.setErrorEventCallback([&](const CanErrorEvent&) {
        callbackCount++;
        // Simulate some work in callback
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    });

    // Multiple threads processing error frames
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&]() {
            uint32_t canId = 0x20000001;  // CAN_ERR_FLAG | CAN_ERR_TX_TIMEOUT
            uint8_t data[8] = {0};
            for (int i = 0; i < 10; ++i) {
                handler.processErrorFrame(canId, data);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(callbackCount.load(), 40);
}

TEST(CanErrorHandlerCallbackTest, CallbackExceptionHandled) {
    CanErrorHandler handler;
    std::atomic<int> callbackCount{0};

    handler.setBusStateCallback([&](CanBusState, CanBusState) {
        callbackCount++;
        throw std::runtime_error("Test exception from callback");
    });

    // Should not crash even if callback throws
    uint32_t canId = 0x20000040;  // CAN_ERR_FLAG | CAN_ERR_BUSOFF
    uint8_t data[8] = {0};

    // This should handle the exception gracefully
    EXPECT_NO_THROW(handler.processErrorFrame(canId, data));
    EXPECT_GE(callbackCount.load(), 1);
}

#endif // __linux__

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

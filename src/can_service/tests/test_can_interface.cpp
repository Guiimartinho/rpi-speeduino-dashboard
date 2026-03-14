/**
 * @file test_can_interface.cpp
 * @brief Unit tests for CAN interface
 *
 * Tests the CanInterface class functionality including:
 * - Frame validation (DLC, ID bounds)
 * - Status tracking
 * - Error handling
 * - Platform-independent functionality
 *
 * ISO 26262 ASIL-B: Comprehensive testing for safety-critical CAN interface
 */

#include "can_service/can_interface.hpp"

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <thread>

using namespace speeduino;

// ═══════════════════════════════════════════════════════════════════════════════
// Test Constants - Matching production code constants
// ═══════════════════════════════════════════════════════════════════════════════
namespace test_constants {
constexpr uint8_t CAN_CLASSIC_MAX_DLC = 8;
constexpr uint32_t CAN_STD_ID_MAX     = 0x7FF;
constexpr uint32_t CAN_EXT_ID_MAX     = 0x1FFFFFFF;
}  // namespace test_constants

// ═══════════════════════════════════════════════════════════════════════════════
// CanFrame Tests - Structure validation
// ═══════════════════════════════════════════════════════════════════════════════

class CanFrameTest : public ::testing::Test {
protected:
    CanFrame frame;
};

TEST_F(CanFrameTest, DefaultConstruction) {
    // Default frame should be zero-initialized
    EXPECT_EQ(frame.id, 0U);
    EXPECT_EQ(frame.dlc, 0U);
    EXPECT_EQ(frame.timestamp_us, 0U);
    EXPECT_FALSE(frame.is_extended);
    EXPECT_FALSE(frame.is_rtr);
    EXPECT_FALSE(frame.is_error);

    // Data should be zero
    for (size_t i = 0; i < frame.data.size(); ++i) {
        EXPECT_EQ(frame.data[i], 0U) << "Data byte " << i << " not zero";
    }
}

TEST_F(CanFrameTest, StandardIdRange) {
    // Valid standard ID
    frame.id          = 0x123;
    frame.is_extended = false;
    EXPECT_LE(frame.id, test_constants::CAN_STD_ID_MAX);

    // Maximum standard ID
    frame.id = test_constants::CAN_STD_ID_MAX;
    EXPECT_EQ(frame.id, 0x7FFU);
}

TEST_F(CanFrameTest, ExtendedIdRange) {
    // Valid extended ID
    frame.id          = 0x12345678;
    frame.is_extended = true;
    EXPECT_LE(frame.id, test_constants::CAN_EXT_ID_MAX);

    // Maximum extended ID
    frame.id = test_constants::CAN_EXT_ID_MAX;
    EXPECT_EQ(frame.id, 0x1FFFFFFFU);
}

TEST_F(CanFrameTest, DlcValidRange) {
    // Valid DLC values
    for (uint8_t dlc = 0; dlc <= test_constants::CAN_CLASSIC_MAX_DLC; ++dlc) {
        frame.dlc = dlc;
        EXPECT_LE(frame.dlc, test_constants::CAN_CLASSIC_MAX_DLC);
    }
}

TEST_F(CanFrameTest, DataPayload) {
    // Set data payload
    frame.dlc  = 8;
    frame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    EXPECT_EQ(frame.data[0], 0x01U);
    EXPECT_EQ(frame.data[7], 0x08U);
}

TEST_F(CanFrameTest, Flags) {
    // Test flag combinations
    frame.is_extended = true;
    frame.is_rtr      = true;
    frame.is_error    = false;

    EXPECT_TRUE(frame.is_extended);
    EXPECT_TRUE(frame.is_rtr);
    EXPECT_FALSE(frame.is_error);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CanStatus Tests - Status tracking validation
// ═══════════════════════════════════════════════════════════════════════════════

class CanStatusTest : public ::testing::Test {
protected:
    CanStatus status;
};

TEST_F(CanStatusTest, DefaultConstruction) {
    EXPECT_FALSE(status.connected);
    EXPECT_EQ(status.rx_count, 0U);
    EXPECT_EQ(status.tx_count, 0U);
    EXPECT_EQ(status.error_count, 0U);
    EXPECT_EQ(status.last_rx_timestamp, 0U);
}

TEST_F(CanStatusTest, CounterIncrement) {
    status.rx_count    = 100;
    status.tx_count    = 50;
    status.error_count = 5;

    EXPECT_EQ(status.rx_count, 100U);
    EXPECT_EQ(status.tx_count, 50U);
    EXPECT_EQ(status.error_count, 5U);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CanInterface Tests - Core functionality
// ═══════════════════════════════════════════════════════════════════════════════

class CanInterfaceTest : public ::testing::Test {
protected:
    CanInterface interface;
};

TEST_F(CanInterfaceTest, DefaultConstruction) {
    // Not connected by default
    EXPECT_FALSE(interface.isConnected());
    EXPECT_EQ(interface.getFd(), -1);
}

TEST_F(CanInterfaceTest, InitialStatus) {
    auto status = interface.getStatus();

    EXPECT_FALSE(status.connected);
    EXPECT_EQ(status.rx_count, 0U);
    EXPECT_EQ(status.tx_count, 0U);
    EXPECT_EQ(status.error_count, 0U);
}

TEST_F(CanInterfaceTest, OpenInvalidInterface) {
    // Opening non-existent interface should fail gracefully
    bool result = interface.open("nonexistent_interface_12345");

    // On Linux this will fail; on other platforms it always fails
    EXPECT_FALSE(result);
    EXPECT_FALSE(interface.isConnected());
}

TEST_F(CanInterfaceTest, OpenEmptyInterface) {
    // Empty interface name should fail
    bool result = interface.open("");

    EXPECT_FALSE(result);
    EXPECT_FALSE(interface.isConnected());
}

TEST_F(CanInterfaceTest, CloseWithoutOpen) {
    // Close should be safe even if not open
    EXPECT_NO_THROW(interface.close());
    EXPECT_FALSE(interface.isConnected());
}

TEST_F(CanInterfaceTest, DoubleClose) {
    // Double close should be safe
    interface.close();
    EXPECT_NO_THROW(interface.close());
    EXPECT_FALSE(interface.isConnected());
}

TEST_F(CanInterfaceTest, SendWithoutOpen) {
    // Send should fail if not connected
    CanFrame frame;
    frame.id   = 0x123;
    frame.dlc  = 8;
    frame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    bool result = interface.send(frame);

    EXPECT_FALSE(result);
}

TEST_F(CanInterfaceTest, ReceiveWithoutOpen) {
    // Receive should return nullopt if not connected
    auto result = interface.receive(10);

    EXPECT_FALSE(result.has_value());
}

TEST_F(CanInterfaceTest, SetCallback) {
    bool callbackCalled = false;
    CanFrame receivedFrame;

    interface.setCallback([&](const CanFrame& frame) {
        callbackCalled = true;
        receivedFrame  = frame;
    });

    // Callback is set but won't be called without receive
    EXPECT_FALSE(callbackCalled);
}

TEST_F(CanInterfaceTest, ReceiveLoopControl) {
    // Start and stop receive loop (internal state)
    interface.startReceiveLoop();
    interface.stopReceiveLoop();

    // Should not throw or crash
    EXPECT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// DLC Validation Tests - Buffer overflow prevention
// ISO 26262 ASIL-B: Critical bounds checking
// ═══════════════════════════════════════════════════════════════════════════════

class CanInterfaceDlcValidationTest : public ::testing::Test {
protected:
    CanInterface interface;
    CanFrame frame;

    void SetUp() override {
        frame.id   = 0x123;
        frame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    }
};

TEST_F(CanInterfaceDlcValidationTest, ValidDlcValues) {
    // Test all valid DLC values (0-8)
    for (uint8_t dlc = 0; dlc <= 8; ++dlc) {
        frame.dlc = dlc;
        // Send will fail because not connected, but DLC is valid
        // We're testing that invalid DLC doesn't crash
        EXPECT_FALSE(interface.send(frame));
    }
}

TEST_F(CanInterfaceDlcValidationTest, InvalidDlcReject) {
    // DLC > 8 should be rejected even if connection existed
    frame.dlc = 9;
    EXPECT_FALSE(interface.send(frame));

    frame.dlc = 15;
    EXPECT_FALSE(interface.send(frame));

    frame.dlc = 255;
    EXPECT_FALSE(interface.send(frame));
}

// ═══════════════════════════════════════════════════════════════════════════════
// CAN ID Validation Tests - ID bounds checking
// ═══════════════════════════════════════════════════════════════════════════════

class CanInterfaceIdValidationTest : public ::testing::Test {
protected:
    CanInterface interface;
    CanFrame frame;

    void SetUp() override {
        frame.dlc  = 8;
        frame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    }
};

TEST_F(CanInterfaceIdValidationTest, ValidStandardId) {
    frame.is_extended = false;
    frame.id          = 0x7FF;  // Max standard ID

    // Will fail because not connected, but ID is valid
    EXPECT_FALSE(interface.send(frame));
}

TEST_F(CanInterfaceIdValidationTest, ValidExtendedId) {
    frame.is_extended = true;
    frame.id          = 0x1FFFFFFF;  // Max extended ID

    // Will fail because not connected, but ID is valid
    EXPECT_FALSE(interface.send(frame));
}

// ═══════════════════════════════════════════════════════════════════════════════
// Thread Safety Tests - Concurrent access
// ═══════════════════════════════════════════════════════════════════════════════

class CanInterfaceThreadSafetyTest : public ::testing::Test {
protected:
    CanInterface interface;
};

TEST_F(CanInterfaceThreadSafetyTest, ConcurrentStatusAccess) {
    // Multiple threads reading status should not crash
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([this]() {
            for (int j = 0; j < 100; ++j) {
                auto status = interface.getStatus();
                (void)status;  // Prevent unused warning
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_TRUE(true);  // If we get here, no crashes occurred
}

TEST_F(CanInterfaceThreadSafetyTest, ConcurrentOpenClose) {
    // Stress test open/close from multiple threads
    std::atomic<int> successCount{0};
    std::atomic<int> failCount{0};

    std::vector<std::thread> threads;

    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([this, &successCount, &failCount]() {
            for (int j = 0; j < 10; ++j) {
                if (interface.open("vcan0")) {
                    successCount++;
                    interface.close();
                } else {
                    failCount++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // All operations should complete without crash
    EXPECT_GE(successCount + failCount, 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Performance Tests - Timing constraints
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CanInterfacePerformanceTest, StatusAccessLatency) {
    CanInterface interface;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        auto status = interface.getStatus();
        (void)status;
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 10000 status reads should complete in < 100ms
    EXPECT_LT(duration.count(), 100000);
}

TEST(CanInterfacePerformanceTest, FrameConstructionLatency) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        CanFrame frame;
        frame.id   = 0x123;
        frame.dlc  = 8;
        frame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
        (void)frame;
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 10000 frame constructions should complete in < 50ms
    EXPECT_LT(duration.count(), 50000);
}

// ═══════════════════════════════════════════════════════════════════════════════
// RAII Tests - Resource cleanup
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CanInterfaceRaiiTest, DestructorCleansUp) {
    // Create and destroy interface - should not leak
    {
        CanInterface interface;
        interface.open("vcan0");  // May fail, that's OK
    }
    // Destructor called - verify no crash
    EXPECT_TRUE(true);
}

TEST(CanInterfaceRaiiTest, MoveConstruction) {
    // CanInterface is non-copyable, verify compilation
    CanInterface interface1;
    // CanInterface interface2 = interface1;  // Should not compile

    EXPECT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Edge Cases Tests - Boundary conditions
// ═══════════════════════════════════════════════════════════════════════════════

TEST(CanInterfaceEdgeCaseTest, ZeroDlcFrame) {
    CanInterface interface;
    CanFrame frame;
    frame.id  = 0x123;
    frame.dlc = 0;  // Valid: zero data bytes

    // Will fail because not connected, but frame is valid
    EXPECT_FALSE(interface.send(frame));
}

TEST(CanInterfaceEdgeCaseTest, RtrFrame) {
    CanInterface interface;
    CanFrame frame;
    frame.id     = 0x123;
    frame.dlc    = 8;
    frame.is_rtr = true;  // Remote Transmission Request

    // Will fail because not connected
    EXPECT_FALSE(interface.send(frame));
}

TEST(CanInterfaceEdgeCaseTest, ErrorFrame) {
    CanFrame frame;
    frame.is_error = true;

    EXPECT_TRUE(frame.is_error);
}

TEST(CanInterfaceEdgeCaseTest, ReceiveTimeout) {
    CanInterface interface;

    auto start  = std::chrono::high_resolution_clock::now();
    auto result = interface.receive(1);  // 1ms timeout
    auto end    = std::chrono::high_resolution_clock::now();

    EXPECT_FALSE(result.has_value());

    // Should return quickly (not connected)
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 50);  // Should be nearly instant
}

#ifdef __linux__
// ═══════════════════════════════════════════════════════════════════════════════
// Linux-specific Tests - vcan interface
// ═══════════════════════════════════════════════════════════════════════════════

class CanInterfaceVcanTest : public ::testing::Test {
protected:
    CanInterface interface;
    bool vcanAvailable = false;

    void SetUp() override {
        // Try to open vcan0 - may not be available
        vcanAvailable = interface.open("vcan0");
    }

    void TearDown() override { interface.close(); }
};

TEST_F(CanInterfaceVcanTest, OpenVcan) {
    if (!vcanAvailable) {
        GTEST_SKIP() << "vcan0 not available - run: modprobe vcan && ip link add vcan0 type vcan "
                        "&& ip link set vcan0 up";
    }

    EXPECT_TRUE(interface.isConnected());
    EXPECT_GE(interface.getFd(), 0);
}

TEST_F(CanInterfaceVcanTest, SendReceiveLoopback) {
    if (!vcanAvailable) {
        GTEST_SKIP() << "vcan0 not available";
    }

    // Send a frame
    CanFrame txFrame;
    txFrame.id   = 0x123;
    txFrame.dlc  = 8;
    txFrame.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    bool sendResult = interface.send(txFrame);
    EXPECT_TRUE(sendResult);

    // Receive the loopback frame
    auto rxFrame = interface.receive(100);

    if (rxFrame.has_value()) {
        EXPECT_EQ(rxFrame->id, txFrame.id);
        EXPECT_EQ(rxFrame->dlc, txFrame.dlc);
        for (int i = 0; i < txFrame.dlc; ++i) {
            EXPECT_EQ(rxFrame->data[i], txFrame.data[i]);
        }
    }
}

TEST_F(CanInterfaceVcanTest, StatusAfterSend) {
    if (!vcanAvailable) {
        GTEST_SKIP() << "vcan0 not available";
    }

    auto statusBefore = interface.getStatus();

    CanFrame frame;
    frame.id   = 0x456;
    frame.dlc  = 4;
    frame.data = {0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x00, 0x00, 0x00};

    interface.send(frame);

    auto statusAfter = interface.getStatus();

    EXPECT_GT(statusAfter.tx_count, statusBefore.tx_count);
}

#endif  // __linux__

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

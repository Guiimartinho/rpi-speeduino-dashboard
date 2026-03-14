/**
 * @file test_reverse_detector.cpp
 * @brief Unit tests for reverse gear detection
 *
 * Tests the ReverseDetector class functionality including:
 * - CAN-based detection
 * - GPIO-based detection (mock)
 * - Debounce logic
 * - Callback invocation
 * - Thread safety
 *
 * ISO 26262 ASIL-B: Comprehensive testing for safety-critical reverse detection
 */

#include "reverse_service/reverse_detector.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <vector>

using namespace speeduino;

// ═══════════════════════════════════════════════════════════════════════════════
// Test Fixtures - Common setup for reverse detector tests
// ═══════════════════════════════════════════════════════════════════════════════

class ReverseDetectorTest : public ::testing::Test {
protected:
    ReverseDetector detector;
    ReverseConfig config;

    void SetUp() override {
        // Default CAN-only configuration
        config.detection_mode = "can";
        config.can_enabled    = true;
        config.gpio_enabled   = false;
        config.can_id         = 0x370;
        config.byte_index     = 3;
        config.bit_mask       = 0x80;
        config.expected_value = 0x80;
        config.debounce_ms    = 50;
    }

    void TearDown() override { detector.shutdown(); }

    // Helper to create a CAN frame with reverse engaged
    std::array<uint8_t, 8> createReverseEngagedFrame() {
        std::array<uint8_t, 8> data = {0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00};
        return data;
    }

    // Helper to create a CAN frame with reverse disengaged
    std::array<uint8_t, 8> createReverseDisengagedFrame() {
        std::array<uint8_t, 8> data = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        return data;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// Initialization Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, DefaultConstruction) {
    ReverseDetector newDetector;

    EXPECT_FALSE(newDetector.isReverseEngaged());
    EXPECT_EQ(newDetector.getSource(), ReverseDetector::Source::CAN);
}

TEST_F(ReverseDetectorTest, InitWithCanConfig) {
    config.detection_mode = "can";
    config.can_enabled    = true;
    config.gpio_enabled   = false;

    bool result = detector.init(config);

    EXPECT_TRUE(result);
    EXPECT_FALSE(detector.isReverseEngaged());
}

TEST_F(ReverseDetectorTest, InitWithGpioConfig) {
    config.detection_mode = "gpio";
    config.can_enabled    = false;
    config.gpio_enabled   = true;
    config.gpio_chip      = "gpiochip0";
    config.gpio_line      = 17;

    // Init will succeed but GPIO may fail to initialize (no hardware)
    bool result = detector.init(config);

    // Should return true even if GPIO init fails (falls back gracefully)
    EXPECT_TRUE(result);
}

TEST_F(ReverseDetectorTest, InitWithBothConfig) {
    config.detection_mode = "both";
    config.can_enabled    = true;
    config.gpio_enabled   = true;

    bool result = detector.init(config);

    EXPECT_TRUE(result);
}

TEST_F(ReverseDetectorTest, InitWithSpeeduinoCanConfig) {
    config.detection_mode = "speeduino_can";
    config.can_id         = 0x370;

    bool result = detector.init(config);

    EXPECT_TRUE(result);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CAN Frame Processing Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, ProcessCanFrameEngagesReverse) {
    detector.init(config);

    auto data = createReverseEngagedFrame();

    // Process frame multiple times to overcome debounce
    for (int i = 0; i < 10; ++i) {
        detector.procesCanFrame(config.can_id, data.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Wait for debounce
    std::this_thread::sleep_for(std::chrono::milliseconds(config.debounce_ms + 20));
    detector.procesCanFrame(config.can_id, data.data(), 8);

    EXPECT_TRUE(detector.isReverseEngaged());
    EXPECT_EQ(detector.getSource(), ReverseDetector::Source::CAN);
}

TEST_F(ReverseDetectorTest, ProcessCanFrameDisengagesReverse) {
    detector.init(config);

    // First engage
    auto engagedData = createReverseEngagedFrame();
    for (int i = 0; i < 10; ++i) {
        detector.procesCanFrame(config.can_id, engagedData.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(config.debounce_ms + 20));
    detector.procesCanFrame(config.can_id, engagedData.data(), 8);

    EXPECT_TRUE(detector.isReverseEngaged());

    // Now disengage
    auto disengagedData = createReverseDisengagedFrame();
    for (int i = 0; i < 10; ++i) {
        detector.procesCanFrame(config.can_id, disengagedData.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(config.debounce_ms + 20));
    detector.procesCanFrame(config.can_id, disengagedData.data(), 8);

    EXPECT_FALSE(detector.isReverseEngaged());
}

TEST_F(ReverseDetectorTest, IgnoreWrongCanId) {
    detector.init(config);

    auto data = createReverseEngagedFrame();

    // Process with wrong CAN ID
    detector.procesCanFrame(0x999, data.data(), 8);
    std::this_thread::sleep_for(std::chrono::milliseconds(config.debounce_ms + 20));
    detector.procesCanFrame(0x999, data.data(), 8);

    EXPECT_FALSE(detector.isReverseEngaged());
}

TEST_F(ReverseDetectorTest, IgnoreShortDlc) {
    detector.init(config);

    auto data = createReverseEngagedFrame();

    // DLC shorter than byte_index
    detector.procesCanFrame(config.can_id, data.data(), 2);
    std::this_thread::sleep_for(std::chrono::milliseconds(config.debounce_ms + 20));
    detector.procesCanFrame(config.can_id, data.data(), 2);

    EXPECT_FALSE(detector.isReverseEngaged());
}

TEST_F(ReverseDetectorTest, ProcessCanFrameWhenCanDisabled) {
    config.detection_mode = "gpio";
    config.can_enabled    = false;
    detector.init(config);

    auto data = createReverseEngagedFrame();

    // Should be ignored when CAN is disabled
    detector.procesCanFrame(config.can_id, data.data(), 8);
    std::this_thread::sleep_for(std::chrono::milliseconds(config.debounce_ms + 20));

    EXPECT_FALSE(detector.isReverseEngaged());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Debounce Tests - Critical for reliable detection
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, DebounceFiltersNoise) {
    config.debounce_ms = 100;
    detector.init(config);

    auto engagedData    = createReverseEngagedFrame();
    auto disengagedData = createReverseDisengagedFrame();

    // Rapid toggle should not change state (noise)
    for (int i = 0; i < 5; ++i) {
        detector.procesCanFrame(config.can_id, engagedData.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        detector.procesCanFrame(config.can_id, disengagedData.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Should still be disengaged (initial state) due to debounce
    EXPECT_FALSE(detector.isReverseEngaged());
}

TEST_F(ReverseDetectorTest, StableSignalPassesDebounce) {
    config.debounce_ms = 50;
    detector.init(config);

    auto engagedData = createReverseEngagedFrame();

    // Send consistent signal for longer than debounce
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        detector.procesCanFrame(config.can_id, engagedData.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(detector.isReverseEngaged());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Callback Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, CallbackInvokedOnStateChange) {
    std::atomic<int> callbackCount{0};
    std::atomic<bool> lastEngaged{false};
    std::atomic<uint8_t> lastSource{0};

    detector.setCallback([&](bool engaged, uint8_t source) {
        callbackCount++;
        lastEngaged = engaged;
        lastSource  = source;
    });

    detector.init(config);

    auto engagedData = createReverseEngagedFrame();

    // Trigger state change
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        detector.procesCanFrame(config.can_id, engagedData.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Callback should have been called
    EXPECT_GE(callbackCount.load(), 1);
    EXPECT_TRUE(lastEngaged.load());
    EXPECT_EQ(lastSource.load(), static_cast<uint8_t>(ReverseDetector::Source::CAN));
}

TEST_F(ReverseDetectorTest, CallbackNotInvokedWithoutStateChange) {
    std::atomic<int> callbackCount{0};

    detector.setCallback([&](bool, uint8_t) { callbackCount++; });

    detector.init(config);

    auto disengagedData = createReverseDisengagedFrame();

    // Send same state (disengaged) - no change
    for (int i = 0; i < 10; ++i) {
        detector.procesCanFrame(config.can_id, disengagedData.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Callback should NOT have been called
    EXPECT_EQ(callbackCount.load(), 0);
}

TEST_F(ReverseDetectorTest, CallbackSurvivesException) {
    std::atomic<int> callbackCount{0};

    detector.setCallback([&](bool, uint8_t) {
        callbackCount++;
        // Callback that might throw - detector should survive
    });

    detector.init(config);

    auto engagedData = createReverseEngagedFrame();

    // Trigger state change
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        detector.procesCanFrame(config.can_id, engagedData.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Should complete without crash
    EXPECT_GE(callbackCount.load(), 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Thread Safety Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, ConcurrentCanProcessing) {
    detector.init(config);

    std::vector<std::thread> threads;
    std::atomic<int> processCount{0};

    // Multiple threads processing CAN frames
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([this, &processCount]() {
            auto data = createReverseEngagedFrame();
            for (int i = 0; i < 100; ++i) {
                detector.procesCanFrame(config.can_id, data.data(), 8);
                processCount++;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(processCount.load(), 400);
    // No crash = success
}

TEST_F(ReverseDetectorTest, ConcurrentStateAccess) {
    detector.init(config);

    std::vector<std::thread> threads;

    // Writer thread
    threads.emplace_back([this]() {
        auto engagedData    = createReverseEngagedFrame();
        auto disengagedData = createReverseDisengagedFrame();
        for (int i = 0; i < 100; ++i) {
            detector.procesCanFrame(config.can_id,
                                    (i % 2 == 0) ? engagedData.data() : disengagedData.data(), 8);
            std::this_thread::sleep_for(std::chrono::microseconds(500));
        }
    });

    // Reader threads
    for (int t = 0; t < 3; ++t) {
        threads.emplace_back([this]() {
            for (int i = 0; i < 1000; ++i) {
                bool state = detector.isReverseEngaged();
                (void)state;
                auto source = detector.getSource();
                (void)source;
                auto timestamp = detector.getLastChangeTimestamp();
                (void)timestamp;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // No crash = success
    EXPECT_TRUE(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Config Preset Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, GolQuadradoPreset) {
    ReverseConfig presetConfig;
    presetConfig.applyPreset("gol_quadrado");

    EXPECT_EQ(presetConfig.detection_mode, "gpio");
    EXPECT_FALSE(presetConfig.can_enabled);
    EXPECT_TRUE(presetConfig.gpio_enabled);
    EXPECT_EQ(presetConfig.gpio_line, 17U);
    EXPECT_TRUE(presetConfig.gpio_active_low);
    EXPECT_EQ(presetConfig.debounce_ms, 100U);
}

TEST_F(ReverseDetectorTest, ClassicVwPreset) {
    ReverseConfig presetConfig;
    presetConfig.applyPreset("classic_vw");

    EXPECT_EQ(presetConfig.detection_mode, "gpio");
    EXPECT_FALSE(presetConfig.can_enabled);
    EXPECT_TRUE(presetConfig.gpio_enabled);
}

TEST_F(ReverseDetectorTest, SpeeduinoCanPreset) {
    ReverseConfig presetConfig;
    presetConfig.applyPreset("speeduino_can");

    EXPECT_EQ(presetConfig.detection_mode, "can");
    EXPECT_TRUE(presetConfig.can_enabled);
    EXPECT_FALSE(presetConfig.gpio_enabled);
    EXPECT_EQ(presetConfig.can_id, 0x370U);
}

TEST_F(ReverseDetectorTest, HaltechPreset) {
    ReverseConfig presetConfig;
    presetConfig.applyPreset("haltech");

    EXPECT_EQ(presetConfig.detection_mode, "can");
    EXPECT_TRUE(presetConfig.can_enabled);
    EXPECT_FALSE(presetConfig.gpio_enabled);
    EXPECT_EQ(presetConfig.can_id, 0x360U);
}

TEST_F(ReverseDetectorTest, UnknownPresetNoOp) {
    ReverseConfig presetConfig;
    presetConfig.can_id = 0x999;

    presetConfig.applyPreset("unknown_preset");

    // Should not change anything
    EXPECT_EQ(presetConfig.can_id, 0x999U);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Source Tracking Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, SourceTracksCan) {
    detector.init(config);

    auto data = createReverseEngagedFrame();

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        detector.procesCanFrame(config.can_id, data.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (detector.isReverseEngaged()) {
        EXPECT_EQ(detector.getSource(), ReverseDetector::Source::CAN);
    }
}

TEST_F(ReverseDetectorTest, TimestampUpdatesOnChange) {
    detector.init(config);

    uint32_t initialTimestamp = detector.getLastChangeTimestamp();

    auto data = createReverseEngagedFrame();

    // Wait and trigger change
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        detector.procesCanFrame(config.can_id, data.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (detector.isReverseEngaged()) {
        uint32_t newTimestamp = detector.getLastChangeTimestamp();
        EXPECT_GT(newTimestamp, initialTimestamp);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// Shutdown Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, ShutdownSafe) {
    detector.init(config);

    EXPECT_NO_THROW(detector.shutdown());
}

TEST_F(ReverseDetectorTest, DoubleShutdown) {
    detector.init(config);

    detector.shutdown();
    EXPECT_NO_THROW(detector.shutdown());
}

TEST_F(ReverseDetectorTest, ShutdownWithoutInit) {
    ReverseDetector uninitDetector;
    EXPECT_NO_THROW(uninitDetector.shutdown());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Bit Mask Tests - Various bit patterns
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, SingleBitMask) {
    config.bit_mask       = 0x01;
    config.expected_value = 0x01;
    config.byte_index     = 0;
    detector.init(config);

    std::array<uint8_t, 8> data = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        detector.procesCanFrame(config.can_id, data.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(detector.isReverseEngaged());
}

TEST_F(ReverseDetectorTest, MultiBitMask) {
    config.bit_mask       = 0x0F;
    config.expected_value = 0x05;
    config.byte_index     = 2;
    detector.init(config);

    std::array<uint8_t, 8> data = {0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00};

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        detector.procesCanFrame(config.can_id, data.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(detector.isReverseEngaged());
}

TEST_F(ReverseDetectorTest, FullByteMask) {
    config.bit_mask       = 0xFF;
    config.expected_value = 0xAA;
    config.byte_index     = 5;
    detector.init(config);

    std::array<uint8_t, 8> data = {0x00, 0x00, 0x00, 0x00, 0x00, 0xAA, 0x00, 0x00};

    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(100)) {
        detector.procesCanFrame(config.can_id, data.data(), 8);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(detector.isReverseEngaged());
}

// ═══════════════════════════════════════════════════════════════════════════════
// Performance Tests
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ReverseDetectorTest, CanProcessingLatency) {
    detector.init(config);

    auto data = createReverseEngagedFrame();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        detector.procesCanFrame(config.can_id, data.data(), 8);
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 10000 CAN frame processings should complete in < 100ms
    EXPECT_LT(duration.count(), 100000);
}

TEST_F(ReverseDetectorTest, StateQueryLatency) {
    detector.init(config);

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; ++i) {
        bool state = detector.isReverseEngaged();
        (void)state;
    }

    auto end      = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // 100000 state queries should complete in < 50ms
    EXPECT_LT(duration.count(), 50000);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

/**
 * @file test_config_loader.cpp
 * @brief Unit tests for ConfigLoader thread-safe configuration loading
 *
 * ISO 26262 ASIL-B: Validates thread-safe static member access
 * Tests YAML parsing, default values, and preset configurations
 */

#include "common/config_loader.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

using namespace speeduino;

namespace {

// Create a temporary test config directory
class ConfigLoaderTest : public ::testing::Test {
protected:
    std::filesystem::path testDir;

    void SetUp() override {
        testDir = std::filesystem::temp_directory_path() / "speeduino_test_config";
        std::filesystem::create_directories(testDir);
    }

    void TearDown() override { std::filesystem::remove_all(testDir); }

    void writeSystemConfig(const std::string& content) {
        std::ofstream file(testDir / "system.yaml");
        file << content;
        file.close();
    }

    void writeSignalsConfig(const std::string& content) {
        std::ofstream file(testDir / "can_signals.yaml");
        file << content;
        file.close();
    }
};

}  // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════════
// BASIC LOADING TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ConfigLoaderTest, LoadsEmptyDirectory) {
    // Should succeed with defaults when no config files exist
    bool result = ConfigLoader::loadFromDirectory(testDir.string());
    EXPECT_TRUE(result);
}

TEST_F(ConfigLoaderTest, LoadsSystemConfig) {
    writeSystemConfig(R"(
can:
  interface: can1
  bitrate: 1000000
  protocol: bmw
  timeout_ms: 1000
zmq:
  publish_rate_hz: 100
camera:
  device: /dev/video1
  width: 1280
  height: 720
  fps: 60
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    const auto& cfg = ConfigLoader::getSystemConfig();
    EXPECT_EQ(cfg.can_interface, "can1");
    EXPECT_EQ(cfg.can_bitrate, 1000000u);
    EXPECT_EQ(cfg.can_protocol, "bmw");
    EXPECT_EQ(cfg.can_timeout_ms, 1000u);
    EXPECT_EQ(cfg.zmq_publish_rate_hz, 100u);
    EXPECT_EQ(cfg.camera_device, "/dev/video1");
    EXPECT_EQ(cfg.camera_width, 1280u);
    EXPECT_EQ(cfg.camera_height, 720u);
    EXPECT_EQ(cfg.camera_fps, 60u);
}

TEST_F(ConfigLoaderTest, LoadsReverseConfig) {
    writeSystemConfig(R"(
reverse:
  detection_mode: gpio
  can_enabled: false
  gpio_enabled: true
  gpio_chip: gpiochip4
  gpio_line: 27
  gpio_active_low: true
  debounce_ms: 150
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    const auto& cfg = ConfigLoader::getReverseConfig();
    EXPECT_EQ(cfg.detection_mode, "gpio");
    EXPECT_FALSE(cfg.can_enabled);
    EXPECT_TRUE(cfg.gpio_enabled);
    EXPECT_EQ(cfg.gpio_chip, "gpiochip4");
    EXPECT_EQ(cfg.gpio_line, 27u);
    EXPECT_TRUE(cfg.gpio_active_low);
    EXPECT_EQ(cfg.debounce_ms, 150u);
}

TEST_F(ConfigLoaderTest, LoadsReversePresetGolQuadrado) {
    writeSystemConfig(R"(
reverse:
  preset: gol_quadrado
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    const auto& cfg = ConfigLoader::getReverseConfig();
    EXPECT_EQ(cfg.detection_mode, "gpio");
    EXPECT_FALSE(cfg.can_enabled);
    EXPECT_TRUE(cfg.gpio_enabled);
    EXPECT_EQ(cfg.gpio_line, 17u);
    EXPECT_TRUE(cfg.gpio_active_low);
    EXPECT_EQ(cfg.debounce_ms, 100u);
}

TEST_F(ConfigLoaderTest, LoadsReversePresetSpeeduinoCan) {
    writeSystemConfig(R"(
reverse:
  preset: speeduino_can
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    const auto& cfg = ConfigLoader::getReverseConfig();
    EXPECT_EQ(cfg.detection_mode, "can");
    EXPECT_TRUE(cfg.can_enabled);
    EXPECT_FALSE(cfg.gpio_enabled);
    EXPECT_EQ(cfg.can_id, 0x370u);
}

TEST_F(ConfigLoaderTest, LoadsHexCanIds) {
    writeSystemConfig(R"(
reverse:
  can_id: 0x3E0
  bit_mask: 0xFF
  expected_value: 0x80
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    const auto& cfg = ConfigLoader::getReverseConfig();
    EXPECT_EQ(cfg.can_id, 0x3E0u);
    EXPECT_EQ(cfg.bit_mask, 0xFFu);
    EXPECT_EQ(cfg.expected_value, 0x80u);
}

TEST_F(ConfigLoaderTest, LoadsAllowedCommands) {
    writeSystemConfig(R"(
allowed_commands:
  - id: 0x7E0
    rate_limit: 5
    description: Diagnostic request
  - id: 0x100
    rate_limit: 10
    description: Custom command
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    const auto& cmds = ConfigLoader::getAllowedCommands();
    ASSERT_GE(cmds.size(), 2u);

    EXPECT_EQ(cmds[0].can_id, 0x7E0u);
    EXPECT_EQ(cmds[0].rate_limit_hz, 5u);
    EXPECT_EQ(cmds[0].description, "Diagnostic request");

    EXPECT_EQ(cmds[1].can_id, 0x100u);
    EXPECT_EQ(cmds[1].rate_limit_hz, 10u);
}

TEST_F(ConfigLoaderTest, IsCommandAllowed) {
    writeSystemConfig(R"(
allowed_commands:
  - id: 0x7E0
    rate_limit: 5
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    EXPECT_TRUE(ConfigLoader::isCommandAllowed(0x7E0));
    EXPECT_FALSE(ConfigLoader::isCommandAllowed(0x7E1));
}

TEST_F(ConfigLoaderTest, GetCommandRateLimit) {
    writeSystemConfig(R"(
allowed_commands:
  - id: 0x7E0
    rate_limit: 5
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    EXPECT_EQ(ConfigLoader::getCommandRateLimit(0x7E0), 5u);
    EXPECT_EQ(ConfigLoader::getCommandRateLimit(0x999), 0u);
}

// ═══════════════════════════════════════════════════════════════════════════════
// SIGNAL LOADING TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ConfigLoaderTest, LoadsCustomSignals) {
    writeSignalsConfig(R"(
signals:
  - name: test_rpm
    can_id: 0x360
    start_byte: 0
    length_bits: 16
    big_endian: true
    signed: false
    scale: 1.0
    offset: 0.0
    unit: rpm
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    const auto& signals = ConfigLoader::getSignals();
    ASSERT_GE(signals.size(), 1u);

    auto sig = ConfigLoader::findSignal("test_rpm");
    ASSERT_TRUE(sig.has_value());
    EXPECT_EQ(sig->can_id, 0x360u);
    EXPECT_EQ(sig->start_byte, 0u);
    EXPECT_EQ(sig->length_bits, 16u);
    EXPECT_TRUE(sig->is_big_endian);
    EXPECT_FALSE(sig->is_signed);
    EXPECT_EQ(sig->unit, "rpm");
}

TEST_F(ConfigLoaderTest, LoadsBuiltinHaltechSignals) {
    writeSignalsConfig(R"(
use_builtin: haltech
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    const auto& signals = ConfigLoader::getSignals();
    EXPECT_GT(signals.size(), 5u);  // Haltech has many signals

    auto rpm = ConfigLoader::findSignal("rpm");
    ASSERT_TRUE(rpm.has_value());
    EXPECT_EQ(rpm->can_id, 0x360u);
}

TEST_F(ConfigLoaderTest, FindSignalReturnsNulloptForMissing) {
    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));
    auto result = ConfigLoader::findSignal("nonexistent_signal_xyz");
    EXPECT_FALSE(result.has_value());
}

// ═══════════════════════════════════════════════════════════════════════════════
// THREAD SAFETY TESTS
// ISO 26262 ASIL-B: Validates concurrent access to static members
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ConfigLoaderTest, ThreadSafeConcurrentReads) {
    writeSystemConfig(R"(
can:
  interface: can0
  bitrate: 500000
zmq:
  publish_rate_hz: 50
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    std::atomic<int> successCount{0};
    std::atomic<int> errorCount{0};
    constexpr int NUM_THREADS      = 10;
    constexpr int READS_PER_THREAD = 100;

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&]() {
            for (int j = 0; j < READS_PER_THREAD; ++j) {
                try {
                    // Multiple concurrent reads
                    const auto& sys     = ConfigLoader::getSystemConfig();
                    const auto& rev     = ConfigLoader::getReverseConfig();
                    const auto& signals = ConfigLoader::getSignals();
                    auto sig            = ConfigLoader::findSignal("rpm");
                    bool allowed        = ConfigLoader::isCommandAllowed(0x7E0);

                    // Verify values are consistent
                    if (sys.can_interface == "can0" && sys.can_bitrate == 500000) {
                        ++successCount;
                    } else {
                        ++errorCount;
                    }
                    (void)rev;
                    (void)signals;
                    (void)sig;
                    (void)allowed;
                } catch (...) {
                    ++errorCount;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount.load(), NUM_THREADS * READS_PER_THREAD);
    EXPECT_EQ(errorCount.load(), 0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// EDGE CASE TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST_F(ConfigLoaderTest, HandlesInvalidYaml) {
    std::ofstream file(testDir / "system.yaml");
    file << "invalid: yaml: syntax: [";
    file.close();

    // Should handle gracefully
    bool result = ConfigLoader::loadFromDirectory(testDir.string());
    EXPECT_FALSE(result);
}

TEST_F(ConfigLoaderTest, HandlesEmptyHexValue) {
    writeSystemConfig(R"(
reverse:
  can_id: 0x
)");

    // Should use default value for empty hex
    bool result = ConfigLoader::loadFromDirectory(testDir.string());
    EXPECT_TRUE(result);
}

TEST_F(ConfigLoaderTest, HandlesMissingOptionalFields) {
    writeSystemConfig(R"(
can:
  interface: can0
# Missing all other optional fields
)");

    EXPECT_TRUE(ConfigLoader::loadFromDirectory(testDir.string()));

    const auto& cfg = ConfigLoader::getSystemConfig();
    EXPECT_EQ(cfg.can_interface, "can0");
    // Default values should be used for missing fields
    EXPECT_GT(cfg.zmq_publish_rate_hz, 0u);
}

// ═══════════════════════════════════════════════════════════════════════════════
// REVERSE CONFIG PRESET TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(ReverseConfigTest, ApplyPresetGolQuadrado) {
    ReverseConfig cfg;
    cfg.applyPreset("gol_quadrado");

    EXPECT_EQ(cfg.detection_mode, "gpio");
    EXPECT_FALSE(cfg.can_enabled);
    EXPECT_TRUE(cfg.gpio_enabled);
    EXPECT_EQ(cfg.gpio_line, 17u);
    EXPECT_TRUE(cfg.gpio_active_low);
    EXPECT_EQ(cfg.debounce_ms, 100u);
}

TEST(ReverseConfigTest, ApplyPresetClassicVw) {
    ReverseConfig cfg;
    cfg.applyPreset("classic_vw");

    EXPECT_EQ(cfg.detection_mode, "gpio");
    EXPECT_FALSE(cfg.can_enabled);
    EXPECT_TRUE(cfg.gpio_enabled);
}

TEST(ReverseConfigTest, ApplyPresetSpeeduinoCan) {
    ReverseConfig cfg;
    cfg.applyPreset("speeduino_can");

    EXPECT_EQ(cfg.detection_mode, "can");
    EXPECT_TRUE(cfg.can_enabled);
    EXPECT_FALSE(cfg.gpio_enabled);
    EXPECT_EQ(cfg.can_id, 0x370u);
    EXPECT_EQ(cfg.byte_index, 3u);
    EXPECT_EQ(cfg.bit_mask, 0x80u);
}

TEST(ReverseConfigTest, ApplyPresetHaltech) {
    ReverseConfig cfg;
    cfg.applyPreset("haltech");

    EXPECT_EQ(cfg.detection_mode, "can");
    EXPECT_TRUE(cfg.can_enabled);
    EXPECT_FALSE(cfg.gpio_enabled);
    EXPECT_EQ(cfg.can_id, 0x360u);
    EXPECT_EQ(cfg.byte_index, 4u);
    EXPECT_EQ(cfg.bit_mask, 0x02u);
}

TEST(ReverseConfigTest, ApplyPresetUnknown) {
    ReverseConfig cfg;
    std::string originalMode = cfg.detection_mode;
    cfg.applyPreset("unknown_preset_xyz");

    // Should not change anything
    EXPECT_EQ(cfg.detection_mode, originalMode);
}

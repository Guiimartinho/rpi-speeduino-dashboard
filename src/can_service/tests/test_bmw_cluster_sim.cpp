#include <gtest/gtest.h>
#include "can_service/bmw_cluster_sim.hpp"

using namespace speeduino;

// ═══════════════════════════════════════════════════════════════════════════════
// Mock CAN Interface for BMW Cluster Sim Testing
// ═══════════════════════════════════════════════════════════════════════════════

class MockCanInterface : public CanInterface {
public:
    MockCanInterface() : CanInterface() {}

    bool open(const std::string& interface_name) override {
        (void)interface_name;
        return true;
    }
    void close() override {}
    bool isConnected() const override { return true; }
    std::optional<CanFrame> receive(int timeout_ms) override {
        (void)timeout_ms;
        return std::nullopt;
    }
    bool send(const CanFrame& frame) override {
        m_sentFrames.push_back(frame);
        return true;
    }

    const std::vector<CanFrame>& getSentFrames() const { return m_sentFrames; }
    void clearSentFrames() { m_sentFrames.clear(); }
    std::vector<CanFrame> getFramesById(uint32_t id) const {
        std::vector<CanFrame> result;
        for (const auto& frame : m_sentFrames) {
            if (frame.id == id) {
                result.push_back(frame);
            }
        }
        return result;
    }

private:
    std::vector<CanFrame> m_sentFrames;
};

// ═══════════════════════════════════════════════════════════════════════════════
// BMW Cluster Simulator Tests
// ═══════════════════════════════════════════════════════════════════════════════

class BMWClusterSimTest : public ::testing::Test {
protected:
    void SetUp() override {
        interface = std::make_unique<MockCanInterface>();
        sim = std::make_unique<BMWClusterSim>(*interface);
    }

    std::unique_ptr<MockCanInterface> interface;
    std::unique_ptr<BMWClusterSim> sim;
};

TEST_F(BMWClusterSimTest, DefaultState) {
    EXPECT_TRUE(sim->isASCEnabled());
    EXPECT_TRUE(sim->isEGSEnabled());
    EXPECT_FALSE(sim->isSASEnabled());
    EXPECT_FALSE(sim->isABSEnabled());
    EXPECT_FALSE(sim->isRunning());
}

TEST_F(BMWClusterSimTest, EnableDisableModules) {
    sim->enableASC(false);
    sim->enableEGS(false);
    sim->enableSAS(true);
    sim->enableABS(true);

    EXPECT_FALSE(sim->isASCEnabled());
    EXPECT_FALSE(sim->isEGSEnabled());
    EXPECT_TRUE(sim->isSASEnabled());
    EXPECT_TRUE(sim->isABSEnabled());
}

TEST_F(BMWClusterSimTest, ASC1_FrameFormat) {
    sim->enableASC(true);
    sim->enableEGS(false);
    sim->enableSAS(false);
    sim->enableABS(false);

    sim->setDSCOff(false);
    sim->setDSCActive(false);
    sim->setTractionActive(false);
    sim->setABSActive(false);

    // ASC sends when tickCounter % 5 == 0, need 5 ticks
    for (int i = 0; i < 5; i++) sim->tick();

    auto frames = interface->getFramesById(bmw::ASC1_ID);
    ASSERT_EQ(frames.size(), 1);

    const auto& frame = frames[0];
    EXPECT_EQ(frame.id, 0x153);
    EXPECT_EQ(frame.dlc, 8);
    EXPECT_EQ(frame.data[0], 0x00);  // No flags set
}

TEST_F(BMWClusterSimTest, ASC1_DSCOff) {
    sim->enableASC(true);
    sim->enableEGS(false);
    sim->enableSAS(false);
    sim->enableABS(false);

    sim->setDSCOff(true);
    // ASC sends when tickCounter % 5 == 0, need 5 ticks
    for (int i = 0; i < 5; i++) sim->tick();

    auto frames = interface->getFramesById(bmw::ASC1_ID);
    ASSERT_EQ(frames.size(), 1);

    EXPECT_EQ(frames[0].data[0], 0x01);  // DSC off flag
}

TEST_F(BMWClusterSimTest, ASC1_AllFlags) {
    sim->enableASC(true);
    sim->enableEGS(false);
    sim->enableSAS(false);
    sim->enableABS(false);

    sim->setDSCOff(true);
    sim->setDSCActive(true);
    sim->setTractionActive(true);
    sim->setABSActive(true);

    // ASC sends when tickCounter % 5 == 0, need 5 ticks
    for (int i = 0; i < 5; i++) sim->tick();

    auto frames = interface->getFramesById(bmw::ASC1_ID);
    ASSERT_EQ(frames.size(), 1);

    EXPECT_EQ(frames[0].data[0], 0x0F);  // All flags set
}

TEST_F(BMWClusterSimTest, EGS_FrameFormat) {
    sim->enableASC(false);
    sim->enableEGS(true);
    sim->enableSAS(false);
    sim->enableABS(false);

    sim->setGear(BMWGear::NEUTRAL);
    sim->setSportMode(false);

    sim->tick();

    auto frames = interface->getFramesById(bmw::EGS_ID);
    ASSERT_EQ(frames.size(), 1);

    const auto& frame = frames[0];
    EXPECT_EQ(frame.id, 0x43F);
    EXPECT_EQ(frame.dlc, 8);
    EXPECT_EQ(frame.data[0], static_cast<uint8_t>(BMWGear::NEUTRAL));  // 'N' = 0x4E
    EXPECT_EQ(frame.data[1], 0x00);  // Sport mode off
}

TEST_F(BMWClusterSimTest, EGS_Gears) {
    sim->enableASC(false);
    sim->enableEGS(true);
    sim->enableSAS(false);
    sim->enableABS(false);

    // Test each gear
    struct GearTest {
        uint8_t input;
        BMWGear expected;
    };

    std::vector<GearTest> tests = {
        {0, BMWGear::NEUTRAL},
        {1, BMWGear::GEAR_1},
        {2, BMWGear::GEAR_2},
        {3, BMWGear::GEAR_3},
        {4, BMWGear::GEAR_4},
        {5, BMWGear::GEAR_5},
        {6, BMWGear::GEAR_6},
        {7, BMWGear::REVERSE},
    };

    for (const auto& test : tests) {
        interface->clearSentFrames();
        sim->setGear(test.input);
        // EGS sends when tickCounter % 5 == 1, tick 5 times to guarantee a send
        for (int i = 0; i < 5; i++) sim->tick();

        auto frames = interface->getFramesById(bmw::EGS_ID);
        ASSERT_GE(frames.size(), 1);  // At least one frame sent in 5 ticks
        EXPECT_EQ(frames.back().data[0], static_cast<uint8_t>(test.expected))
            << "Failed for gear input " << static_cast<int>(test.input);
    }
}

TEST_F(BMWClusterSimTest, EGS_SportMode) {
    sim->enableASC(false);
    sim->enableEGS(true);
    sim->enableSAS(false);
    sim->enableABS(false);

    sim->setSportMode(true);
    sim->tick();

    auto frames = interface->getFramesById(bmw::EGS_ID);
    ASSERT_EQ(frames.size(), 1);

    EXPECT_EQ(frames[0].data[1], 0x01);  // Sport mode on
}

TEST_F(BMWClusterSimTest, SAS_FrameFormat) {
    sim->enableASC(false);
    sim->enableEGS(false);
    sim->enableSAS(true);
    sim->enableABS(false);

    sim->setSteeringAngle(0);
    sim->setSteeringRate(0);

    sim->tick();

    auto frames = interface->getFramesById(bmw::SAS_ID);
    ASSERT_EQ(frames.size(), 1);

    const auto& frame = frames[0];
    EXPECT_EQ(frame.id, 0x1D2);
    EXPECT_EQ(frame.dlc, 8);

    // Angle = 0 × 10 = 0
    int16_t angle = frame.data[0] | (frame.data[1] << 8);
    EXPECT_EQ(angle, 0);

    // Status byte
    EXPECT_EQ(frame.data[4], 0x03);  // Calibrated and valid
}

TEST_F(BMWClusterSimTest, SAS_SteeringAngle) {
    sim->enableASC(false);
    sim->enableEGS(false);
    sim->enableSAS(true);
    sim->enableABS(false);

    // Test positive angle (right)
    sim->setSteeringAngle(45);  // 45 degrees right
    sim->tick();

    auto frames = interface->getFramesById(bmw::SAS_ID);
    ASSERT_EQ(frames.size(), 1);

    // Angle × 10 = 450 = 0x01C2 (Little Endian)
    int16_t angle = frames[0].data[0] | (frames[0].data[1] << 8);
    EXPECT_EQ(angle, 450);
}

TEST_F(BMWClusterSimTest, SAS_SteeringAngle_Negative) {
    sim->enableASC(false);
    sim->enableEGS(false);
    sim->enableSAS(true);
    sim->enableABS(false);

    // Test negative angle (left)
    sim->setSteeringAngle(-90);  // 90 degrees left
    sim->tick();

    auto frames = interface->getFramesById(bmw::SAS_ID);
    ASSERT_EQ(frames.size(), 1);

    // Angle × 10 = -900 (signed Little Endian)
    int16_t angle = static_cast<int16_t>(frames[0].data[0] | (frames[0].data[1] << 8));
    EXPECT_EQ(angle, -900);
}

TEST_F(BMWClusterSimTest, SAS_AngleClamping) {
    sim->enableASC(false);
    sim->enableEGS(false);
    sim->enableSAS(true);
    sim->enableABS(false);

    // Test clamping to max (720 degrees)
    sim->setSteeringAngle(1000);  // Should clamp to 720
    sim->tick();

    auto frames = interface->getFramesById(bmw::SAS_ID);
    int16_t angle = static_cast<int16_t>(frames[0].data[0] | (frames[0].data[1] << 8));
    EXPECT_EQ(angle, 7200);  // 720 × 10
}

TEST_F(BMWClusterSimTest, ABS_FrameFormat) {
    sim->enableASC(false);
    sim->enableEGS(false);
    sim->enableSAS(false);
    sim->enableABS(true);

    sim->setAllWheelSpeeds(1000);  // 100 km/h × 10

    // ABS sends when tickCounter % 2 == 0, need 2 ticks
    sim->tick();
    sim->tick();

    // Should have both front and rear frames
    auto frontFrames = interface->getFramesById(bmw::ABS1_ID);
    auto rearFrames = interface->getFramesById(bmw::ABS2_ID);

    ASSERT_EQ(frontFrames.size(), 1);
    ASSERT_EQ(rearFrames.size(), 1);

    EXPECT_EQ(frontFrames[0].id, 0x1F0);
    EXPECT_EQ(rearFrames[0].id, 0x1F5);
}

TEST_F(BMWClusterSimTest, ABS_WheelSpeeds) {
    sim->enableASC(false);
    sim->enableEGS(false);
    sim->enableSAS(false);
    sim->enableABS(true);

    // Set 100 km/h (1000 in ×10 format)
    sim->setWheelSpeedFL(1000);
    sim->setWheelSpeedFR(1000);
    sim->setWheelSpeedRL(1000);
    sim->setWheelSpeedRR(1000);

    // ABS sends when tickCounter % 2 == 0, need 2 ticks
    sim->tick();
    sim->tick();

    auto frontFrames = interface->getFramesById(bmw::ABS1_ID);
    ASSERT_EQ(frontFrames.size(), 1);

    const auto& frame = frontFrames[0];
    // Speed conversion: (speed × 16) / 10 = (1000 × 16) / 10 = 1600
    uint16_t flSpeed = frame.data[0] | (frame.data[1] << 8);
    EXPECT_EQ(flSpeed, 1600);
}

TEST_F(BMWClusterSimTest, Statistics) {
    sim->enableASC(true);
    sim->enableEGS(true);
    sim->enableSAS(true);
    sim->enableABS(true);

    EXPECT_EQ(sim->getFramesSent(), 0);
    EXPECT_EQ(sim->getFramesFailed(), 0);

    // Tick 5 times to hit all rate-limited sends:
    // - ASC sends at tick 5 (counter % 5 == 0): 1 frame
    // - EGS sends at tick 1 (counter % 5 == 1): 1 frame
    // - SAS sends every tick: 5 frames
    // - ABS sends at ticks 2,4 (counter % 2 == 0): 2 calls × 2 frames = 4 frames
    // Total: 1 + 1 + 5 + 4 = 11 frames
    for (int i = 0; i < 5; i++) sim->tick();

    EXPECT_EQ(sim->getFramesSent(), 11);
    EXPECT_EQ(sim->getFramesFailed(), 0);
}

TEST_F(BMWClusterSimTest, BMWGearEnum) {
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::PARK), 0x50);     // 'P'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::REVERSE), 0x52);  // 'R'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::NEUTRAL), 0x4E);  // 'N'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::DRIVE), 0x44);    // 'D'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::GEAR_1), 0x31);   // '1'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::GEAR_2), 0x32);   // '2'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::GEAR_3), 0x33);   // '3'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::GEAR_4), 0x34);   // '4'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::GEAR_5), 0x35);   // '5'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::GEAR_6), 0x36);   // '6'
    EXPECT_EQ(static_cast<uint8_t>(BMWGear::MANUAL), 0x4D);   // 'M'
}

TEST_F(BMWClusterSimTest, BMWConstants) {
    EXPECT_EQ(bmw::ASC1_ID, 0x153);
    EXPECT_EQ(bmw::ASC2_ID, 0x154);
    EXPECT_EQ(bmw::EGS_ID, 0x43F);
    EXPECT_EQ(bmw::SAS_ID, 0x1D2);
    EXPECT_EQ(bmw::ABS1_ID, 0x1F0);
    EXPECT_EQ(bmw::ABS2_ID, 0x1F5);

    EXPECT_EQ(bmw::ASC_RATE_HZ, 10);
    EXPECT_EQ(bmw::EGS_RATE_HZ, 10);
    EXPECT_EQ(bmw::SAS_RATE_HZ, 50);
    EXPECT_EQ(bmw::ABS_RATE_HZ, 20);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

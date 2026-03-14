#include "can_service/wideband_forwarder.hpp"

#include "mock_can_interface.hpp"

#include <gtest/gtest.h>

using namespace speeduino;
using speeduino::testing::MockCanInterface;

// ═══════════════════════════════════════════════════════════════════════════════
// Wideband Forwarder Tests
// ═══════════════════════════════════════════════════════════════════════════════

class WidebandForwarderTest : public ::testing::Test {
protected:
    void SetUp() override {
        interface = std::make_unique<MockCanInterface>();
        forwarder = std::make_unique<WidebandForwarder>(*interface);
    }

    std::unique_ptr<MockCanInterface> interface;
    std::unique_ptr<WidebandForwarder> forwarder;
};

TEST_F(WidebandForwarderTest, DefaultState) {
    EXPECT_EQ(forwarder->getFormat(), WidebandFormat::AEM_XSERIES);
    EXPECT_EQ(forwarder->getCanId(), wideband::AEM_XSERIES_ID);
    EXPECT_EQ(forwarder->getRate(), 20);
    EXPECT_FALSE(forwarder->isEnabled());
    EXPECT_NEAR(forwarder->getLambda(), 1.0f, 0.001f);
}

TEST_F(WidebandForwarderTest, SetFormat_AEM) {
    forwarder->setFormat(WidebandFormat::AEM_XSERIES);

    EXPECT_EQ(forwarder->getFormat(), WidebandFormat::AEM_XSERIES);
    EXPECT_EQ(forwarder->getCanId(), 0x180);
}

TEST_F(WidebandForwarderTest, SetFormat_Innovate) {
    forwarder->setFormat(WidebandFormat::INNOVATE_LC2);

    EXPECT_EQ(forwarder->getFormat(), WidebandFormat::INNOVATE_LC2);
    EXPECT_EQ(forwarder->getCanId(), 0x190);
}

TEST_F(WidebandForwarderTest, SetFormat_PLX) {
    forwarder->setFormat(WidebandFormat::PLX_SM_AFR);

    EXPECT_EQ(forwarder->getFormat(), WidebandFormat::PLX_SM_AFR);
    EXPECT_EQ(forwarder->getCanId(), 0x181);
}

TEST_F(WidebandForwarderTest, SetFormat_Spartan) {
    forwarder->setFormat(WidebandFormat::SPARTAN_14POINT7);

    EXPECT_EQ(forwarder->getFormat(), WidebandFormat::SPARTAN_14POINT7);
    EXPECT_EQ(forwarder->getCanId(), 0x182);
}

TEST_F(WidebandForwarderTest, SetCanIdOverride) {
    forwarder->setCanId(0x200);

    EXPECT_EQ(forwarder->getCanId(), 0x200);
}

TEST_F(WidebandForwarderTest, SetRate) {
    forwarder->setRate(50);
    EXPECT_EQ(forwarder->getRate(), 50);

    // Clamped to max
    forwarder->setRate(100);
    EXPECT_EQ(forwarder->getRate(), 50);

    // Clamped to min
    forwarder->setRate(0);
    EXPECT_EQ(forwarder->getRate(), 1);
}

TEST_F(WidebandForwarderTest, SetLambda) {
    forwarder->setLambda(0.85f);
    EXPECT_NEAR(forwarder->getLambda(), 0.85f, 0.001f);

    // Clamped to max
    forwarder->setLambda(3.0f);
    EXPECT_NEAR(forwarder->getLambda(), wideband::LAMBDA_MAX, 0.001f);

    // Clamped to min
    forwarder->setLambda(0.2f);
    EXPECT_NEAR(forwarder->getLambda(), wideband::LAMBDA_MIN, 0.001f);
}

TEST_F(WidebandForwarderTest, SetAFR) {
    forwarder->setAFR(14.7f);  // Stoich
    EXPECT_NEAR(forwarder->getLambda(), 1.0f, 0.01f);
    EXPECT_NEAR(forwarder->getAFR(), 14.7f, 0.1f);

    forwarder->setAFR(12.5f);  // Rich
    EXPECT_NEAR(forwarder->getLambda(), 12.5f / 14.7f, 0.01f);
}

TEST_F(WidebandForwarderTest, SendNow_Disabled) {
    forwarder->setEnabled(false);
    bool result = forwarder->sendNow();

    EXPECT_FALSE(result);
    EXPECT_TRUE(interface->getSentFrames().empty());
}

TEST_F(WidebandForwarderTest, SendNow_Enabled_AEM) {
    forwarder->setFormat(WidebandFormat::AEM_XSERIES);
    forwarder->setEnabled(true);
    forwarder->setLambda(1.0f);

    bool result = forwarder->sendNow();

    ASSERT_TRUE(result);
    ASSERT_EQ(interface->getSentFrames().size(), 1);

    const auto& frame = interface->getLastFrame();
    EXPECT_EQ(frame.id, 0x180);
    EXPECT_EQ(frame.dlc, 8);

    // Lambda × 10000 = 10000 = 0x2710 (Big Endian)
    EXPECT_EQ(frame.data[0], 0x27);
    EXPECT_EQ(frame.data[1], 0x10);
}

TEST_F(WidebandForwarderTest, SendNow_Enabled_Innovate) {
    forwarder->setFormat(WidebandFormat::INNOVATE_LC2);
    forwarder->setEnabled(true);
    forwarder->setLambda(1.0f);

    forwarder->sendNow();

    const auto& frame = interface->getLastFrame();
    EXPECT_EQ(frame.id, 0x190);

    // AFR × 10 = 147 = 0x0093 (Big Endian)
    EXPECT_EQ(frame.data[0], 0x00);
    EXPECT_EQ(frame.data[1], 0x93);

    // Lambda × 1000 = 1000 = 0x03E8 (Big Endian)
    EXPECT_EQ(frame.data[2], 0x03);
    EXPECT_EQ(frame.data[3], 0xE8);
}

TEST_F(WidebandForwarderTest, SendNow_Enabled_PLX) {
    forwarder->setFormat(WidebandFormat::PLX_SM_AFR);
    forwarder->setEnabled(true);
    forwarder->setLambda(1.0f);

    forwarder->sendNow();

    const auto& frame = interface->getLastFrame();
    EXPECT_EQ(frame.id, 0x181);

    // AFR × 100 = 1470 = 0x05BE (Little Endian!)
    EXPECT_EQ(frame.data[0], 0xBE);
    EXPECT_EQ(frame.data[1], 0x05);
}

TEST_F(WidebandForwarderTest, SendNow_Enabled_Spartan) {
    forwarder->setFormat(WidebandFormat::SPARTAN_14POINT7);
    forwarder->setEnabled(true);
    forwarder->setLambda(1.0f);

    forwarder->sendNow();

    const auto& frame = interface->getLastFrame();
    EXPECT_EQ(frame.id, 0x182);

    // Lambda × 1000 = 1000 = 0x03E8 (Big Endian)
    EXPECT_EQ(frame.data[0], 0x03);
    EXPECT_EQ(frame.data[1], 0xE8);
}

TEST_F(WidebandForwarderTest, RichLambda_AEM) {
    forwarder->setFormat(WidebandFormat::AEM_XSERIES);
    forwarder->setEnabled(true);
    forwarder->setLambda(0.85f);  // Rich

    forwarder->sendNow();

    const auto& frame = interface->getLastFrame();

    // Lambda × 10000 = 8500 = 0x2134 (Big Endian)
    uint16_t lambda = (frame.data[0] << 8) | frame.data[1];
    EXPECT_EQ(lambda, 8500);
}

TEST_F(WidebandForwarderTest, LeanLambda_AEM) {
    forwarder->setFormat(WidebandFormat::AEM_XSERIES);
    forwarder->setEnabled(true);
    forwarder->setLambda(1.1f);  // Lean

    forwarder->sendNow();

    const auto& frame = interface->getLastFrame();

    // Lambda × 10000 = 11000 = 0x2AF8 (Big Endian)
    uint16_t lambda = (frame.data[0] << 8) | frame.data[1];
    EXPECT_EQ(lambda, 11000);
}

TEST_F(WidebandForwarderTest, Statistics) {
    forwarder->setEnabled(true);

    EXPECT_EQ(forwarder->getFramesSent(), 0);
    EXPECT_EQ(forwarder->getFramesFailed(), 0);

    forwarder->sendNow();
    forwarder->sendNow();
    forwarder->sendNow();

    EXPECT_EQ(forwarder->getFramesSent(), 3);
    EXPECT_EQ(forwarder->getFramesFailed(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

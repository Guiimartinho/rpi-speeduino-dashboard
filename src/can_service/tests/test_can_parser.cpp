#include <gtest/gtest.h>
#include "can_service/can_parser.hpp"
#include "can_service/can_interface.hpp"

using namespace speeduino;

class CanParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Load Haltech signals
        std::vector<CanSignalDef> signals = {
            {"rpm", 0x360, 0, 0, 16, true, false, 1.0, 0, "rpm"},
            {"map", 0x360, 2, 0, 16, true, false, 0.1, 0, "kPa"},
            {"tps", 0x360, 4, 0, 16, true, false, 0.1, 0, "%"},
            {"coolant_temp", 0x3E0, 0, 0, 16, true, false, 0.1, -273.0, "C"},
            {"lambda1", 0x368, 0, 0, 16, true, false, 0.001, 0, ""}
        };
        parser.loadSignals(signals);
    }

    CanParser parser;
};

TEST_F(CanParserTest, ParseHaltechRPM) {
    CanFrame frame;
    frame.id = 0x360;
    frame.dlc = 8;
    // RPM = 3000 = 0x0BB8 (big endian)
    frame.data = {0x0B, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    parser.parseFrame(frame);

    auto rpm = parser.getSignalValue("rpm");
    ASSERT_TRUE(rpm.has_value());
    EXPECT_EQ(static_cast<int>(*rpm), 3000);
}

TEST_F(CanParserTest, ParseHaltechMAP) {
    CanFrame frame;
    frame.id = 0x360;
    frame.dlc = 8;
    // MAP = 101.5 kPa = 1015 = 0x03F7 (big endian)
    frame.data = {0x00, 0x00, 0x03, 0xF7, 0x00, 0x00, 0x00, 0x00};

    parser.parseFrame(frame);

    auto map = parser.getSignalValue("map");
    ASSERT_TRUE(map.has_value());
    EXPECT_NEAR(*map, 101.5, 0.1);
}

TEST_F(CanParserTest, ParseHaltechTemperature) {
    CanFrame frame;
    frame.id = 0x3E0;
    frame.dlc = 8;
    // CLT = 90°C = 363K = 3630 = 0x0E2E (big endian)
    frame.data = {0x0E, 0x2E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    parser.parseFrame(frame);

    auto clt = parser.getSignalValue("coolant_temp");
    ASSERT_TRUE(clt.has_value());
    EXPECT_NEAR(*clt, 90.0, 0.1);
}

TEST_F(CanParserTest, ParseHaltechLambda) {
    CanFrame frame;
    frame.id = 0x368;
    frame.dlc = 8;
    // Lambda = 1.0 = 1000 = 0x03E8 (big endian)
    frame.data = {0x03, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    parser.parseFrame(frame);

    auto lambda = parser.getSignalValue("lambda1");
    ASSERT_TRUE(lambda.has_value());
    EXPECT_NEAR(*lambda, 1.0, 0.001);
}

TEST_F(CanParserTest, EngineDataAggregation) {
    // Send multiple frames
    CanFrame frame360;
    frame360.id = 0x360;
    frame360.dlc = 8;
    frame360.data = {0x0B, 0xB8, 0x03, 0x20, 0x01, 0xF4, 0x00, 0x00};
    // RPM=3000, MAP=80.0, TPS=50.0

    CanFrame frame3E0;
    frame3E0.id = 0x3E0;
    frame3E0.dlc = 8;
    frame3E0.data = {0x0E, 0x2E, 0x0D, 0xAC, 0x00, 0x00, 0x00, 0x00};
    // CLT=90°C, IAT=35°C

    parser.parseFrame(frame360);
    parser.parseFrame(frame3E0);

    auto data = parser.getEngineData();

    EXPECT_EQ(data.rpm, 3000);
    EXPECT_EQ(data.coolant_temp, 90);
    EXPECT_TRUE(data.isEngineRunning());
}

TEST_F(CanParserTest, DataFreshness) {
    CanFrame frame;
    frame.id = 0x360;
    frame.dlc = 8;
    frame.data = {0x0B, 0xB8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    parser.parseFrame(frame);
    EXPECT_TRUE(parser.isDataFresh(500));

    // After reset, data should not be fresh
    parser.reset();
    EXPECT_FALSE(parser.isDataFresh(500));
}

TEST_F(CanParserTest, UnknownFrameIgnored) {
    CanFrame frame;
    frame.id = 0x999; // Unknown frame
    frame.dlc = 8;
    frame.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    parser.parseFrame(frame);

    // RPM should still be 0 (no update)
    auto rpm = parser.getSignalValue("rpm");
    EXPECT_FALSE(rpm.has_value());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

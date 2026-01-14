#include <gtest/gtest.h>
#include <algorithm>
#include "can_service/obd_handler.hpp"

using namespace speeduino;

// ═══════════════════════════════════════════════════════════════════════════════
// Mock CAN Interface for OBD Testing
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
        if (!m_responseQueue.empty()) {
            auto frame = m_responseQueue.front();
            m_responseQueue.erase(m_responseQueue.begin());
            return frame;
        }
        return std::nullopt;
    }
    bool send(const CanFrame& frame) override {
        m_sentFrames.push_back(frame);
        return true;
    }

    void queueResponse(const CanFrame& frame) {
        m_responseQueue.push_back(frame);
    }

    const std::vector<CanFrame>& getSentFrames() const { return m_sentFrames; }
    void clearSentFrames() { m_sentFrames.clear(); }

private:
    std::vector<CanFrame> m_sentFrames;
    std::vector<CanFrame> m_responseQueue;
};

// ═══════════════════════════════════════════════════════════════════════════════
// OBD Handler Tests
// ═══════════════════════════════════════════════════════════════════════════════

class OBDHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        interface = std::make_unique<MockCanInterface>();
        handler = std::make_unique<OBDHandler>(*interface);
    }

    std::unique_ptr<MockCanInterface> interface;
    std::unique_ptr<OBDHandler> handler;
};

TEST_F(OBDHandlerTest, RequestRPM) {
    // Queue a valid RPM response
    CanFrame response;
    response.id = 0x7E8;
    response.dlc = 8;
    // Mode 01, PID 0C, RPM = 3000 = 12000 / 4 = 0x2EE0
    response.data = {0x04, 0x41, 0x0C, 0x2E, 0xE0, 0x00, 0x00, 0x00};
    interface->queueResponse(response);

    auto result = handler->requestPID(obd::PID_RPM);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pid, obd::PID_RPM);
    EXPECT_NEAR(result->value, 3000.0, 0.1);
    EXPECT_EQ(result->unit, "rpm");
}

TEST_F(OBDHandlerTest, RequestCoolantTemp) {
    // Queue a valid coolant temp response
    CanFrame response;
    response.id = 0x7E8;
    response.dlc = 8;
    // Mode 01, PID 05, CLT = 90°C = 130 (raw + offset)
    response.data = {0x03, 0x41, 0x05, 0x82, 0x00, 0x00, 0x00, 0x00};
    interface->queueResponse(response);

    auto result = handler->requestPID(obd::PID_COOLANT_TEMP);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->pid, obd::PID_COOLANT_TEMP);
    EXPECT_NEAR(result->value, 90.0, 0.1);
    EXPECT_EQ(result->unit, "C");
}

TEST_F(OBDHandlerTest, RequestVehicleSpeed) {
    CanFrame response;
    response.id = 0x7E8;
    response.dlc = 8;
    // Mode 01, PID 0D, Speed = 100 km/h
    response.data = {0x03, 0x41, 0x0D, 0x64, 0x00, 0x00, 0x00, 0x00};
    interface->queueResponse(response);

    auto result = handler->requestPID(obd::PID_SPEED);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value, 100.0);
    EXPECT_EQ(result->unit, "km/h");
}

TEST_F(OBDHandlerTest, RequestTPS) {
    CanFrame response;
    response.id = 0x7E8;
    response.dlc = 8;
    // Mode 01, PID 11, TPS = 50% = 128 raw
    response.data = {0x03, 0x41, 0x11, 0x80, 0x00, 0x00, 0x00, 0x00};
    interface->queueResponse(response);

    auto result = handler->requestPID(obd::PID_TPS);

    ASSERT_TRUE(result.has_value());
    EXPECT_NEAR(result->value, 50.2, 0.5);
    EXPECT_EQ(result->unit, "%");
}

TEST_F(OBDHandlerTest, RequestMAP) {
    CanFrame response;
    response.id = 0x7E8;
    response.dlc = 8;
    // Mode 01, PID 0B, MAP = 101 kPa
    response.data = {0x03, 0x41, 0x0B, 0x65, 0x00, 0x00, 0x00, 0x00};
    interface->queueResponse(response);

    auto result = handler->requestPID(obd::PID_MAP);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->value, 101.0);
    EXPECT_EQ(result->unit, "kPa");
}

TEST_F(OBDHandlerTest, DTCParsing) {
    // Test DTC from raw bytes
    // P0300 = 0x0300
    DTC dtc = DTC::fromBytes(0x03, 0x00);

    EXPECT_EQ(dtc.code, "P0300");
}

TEST_F(OBDHandlerTest, DTCParsingChassis) {
    // C0100 = 0x4100
    DTC dtc = DTC::fromBytes(0x41, 0x00);

    EXPECT_EQ(dtc.code, "C0100");
}

TEST_F(OBDHandlerTest, DTCParsingBody) {
    // B0200 = 0x8200
    DTC dtc = DTC::fromBytes(0x82, 0x00);

    EXPECT_EQ(dtc.code, "B0200");
}

TEST_F(OBDHandlerTest, DTCParsingNetwork) {
    // U0100 = 0xC100
    DTC dtc = DTC::fromBytes(0xC1, 0x00);

    EXPECT_EQ(dtc.code, "U0100");
}

TEST_F(OBDHandlerTest, RequestFrameFormat) {
    // Request RPM without queueing response (will timeout)
    handler->requestPID(obd::PID_RPM);

    auto& sentFrames = interface->getSentFrames();
    ASSERT_EQ(sentFrames.size(), 1);

    const auto& frame = sentFrames[0];
    EXPECT_EQ(frame.id, 0x7DF);  // OBD broadcast ID
    EXPECT_EQ(frame.dlc, 8);
    EXPECT_EQ(frame.data[0], 0x02);  // Length = 2
    EXPECT_EQ(frame.data[1], 0x01);  // Mode 01
    EXPECT_EQ(frame.data[2], 0x0C);  // PID 0C (RPM)
}

TEST_F(OBDHandlerTest, VINRequest) {
    // Queue VIN response (Mode 09, PID 02)
    CanFrame response;
    response.id = 0x7E8;
    response.dlc = 8;
    // First frame of multi-frame VIN response
    response.data = {0x10, 0x14, 0x49, 0x02, 0x01, 0x57, 0x56, 0x57};  // "WVW..."
    interface->queueResponse(response);

    // Flow control
    interface->clearSentFrames();
    auto result = handler->getVIN();

    // Should send request frame
    auto& sentFrames = interface->getSentFrames();
    ASSERT_GE(sentFrames.size(), 1);
    EXPECT_EQ(sentFrames[0].data[1], 0x09);  // Mode 09
    EXPECT_EQ(sentFrames[0].data[2], 0x02);  // PID 02 (VIN)
}

TEST_F(OBDHandlerTest, TimeoutHandling) {
    // Don't queue any response - should timeout
    auto result = handler->requestPID(obd::PID_RPM);

    EXPECT_FALSE(result.has_value());
}

TEST_F(OBDHandlerTest, InvalidResponseIgnored) {
    // Queue invalid response (wrong response ID)
    CanFrame response;
    response.id = 0x7E9;  // Wrong ID
    response.dlc = 8;
    response.data = {0x04, 0x41, 0x0C, 0x2E, 0xE0, 0x00, 0x00, 0x00};
    interface->queueResponse(response);

    auto result = handler->requestPID(obd::PID_RPM);

    EXPECT_FALSE(result.has_value());
}

TEST_F(OBDHandlerTest, GetSupportedPIDs_SingleRange) {
    // Queue response for PID 0x00 (supported PIDs 01-20)
    // Bitmap: 0xBE 0x1F 0xA8 0x10
    // Byte 0: 0xBE = 10111110 -> PIDs 01, 03, 04, 05, 06, 07 supported
    // Byte 1: 0x1F = 00011111 -> PIDs 0C, 0D, 0E, 0F, 10 supported
    // Byte 2: 0xA8 = 10101000 -> PIDs 11, 13, 15 supported
    // Byte 3: 0x10 = 00010000 -> PID 1C supported, PID 0x20 NOT set (no next range)
    CanFrame response;
    response.id = 0x7E8;
    response.dlc = 8;
    response.data = {0x06, 0x41, 0x00, 0xBE, 0x1F, 0xA8, 0x10, 0x00};
    interface->queueResponse(response);

    auto supported = handler->getSupportedPIDs();

    // Check expected PIDs are in the list
    EXPECT_TRUE(std::find(supported.begin(), supported.end(), 0x01) != supported.end());
    EXPECT_TRUE(std::find(supported.begin(), supported.end(), 0x03) != supported.end());
    EXPECT_TRUE(std::find(supported.begin(), supported.end(), 0x05) != supported.end());
    EXPECT_TRUE(std::find(supported.begin(), supported.end(), 0x0C) != supported.end());
    EXPECT_TRUE(std::find(supported.begin(), supported.end(), 0x0D) != supported.end());
    EXPECT_TRUE(std::find(supported.begin(), supported.end(), 0x11) != supported.end());

    // PID 02 should NOT be in the list (bit not set)
    EXPECT_TRUE(std::find(supported.begin(), supported.end(), 0x02) == supported.end());
}

TEST_F(OBDHandlerTest, GetSupportedPIDs_MultipleRanges) {
    // First range: 0x00 (PIDs 01-20), with 0x20 bit set
    CanFrame response1;
    response1.id = 0x7E8;
    response1.dlc = 8;
    response1.data = {0x06, 0x41, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00};  // Only PID 01 + next range
    interface->queueResponse(response1);

    // Second range: 0x20 (PIDs 21-40), no next range
    CanFrame response2;
    response2.id = 0x7E8;
    response2.dlc = 8;
    response2.data = {0x06, 0x41, 0x20, 0x80, 0x00, 0x00, 0x00, 0x00};  // Only PID 21
    interface->queueResponse(response2);

    auto supported = handler->getSupportedPIDs();

    // Should have PIDs from both ranges
    EXPECT_TRUE(std::find(supported.begin(), supported.end(), 0x01) != supported.end());
    EXPECT_TRUE(std::find(supported.begin(), supported.end(), 0x21) != supported.end());
    EXPECT_EQ(supported.size(), 2);
}

TEST_F(OBDHandlerTest, VIN_MultiFrame_Complete) {
    // First Frame: VIN starts with "WVWZZZ3CZ"
    // Format: [10 14] [49 02 01] [W V W] = length 0x14 (20 bytes)
    CanFrame ff;
    ff.id = 0x7E8;
    ff.dlc = 8;
    ff.data = {0x10, 0x14, 0x49, 0x02, 0x01, 'W', 'V', 'W'};
    interface->queueResponse(ff);

    // Consecutive Frame 1: "ZZZ3CZW"
    CanFrame cf1;
    cf1.id = 0x7E8;
    cf1.dlc = 8;
    cf1.data = {0x21, 'Z', 'Z', 'Z', '3', 'C', 'Z', 'W'};
    interface->queueResponse(cf1);

    // Consecutive Frame 2: "E123456" + padding
    CanFrame cf2;
    cf2.id = 0x7E8;
    cf2.dlc = 8;
    cf2.data = {0x22, 'E', '1', '2', '3', '4', '5', '6'};
    interface->queueResponse(cf2);

    auto result = handler->getVIN();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->length(), 17);
    EXPECT_EQ(*result, "WVWZZZ3CZWE123456");

    // Verify flow control was sent
    auto& sent = interface->getSentFrames();
    bool foundFC = false;
    for (const auto& frame : sent) {
        if ((frame.data[0] & 0xF0) == 0x30) {  // Flow Control
            foundFC = true;
            EXPECT_EQ(frame.data[0], 0x30);  // CTS
            EXPECT_EQ(frame.data[1], 0x00);  // No block size limit
            EXPECT_EQ(frame.data[2], 0x00);  // No separation time
            break;
        }
    }
    EXPECT_TRUE(foundFC) << "Flow control frame not sent";
}

TEST_F(OBDHandlerTest, VIN_SingleFrame) {
    // Some ECUs might respond with a short VIN in single frame (unusual but possible)
    CanFrame sf;
    sf.id = 0x7E8;
    sf.dlc = 8;
    sf.data = {0x07, 0x49, 0x02, 0x01, 'T', 'E', 'S', 'T'};
    interface->queueResponse(sf);

    auto result = handler->getVIN();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "TEST");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

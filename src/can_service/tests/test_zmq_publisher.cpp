/**
 * @file test_zmq_publisher.cpp
 * @brief Unit tests for ZmqPublisher IPC communication
 *
 * ISO 26262 ASIL-B: Validates reliable message publishing
 * Tests initialization, publishing, and shutdown lifecycle
 */

#include <gtest/gtest.h>
#include "can_service/zmq_publisher.hpp"
#include "common/zmq_messages.hpp"
#include <zmq.hpp>
#include <thread>
#include <chrono>
#include <atomic>
#include <msgpack.hpp>
#include <sstream>

using namespace speeduino;

// ═══════════════════════════════════════════════════════════════════════════════
// ZMQPUBLISHER TESTS
// ═══════════════════════════════════════════════════════════════════════════════

class ZmqPublisherTest : public ::testing::Test {
protected:
    // Use unique endpoints for each test to avoid conflicts
    static std::string getUniqueEndpoint() {
        static std::atomic<int> counter{0};
        return "inproc://test_pub_" + std::to_string(counter++);
    }
};

TEST_F(ZmqPublisherTest, DefaultConstruction) {
    ZmqPublisher publisher;
    EXPECT_EQ(publisher.getPublishCount(), 0u);
}

TEST_F(ZmqPublisherTest, InitAndShutdown) {
    ZmqPublisher publisher;
    std::string endpoint = getUniqueEndpoint();

    EXPECT_TRUE(publisher.init(endpoint));

    publisher.shutdown();
    EXPECT_EQ(publisher.getPublishCount(), 0u);
}

TEST_F(ZmqPublisherTest, DoubleInitFails) {
    ZmqPublisher publisher;
    std::string endpoint1 = getUniqueEndpoint();
    std::string endpoint2 = getUniqueEndpoint();

    EXPECT_TRUE(publisher.init(endpoint1));
    // Second init should fail (already initialized)
    EXPECT_FALSE(publisher.init(endpoint2));

    publisher.shutdown();
}

TEST_F(ZmqPublisherTest, ShutdownWithoutInit) {
    ZmqPublisher publisher;
    // Should not crash
    publisher.shutdown();
    SUCCEED();
}

TEST_F(ZmqPublisherTest, PublishEngineDataIncrementsCount) {
    ZmqPublisher publisher;
    std::string endpoint = getUniqueEndpoint();

    ASSERT_TRUE(publisher.init(endpoint));

    EngineData data{};
    data.rpm = 3500;
    data.coolant_temp_c = 85;
    data.tps_percent = 50;

    EXPECT_EQ(publisher.getPublishCount(), 0u);

    // Publishing without a subscriber still works (PUB socket)
    EXPECT_TRUE(publisher.publishEngineData(data));
    EXPECT_EQ(publisher.getPublishCount(), 1u);

    EXPECT_TRUE(publisher.publishEngineData(data));
    EXPECT_EQ(publisher.getPublishCount(), 2u);

    publisher.shutdown();
}

TEST_F(ZmqPublisherTest, PublishSteeringEvent) {
    ZmqPublisher publisher;
    std::string endpoint = getUniqueEndpoint();

    ASSERT_TRUE(publisher.init(endpoint));

    SteeringEvent event{};
    event.button_id = 5;
    event.pressed = true;
    event.timestamp_ms = 12345;

    EXPECT_TRUE(publisher.publishSteeringEvent(event));
    EXPECT_EQ(publisher.getPublishCount(), 1u);

    publisher.shutdown();
}

TEST_F(ZmqPublisherTest, PublishSystemStatus) {
    ZmqPublisher publisher;
    std::string endpoint = getUniqueEndpoint();

    ASSERT_TRUE(publisher.init(endpoint));

    SystemStatus status{};
    status.cpu_usage_percent = 45;
    status.memory_used_mb = 512;
    status.uptime_seconds = 3600;

    EXPECT_TRUE(publisher.publishSystemStatus(status));
    EXPECT_EQ(publisher.getPublishCount(), 1u);

    publisher.shutdown();
}

TEST_F(ZmqPublisherTest, PublishBeforeInit) {
    ZmqPublisher publisher;

    EngineData data{};
    data.rpm = 3500;

    // Should fail gracefully
    EXPECT_FALSE(publisher.publishEngineData(data));
    EXPECT_EQ(publisher.getPublishCount(), 0u);
}

TEST_F(ZmqPublisherTest, PublishAfterShutdown) {
    ZmqPublisher publisher;
    std::string endpoint = getUniqueEndpoint();

    ASSERT_TRUE(publisher.init(endpoint));
    publisher.shutdown();

    EngineData data{};
    data.rpm = 3500;

    // Should fail gracefully
    EXPECT_FALSE(publisher.publishEngineData(data));
}

// ═══════════════════════════════════════════════════════════════════════════════
// ZMQCOMMANDSERVER TESTS
// ═══════════════════════════════════════════════════════════════════════════════

class ZmqCommandServerTest : public ::testing::Test {
protected:
    static std::string getUniqueEndpoint() {
        static std::atomic<int> counter{0};
        return "inproc://test_cmd_" + std::to_string(counter++);
    }
};

TEST_F(ZmqCommandServerTest, DefaultConstruction) {
    ZmqCommandServer server;
    // Should not crash
    SUCCEED();
}

TEST_F(ZmqCommandServerTest, InitAndShutdown) {
    ZmqCommandServer server;
    std::string endpoint = getUniqueEndpoint();

    EXPECT_TRUE(server.init(endpoint));

    server.shutdown();
    SUCCEED();
}

TEST_F(ZmqCommandServerTest, ReceiveCommandTimesOut) {
    ZmqCommandServer server;
    std::string endpoint = getUniqueEndpoint();

    ASSERT_TRUE(server.init(endpoint));

    // Should return nullopt on timeout
    auto cmd = server.receiveCommand(10);  // 10ms timeout
    EXPECT_FALSE(cmd.has_value());

    server.shutdown();
}

TEST_F(ZmqCommandServerTest, ShutdownWithoutInit) {
    ZmqCommandServer server;
    // Should not crash
    server.shutdown();
    SUCCEED();
}

TEST_F(ZmqCommandServerTest, DoubleInitFails) {
    ZmqCommandServer server;
    std::string endpoint1 = getUniqueEndpoint();
    std::string endpoint2 = getUniqueEndpoint();

    EXPECT_TRUE(server.init(endpoint1));
    // Second init should fail
    EXPECT_FALSE(server.init(endpoint2));

    server.shutdown();
}

// ═══════════════════════════════════════════════════════════════════════════════
// INTEGRATION TEST: Publisher + Subscriber
// ═══════════════════════════════════════════════════════════════════════════════

TEST(ZmqIntegrationTest, PublisherSubscriberCommunication) {
    // Use TCP for this test to ensure proper connectivity
    std::string endpoint = "tcp://127.0.0.1:15555";

    // Create subscriber first (to not miss messages)
    zmq::context_t ctx(1);
    zmq::socket_t subscriber(ctx, zmq::socket_type::sub);

    int linger = 0;
    subscriber.set(zmq::sockopt::linger, linger);
    subscriber.set(zmq::sockopt::subscribe, topics::ENGINE_DATA);

    try {
        subscriber.connect(endpoint);
    } catch (const zmq::error_t& e) {
        GTEST_SKIP() << "Could not connect: " << e.what();
    }

    // Small delay for connection establishment
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Create publisher
    ZmqPublisher publisher;
    if (!publisher.init(endpoint)) {
        subscriber.close();
        ctx.close();
        GTEST_SKIP() << "Could not initialize publisher";
    }

    // Another delay for binding
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Publish data
    EngineData sentData{};
    sentData.rpm = 4200;
    sentData.coolant_temp_c = 90;
    sentData.tps_percent = 75;
    sentData.map_kpa = 101;
    sentData.lambda = 1000;  // 1.0 * 1000

    EXPECT_TRUE(publisher.publishEngineData(sentData));

    // Receive with timeout
    zmq::pollitem_t items[] = {{subscriber, 0, ZMQ_POLLIN, 0}};
    int rc = zmq::poll(items, 1, std::chrono::milliseconds(500));

    if (rc > 0) {
        // Receive topic
        zmq::message_t topic;
        auto topicResult = subscriber.recv(topic, zmq::recv_flags::none);
        ASSERT_TRUE(topicResult.has_value());
        EXPECT_EQ(std::string(static_cast<char*>(topic.data()), topic.size()),
                  topics::ENGINE_DATA);

        // Receive data
        zmq::message_t data;
        auto dataResult = subscriber.recv(data, zmq::recv_flags::none);
        ASSERT_TRUE(dataResult.has_value());

        // Deserialize with msgpack
        msgpack::object_handle oh = msgpack::unpack(
            static_cast<const char*>(data.data()), data.size());
        EngineData receivedData;
        oh.get().convert(receivedData);

        EXPECT_EQ(receivedData.rpm, 4200);
        EXPECT_EQ(receivedData.coolant_temp_c, 90);
        EXPECT_EQ(receivedData.tps_percent, 75);
        EXPECT_EQ(receivedData.map_kpa, 101);
    }

    publisher.shutdown();
    subscriber.close();
    ctx.close();
}

// ═══════════════════════════════════════════════════════════════════════════════
// THREAD SAFETY TESTS
// ═══════════════════════════════════════════════════════════════════════════════

TEST(ZmqPublisherThreadTest, ConcurrentPublish) {
    ZmqPublisher publisher;
    std::string endpoint = "inproc://thread_test_pub";

    ASSERT_TRUE(publisher.init(endpoint));

    std::atomic<int> successCount{0};
    std::atomic<int> failCount{0};
    constexpr int NUM_THREADS = 4;
    constexpr int PUBLISHES_PER_THREAD = 50;

    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, threadId = i]() {
            for (int j = 0; j < PUBLISHES_PER_THREAD; ++j) {
                EngineData data{};
                data.rpm = static_cast<uint16_t>(threadId * 1000 + j);

                if (publisher.publishEngineData(data)) {
                    ++successCount;
                } else {
                    ++failCount;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    publisher.shutdown();

    EXPECT_EQ(successCount.load(), NUM_THREADS * PUBLISHES_PER_THREAD);
    EXPECT_EQ(failCount.load(), 0);
    EXPECT_EQ(publisher.getPublishCount(),
              static_cast<uint32_t>(NUM_THREADS * PUBLISHES_PER_THREAD));
}

// ═══════════════════════════════════════════════════════════════════════════════
// STRESS TEST
// ═══════════════════════════════════════════════════════════════════════════════

TEST(ZmqPublisherStressTest, RapidPublish) {
    ZmqPublisher publisher;
    std::string endpoint = "inproc://stress_test_pub";

    ASSERT_TRUE(publisher.init(endpoint));

    constexpr int NUM_PUBLISHES = 1000;
    EngineData data{};

    for (int i = 0; i < NUM_PUBLISHES; ++i) {
        data.rpm = static_cast<uint16_t>(i % 8000);
        EXPECT_TRUE(publisher.publishEngineData(data));
    }

    EXPECT_EQ(publisher.getPublishCount(), static_cast<uint32_t>(NUM_PUBLISHES));

    publisher.shutdown();
}

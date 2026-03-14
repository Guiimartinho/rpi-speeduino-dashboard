#ifndef CAN_SERVICE_ZMQ_PUBLISHER_HPP
#define CAN_SERVICE_ZMQ_PUBLISHER_HPP

#include "common/zmq_messages.hpp"

#include <zmq.hpp>

#include <atomic>
#include <memory>
#include <string>

namespace speeduino {

class ZmqPublisher {
public:
    ZmqPublisher();
    ~ZmqPublisher();

    // Initialize publisher socket
    bool init(const std::string& endpoint);

    // Shutdown
    void shutdown();

    // Publish engine data
    bool publishEngineData(const EngineData& data);

    // Publish steering event
    bool publishSteeringEvent(const SteeringEvent& event);

    // Publish system status
    bool publishSystemStatus(const SystemStatus& status);

    // Get publish count
    uint32_t getPublishCount() const { return m_publishCount; }

private:
    // Serialize and send with topic
    template <typename T>
    bool publish(const char* topic, const T& data);

    std::unique_ptr<zmq::context_t> m_context;
    std::unique_ptr<zmq::socket_t> m_socket;
    std::atomic<bool> m_initialized{false};
    std::atomic<uint32_t> m_publishCount{0};
};

// REQ/REP server for CAN commands
class ZmqCommandServer {
public:
    ZmqCommandServer();
    ~ZmqCommandServer();

    // Initialize server socket
    bool init(const std::string& endpoint);

    // Shutdown
    void shutdown();

    // Receive command (blocking with timeout)
    // Returns nullopt on timeout
    std::optional<CanCommand> receiveCommand(int timeout_ms = 100);

    // Send response
    bool sendResponse(const CanCommandResponse& response);

private:
    std::unique_ptr<zmq::context_t> m_context;
    std::unique_ptr<zmq::socket_t> m_socket;
    std::atomic<bool> m_initialized{false};
};

}  // namespace speeduino

#endif  // CAN_SERVICE_ZMQ_PUBLISHER_HPP

#include "can_service/zmq_publisher.hpp"
#include "common/logger.hpp"
#include <msgpack.hpp>
#include <sstream>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// ZMQ Socket Configuration Constants
// ISO 26262: Named constants improve code readability and maintainability
// ═══════════════════════════════════════════════════════════════════════════════
namespace {
    /// High water mark for ZMQ publisher - drops old messages if subscriber is slow
    constexpr int ZMQ_PUBLISHER_HWM = 10;
    /// Linger time for ZMQ sockets (0 = don't wait on close)
    constexpr int ZMQ_LINGER_MS = 0;
} // anonymous namespace

// ZmqPublisher implementation
ZmqPublisher::ZmqPublisher() = default;

ZmqPublisher::~ZmqPublisher() {
    shutdown();
}

bool ZmqPublisher::init(const std::string& endpoint) {
    try {
        m_context = std::make_unique<zmq::context_t>(1);
        m_socket = std::make_unique<zmq::socket_t>(*m_context, zmq::socket_type::pub);

        // Set socket options
        m_socket->set(zmq::sockopt::linger, ZMQ_LINGER_MS);
        m_socket->set(zmq::sockopt::sndhwm, ZMQ_PUBLISHER_HWM);

        m_socket->bind(endpoint);

        m_initialized = true;
        LOG_INFO("ZMQ publisher bound to " + endpoint);
        return true;
    } catch (const zmq::error_t& e) {
        LOG_ERROR("Failed to init ZMQ publisher: " + std::string(e.what()));
        return false;
    }
}

void ZmqPublisher::shutdown() {
    if (m_initialized) {
        m_socket.reset();
        m_context.reset();
        m_initialized = false;
        LOG_INFO("ZMQ publisher shutdown");
    }
}

template<typename T>
bool ZmqPublisher::publish(const char* topic, const T& data) {
    if (!m_initialized) {
        return false;
    }

    try {
        // Serialize with msgpack
        std::stringstream ss;
        msgpack::pack(ss, data);
        std::string packed = ss.str();

        // Send topic frame
        zmq::message_t topic_msg(topic, std::strlen(topic));
        m_socket->send(topic_msg, zmq::send_flags::sndmore);

        // Send data frame
        zmq::message_t data_msg(packed.data(), packed.size());
        m_socket->send(data_msg, zmq::send_flags::none);

        m_publishCount++;
        return true;
    } catch (const zmq::error_t& e) {
        LOG_ERROR("ZMQ publish error: " + std::string(e.what()));
        return false;
    }
}

bool ZmqPublisher::publishEngineData(const EngineData& data) {
    return publish(topics::ENGINE, data);
}

bool ZmqPublisher::publishSteeringEvent(const SteeringEvent& event) {
    return publish(topics::STEERING, event);
}

bool ZmqPublisher::publishSystemStatus(const SystemStatus& status) {
    return publish(topics::STATUS, status);
}

// ZmqCommandServer implementation
ZmqCommandServer::ZmqCommandServer() = default;

ZmqCommandServer::~ZmqCommandServer() {
    shutdown();
}

bool ZmqCommandServer::init(const std::string& endpoint) {
    try {
        m_context = std::make_unique<zmq::context_t>(1);
        m_socket = std::make_unique<zmq::socket_t>(*m_context, zmq::socket_type::rep);

        m_socket->set(zmq::sockopt::linger, ZMQ_LINGER_MS);

        m_socket->bind(endpoint);

        m_initialized = true;
        LOG_INFO("ZMQ command server bound to " + endpoint);
        return true;
    } catch (const zmq::error_t& e) {
        LOG_ERROR("Failed to init ZMQ command server: " + std::string(e.what()));
        return false;
    }
}

void ZmqCommandServer::shutdown() {
    if (m_initialized) {
        m_socket.reset();
        m_context.reset();
        m_initialized = false;
        LOG_INFO("ZMQ command server shutdown");
    }
}

std::optional<CanCommand> ZmqCommandServer::receiveCommand(int timeout_ms) {
    if (!m_initialized) {
        return std::nullopt;
    }

    try {
        zmq::pollitem_t items[] = {{*m_socket, 0, ZMQ_POLLIN, 0}};
        zmq::poll(items, 1, std::chrono::milliseconds(timeout_ms));

        if (items[0].revents & ZMQ_POLLIN) {
            zmq::message_t msg;
            auto result = m_socket->recv(msg, zmq::recv_flags::none);
            if (!result) {
                return std::nullopt;
            }

            // Deserialize
            msgpack::object_handle oh = msgpack::unpack(
                static_cast<const char*>(msg.data()), msg.size());
            msgpack::object obj = oh.get();

            CanCommand cmd;
            obj.convert(cmd);
            return cmd;
        }
    } catch (const std::exception& e) {
        LOG_ERROR("ZMQ receive error: " + std::string(e.what()));
    }

    return std::nullopt;
}

bool ZmqCommandServer::sendResponse(const CanCommandResponse& response) {
    if (!m_initialized) {
        return false;
    }

    try {
        std::stringstream ss;
        msgpack::pack(ss, response);
        std::string packed = ss.str();

        zmq::message_t msg(packed.data(), packed.size());
        m_socket->send(msg, zmq::send_flags::none);
        return true;
    } catch (const zmq::error_t& e) {
        LOG_ERROR("ZMQ send response error: " + std::string(e.what()));
        return false;
    }
}

} // namespace speeduino

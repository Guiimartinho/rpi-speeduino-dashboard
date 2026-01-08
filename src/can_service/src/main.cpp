#include "can_service/can_interface.hpp"
#include "can_service/can_parser.hpp"
#include "can_service/can_writer.hpp"
#include "can_service/zmq_publisher.hpp"
#include "common/config_loader.hpp"
#include "common/logger.hpp"
#include "common/zmq_messages.hpp"

#include <csignal>
#include <chrono>
#include <thread>
#include <atomic>
#include <getopt.h>

namespace {

std::atomic<bool> g_running{true};

void signalHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        speeduino::Logger::info("Shutdown signal received");
        g_running = false;
    }
}

void printUsage(const char* progname) {
    std::cout << "Usage: " << progname << " [options]\n"
              << "Options:\n"
              << "  -i, --interface <name>   CAN interface (default: can0)\n"
              << "  -c, --config <dir>       Config directory (default: /etc/speeduino-ui)\n"
              << "  -v, --verbose            Enable debug logging\n"
              << "  -h, --help               Show this help\n";
}

// Forward declarations from signal_database.cpp
std::vector<speeduino::CanSignalDef> getSignalsForProtocol(const std::string& protocol);

} // anonymous namespace

int main(int argc, char* argv[]) {
    using namespace speeduino;

    // Default options
    std::string interfaceName = "can0";
    std::string configDir = "/etc/speeduino-ui";
    bool verbose = false;

    // Parse command line
    static struct option long_options[] = {
        {"interface", required_argument, nullptr, 'i'},
        {"config",    required_argument, nullptr, 'c'},
        {"verbose",   no_argument,       nullptr, 'v'},
        {"help",      no_argument,       nullptr, 'h'},
        {nullptr,     0,                 nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:c:vh", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'i':
                interfaceName = optarg;
                break;
            case 'c':
                configDir = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }

    // Initialize logger
    Logger::init("can_service");
    if (verbose) {
        Logger::setLevel(LogLevel::DEBUG);
    }

    LOG_INFO("Speeduino CAN Service starting...");
    LOG_INFO("Interface: " + interfaceName);
    LOG_INFO("Config dir: " + configDir);

    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Load configuration
    if (!ConfigLoader::loadFromDirectory(configDir)) {
        LOG_WARN("Failed to load config, using defaults");
    }

    const auto& sysConfig = ConfigLoader::getSystemConfig();

    // Initialize CAN interface
    CanInterface canInterface;
    if (!canInterface.open(interfaceName)) {
        LOG_FATAL("Failed to open CAN interface " + interfaceName);
        return 1;
    }

    // Initialize CAN parser
    CanParser parser;
    auto signals = getSignalsForProtocol(sysConfig.can_protocol);
    parser.loadSignals(signals);

    // Initialize CAN writer
    CanWriter writer(canInterface);
    writer.loadAllowedCommands(ConfigLoader::getAllowedCommands());

    // Initialize ZMQ publisher
    ZmqPublisher publisher;
    if (!publisher.init(endpoints::ENGINE_DATA)) {
        LOG_FATAL("Failed to init ZMQ publisher");
        return 1;
    }

    // Initialize ZMQ command server
    ZmqCommandServer cmdServer;
    if (!cmdServer.init(endpoints::CAN_COMMAND)) {
        LOG_FATAL("Failed to init ZMQ command server");
        return 1;
    }

    LOG_INFO("CAN service initialized, starting main loop");

    // Calculate publish interval
    const auto publishInterval = std::chrono::microseconds(
        1000000 / sysConfig.zmq_publish_rate_hz);
    auto lastPublish = std::chrono::steady_clock::now();

    // Timeout tracking
    uint32_t canTimeoutMs = sysConfig.can_timeout_ms;
    bool wasConnected = true;

    // Main loop
    while (g_running) {
        // Receive CAN frames (non-blocking with short timeout)
        auto frame = canInterface.receive(10);
        if (frame) {
            parser.parseFrame(*frame);
        }

        // Check for incoming commands
        auto cmd = cmdServer.receiveCommand(0);
        if (cmd) {
            LOG_DEBUG("Received CAN command: ID=0x" + std::to_string(cmd->can_id));

            auto result = writer.send(cmd->can_id, cmd->data.data(), cmd->dlc);

            CanCommandResponse response;
            response.success = (result == CanWriter::SendResult::OK);
            response.error_code = static_cast<uint8_t>(result);

            switch (result) {
                case CanWriter::SendResult::OK:
                    response.error_message = "OK";
                    break;
                case CanWriter::SendResult::BLOCKED:
                    response.error_message = "CAN ID not in whitelist";
                    break;
                case CanWriter::SendResult::RATE_LIMITED:
                    response.error_message = "Rate limit exceeded";
                    break;
                case CanWriter::SendResult::CAN_ERROR:
                    response.error_message = "CAN write failed";
                    break;
                case CanWriter::SendResult::INVALID:
                    response.error_message = "Invalid parameters";
                    break;
            }

            cmdServer.sendResponse(response);
        }

        // Publish at configured rate
        auto now = std::chrono::steady_clock::now();
        if (now - lastPublish >= publishInterval) {
            lastPublish = now;

            EngineData data = parser.getEngineData();

            // Check for CAN timeout
            bool isFresh = parser.isDataFresh(canTimeoutMs);
            if (isFresh) {
                data.flags |= EngineData::FLAG_CAN_OK;
            } else {
                data.flags &= ~EngineData::FLAG_CAN_OK;

                if (wasConnected) {
                    LOG_WARN("CAN data timeout - no frames received for " +
                             std::to_string(canTimeoutMs) + "ms");
                }
            }
            wasConnected = isFresh;

            publisher.publishEngineData(data);
        }
    }

    LOG_INFO("Shutting down...");

    cmdServer.shutdown();
    publisher.shutdown();
    canInterface.close();

    LOG_INFO("CAN service stopped");
    Logger::shutdown();

    return 0;
}

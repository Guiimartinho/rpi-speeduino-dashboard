#include "common/config_loader.hpp"
#include "common/logger.hpp"
#include "common/zmq_messages.hpp"
#include "reverse_service/reverse_detector.hpp"

#include <msgpack.hpp>
#include <zmq.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <getopt.h>
#include <iostream>
#include <sstream>
#include <thread>

#ifdef __linux__
    #include <cstring>
    #include <linux/can.h>
    #include <linux/can/raw.h>
    #include <net/if.h>
    #include <poll.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════════
// Reverse Service Timing Constants
// ISO 26262: Named constants for polling and GPIO check intervals
// ═══════════════════════════════════════════════════════════════════════════════
namespace {
/// Poll timeout for CAN socket in milliseconds
constexpr int POLL_TIMEOUT_MS = 50;
/// GPIO check interval in milliseconds
constexpr int GPIO_CHECK_INTERVAL_MS = 50;
/// Idle sleep duration when no CAN socket in milliseconds
constexpr int IDLE_SLEEP_MS = 50;
}  // anonymous namespace

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

}  // anonymous namespace

int main(int argc, char* argv[]) {
    using namespace speeduino;

    // Default options
    std::string interfaceName = "can0";
    std::string configDir     = "/etc/speeduino-ui";
    bool verbose              = false;

    // Parse command line
    static struct option long_options[] = {
        {"interface", required_argument, nullptr, 'i'},
        {   "config", required_argument, nullptr, 'c'},
        {  "verbose",       no_argument, nullptr, 'v'},
        {     "help",       no_argument, nullptr, 'h'},
        {    nullptr,                 0, nullptr,   0}
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
    Logger::init("reverse_service");
    if (verbose) {
        Logger::setLevel(LogLevel::Dbg);
    }

    LOG_INFO("Speeduino Reverse Detection Service starting...");

    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Load configuration
    if (!ConfigLoader::loadFromDirectory(configDir)) {
        LOG_WARN("Failed to load config, using defaults");
    }

    const auto& reverseConfig = ConfigLoader::getReverseConfig();
    const auto& sysConfig     = ConfigLoader::getSystemConfig();

    // Initialize ZMQ publisher
    zmq::context_t zmqContext(1);
    zmq::socket_t zmqPublisher(zmqContext, zmq::socket_type::pub);

    try {
        int linger = 0;
        zmqPublisher.set(zmq::sockopt::linger, linger);
        zmqPublisher.bind(endpoints::REVERSE_TRIGGER);
        LOG_INFO("ZMQ publisher bound to " + std::string(endpoints::REVERSE_TRIGGER));
    } catch (const zmq::error_t& e) {
        LOG_FATAL("Failed to init ZMQ: " + std::string(e.what()));
        return 1;
    }

    // Initialize reverse detector
    ReverseDetector detector;
    if (!detector.init(reverseConfig)) {
        LOG_FATAL("Failed to initialize reverse detector");
        return 1;
    }

    // Set callback to publish state changes
    detector.setCallback([&zmqPublisher](bool engaged, uint8_t source) {
        ReverseEvent event;
        event.timestamp_ms =
            static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now().time_since_epoch())
                                      .count());
        event.engaged = engaged;
        event.source  = source;

        try {
            // Serialize with msgpack
            std::stringstream ss;
            msgpack::pack(ss, event);
            std::string packed = ss.str();

            // Send topic
            zmq::message_t topic(topics::REVERSE, std::strlen(topics::REVERSE));
            zmqPublisher.send(topic, zmq::send_flags::sndmore);

            // Send data
            zmq::message_t data(packed.data(), packed.size());
            zmqPublisher.send(data, zmq::send_flags::none);

            LOG_DEBUG("Published reverse event: engaged=" +
                      std::string(engaged ? "true" : "false"));
        } catch (const zmq::error_t& e) {
            LOG_ERROR("Failed to publish: " + std::string(e.what()));
        }
    });

#ifdef __linux__
    // Open CAN socket if CAN detection is enabled
    int canSocket = -1;
    if (reverseConfig.can_enabled) {
        canSocket = socket(PF_CAN, SOCK_RAW, CAN_RAW);
        if (canSocket < 0) {
            LOG_ERROR("Failed to create CAN socket");
            if (!reverseConfig.gpio_enabled) {
                LOG_FATAL("No detection source available");
                return 1;
            }
        } else {
            struct ifreq ifr;
            std::memset(&ifr, 0, sizeof(ifr));
            std::strncpy(ifr.ifr_name, interfaceName.c_str(), IFNAMSIZ - 1);

            if (ioctl(canSocket, SIOCGIFINDEX, &ifr) < 0) {
                LOG_ERROR("Failed to get CAN interface index");
                close(canSocket);
                canSocket = -1;
            } else {
                struct sockaddr_can addr;
                std::memset(&addr, 0, sizeof(addr));
                addr.can_family  = AF_CAN;
                addr.can_ifindex = ifr.ifr_ifindex;

                // Set CAN filter to only receive the reverse detection frame
                struct can_filter filter;
                filter.can_id   = reverseConfig.can_id;
                filter.can_mask = CAN_SFF_MASK;
                setsockopt(canSocket, SOL_CAN_RAW, CAN_RAW_FILTER, &filter, sizeof(filter));

                if (bind(canSocket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
                    LOG_ERROR("Failed to bind CAN socket");
                    close(canSocket);
                    canSocket = -1;
                } else {
                    LOG_INFO("CAN socket opened on " + interfaceName);
                }
            }
        }
    }
#endif

    LOG_INFO("Reverse service initialized, starting main loop");

    // GPIO check interval
    const auto gpioInterval = std::chrono::milliseconds(GPIO_CHECK_INTERVAL_MS);
    auto lastGpioCheck      = std::chrono::steady_clock::now();

    // Main loop
    while (g_running) {
#ifdef __linux__
        if (canSocket >= 0) {
            // Poll CAN socket with timeout
            struct pollfd pfd;
            pfd.fd     = canSocket;
            pfd.events = POLLIN;

            int ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
            if (ret > 0 && (pfd.revents & POLLIN)) {
                struct can_frame cf;
                ssize_t nbytes = read(canSocket, &cf, sizeof(cf));
                if (nbytes == sizeof(cf)) {
                    detector.procesCanFrame(cf.can_id & CAN_SFF_MASK, cf.data, cf.can_dlc);
                }
            }
        } else {
            // No CAN, just sleep a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_SLEEP_MS));
        }
#else
        std::this_thread::sleep_for(std::chrono::milliseconds(IDLE_SLEEP_MS));
#endif

        // Check GPIO at interval
        auto now = std::chrono::steady_clock::now();
        if (now - lastGpioCheck >= gpioInterval) {
            lastGpioCheck = now;
            detector.checkGpio();
        }
    }

    LOG_INFO("Shutting down...");

#ifdef __linux__
    if (canSocket >= 0) {
        close(canSocket);
    }
#endif

    detector.shutdown();
    zmqPublisher.close();
    zmqContext.close();

    LOG_INFO("Reverse service stopped");
    Logger::shutdown();

    return 0;
}

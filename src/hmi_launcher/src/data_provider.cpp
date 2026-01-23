#include "hmi/data_provider.hpp"

#include <msgpack.hpp>
#include <zmq.hpp>

#include <QDebug>

namespace speeduino {

// ═══════════════════════════════════════════════════════════════════════════════
// ZMQ Socket Configuration Constants
// ISO 26262: Named constants for network timing parameters
// ═══════════════════════════════════════════════════════════════════════════════
namespace {
/// ZMQ receive timeout in milliseconds (extended for automotive reliability)
constexpr int ZMQ_RECEIVE_TIMEOUT_MS = 500;
/// ZMQ reconnect interval in milliseconds
constexpr int ZMQ_RECONNECT_INTERVAL_MS = 100;
/// ZMQ max reconnect interval in milliseconds
constexpr int ZMQ_RECONNECT_INTERVAL_MAX_MS = 1000;
/// ZMQ poll timeout in milliseconds
constexpr int ZMQ_POLL_TIMEOUT_MS = 50;
/// Worker thread shutdown timeout in milliseconds
constexpr int WORKER_THREAD_WAIT_TIMEOUT_MS = 2000;
/// ZMQ socket linger time (0 = don't wait on close)
constexpr int ZMQ_LINGER_MS = 0;
}  // anonymous namespace

// ZmqWorker implementation
ZmqWorker::ZmqWorker(QObject* parent) : QObject(parent) {}

ZmqWorker::~ZmqWorker() {
    stop();
}

void ZmqWorker::stop() {
    m_running.store(false, std::memory_order_release);
}

void ZmqWorker::process() {
    try {
        m_context = std::make_unique<zmq::context_t>(1);

        // Engine data subscriber with extended timeouts for automotive reliability
        m_engineSub = std::make_unique<zmq::socket_t>(*m_context, zmq::socket_type::sub);
        m_engineSub->set(zmq::sockopt::linger, ZMQ_LINGER_MS);
        m_engineSub->set(zmq::sockopt::rcvtimeo, ZMQ_RECEIVE_TIMEOUT_MS);
        m_engineSub->set(zmq::sockopt::reconnect_ivl, ZMQ_RECONNECT_INTERVAL_MS);
        m_engineSub->set(zmq::sockopt::reconnect_ivl_max, ZMQ_RECONNECT_INTERVAL_MAX_MS);
        m_engineSub->connect(endpoints::ENGINE_DATA);
        m_engineSub->set(zmq::sockopt::subscribe, topics::ENGINE);

        // Reverse event subscriber
        m_reverseSub = std::make_unique<zmq::socket_t>(*m_context, zmq::socket_type::sub);
        m_reverseSub->set(zmq::sockopt::linger, ZMQ_LINGER_MS);
        m_reverseSub->set(zmq::sockopt::rcvtimeo, ZMQ_RECEIVE_TIMEOUT_MS);
        m_reverseSub->set(zmq::sockopt::reconnect_ivl, ZMQ_RECONNECT_INTERVAL_MS);
        m_reverseSub->set(zmq::sockopt::reconnect_ivl_max, ZMQ_RECONNECT_INTERVAL_MAX_MS);
        m_reverseSub->connect(endpoints::REVERSE_TRIGGER);
        m_reverseSub->set(zmq::sockopt::subscribe, topics::REVERSE);

        qInfo() << "[DataProvider] ZMQ sockets connected successfully";
        emit connectionStatusChanged(true);

        // Use explicit memory ordering for thread-safe running check
        while (m_running.load(std::memory_order_acquire)) {
            // Poll both sockets
            zmq::pollitem_t items[] = {
                { *m_engineSub, 0, ZMQ_POLLIN, 0},
                {*m_reverseSub, 0, ZMQ_POLLIN, 0}
            };

            zmq::poll(items, 2, std::chrono::milliseconds(ZMQ_POLL_TIMEOUT_MS));

            // Engine data
            if (items[0].revents & ZMQ_POLLIN) {
                zmq::message_t topic, data;
                if (m_engineSub->recv(topic, zmq::recv_flags::none) &&
                    m_engineSub->recv(data, zmq::recv_flags::none)) {
                    try {
                        auto oh =
                            msgpack::unpack(static_cast<const char*>(data.data()), data.size());
                        EngineData engineData;
                        oh.get().convert(engineData);
                        emit engineDataReceived(engineData);
                    } catch (const std::exception& e) {
                        qWarning() << "Failed to parse engine data:" << e.what();
                    }
                }
            }

            // Reverse event
            if (items[1].revents & ZMQ_POLLIN) {
                zmq::message_t topic, data;
                if (m_reverseSub->recv(topic, zmq::recv_flags::none) &&
                    m_reverseSub->recv(data, zmq::recv_flags::none)) {
                    try {
                        auto oh =
                            msgpack::unpack(static_cast<const char*>(data.data()), data.size());
                        ReverseEvent event;
                        oh.get().convert(event);
                        emit reverseEventReceived(event);
                    } catch (const std::exception& e) {
                        qWarning() << "Failed to parse reverse event:" << e.what();
                    }
                }
            }
        }

        emit connectionStatusChanged(false);

    } catch (const zmq::error_t& e) {
        qCritical() << "ZMQ error:" << e.what();
        emit connectionStatusChanged(false);
    }

    m_engineSub.reset();
    m_reverseSub.reset();
    m_context.reset();
}

// DataProvider implementation
DataProvider::DataProvider(QObject* parent) : QObject(parent) {
    // Register metatypes for cross-thread signals
    qRegisterMetaType<EngineData>("EngineData");
    qRegisterMetaType<ReverseEvent>("ReverseEvent");
    qRegisterMetaType<SteeringEvent>("SteeringEvent");
}

DataProvider::~DataProvider() {
    stop();
}

// ═══════════════════════════════════════════════════════════════
// THREAD-SAFE PROPERTY GETTERS
// ═══════════════════════════════════════════════════════════════

int DataProvider::rpm() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.rpm;
}

int DataProvider::coolantTemp() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.coolant_temp;
}

int DataProvider::intakeTemp() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.intake_temp;
}

int DataProvider::tps() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.tps;
}

double DataProvider::mapKpa() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.map_kpa / 10.0;
}

double DataProvider::lambda() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.lambda / 1000.0;
}

double DataProvider::ignitionAdvance() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.ignition_advance / 10.0;
}

int DataProvider::injectorDuty() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.injector_duty;
}

int DataProvider::gear() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.gear;
}

double DataProvider::vehicleSpeed() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.vehicle_speed / 10.0;
}

int DataProvider::fuelPressure() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.fuel_pressure;
}

int DataProvider::oilPressure() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.oil_pressure;
}

int DataProvider::oilTemp() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.oil_temp;
}

double DataProvider::batteryVoltage() const {
    QMutexLocker locker(&m_dataMutex);
    // battery_voltage is in mV, convert to V
    return m_data.battery_voltage / 1000.0;
}

double DataProvider::boostTarget() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.boost_target / 10.0;
}

double DataProvider::fuelConsumption() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.fuel_consumption / 100.0;
}

int DataProvider::ve() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.ve;
}

double DataProvider::afrTarget() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.afr_target / 10.0;
}

// ═══════════════════════════════════════════════════════════════
// PER-CYLINDER TRIM GETTERS
// ═══════════════════════════════════════════════════════════════

int DataProvider::fuelTrimCyl1() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.fuel_trim_cyl1;
}

int DataProvider::fuelTrimCyl2() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.fuel_trim_cyl2;
}

int DataProvider::fuelTrimCyl3() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.fuel_trim_cyl3;
}

int DataProvider::fuelTrimCyl4() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.fuel_trim_cyl4;
}

double DataProvider::ignTrimCyl1() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.ign_trim_cyl1;
}

double DataProvider::ignTrimCyl2() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.ign_trim_cyl2;
}

double DataProvider::ignTrimCyl3() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.ign_trim_cyl3;
}

double DataProvider::ignTrimCyl4() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.ign_trim_cyl4;
}

// ═══════════════════════════════════════════════════════════════
// IDLE CONTROL GETTERS
// ═══════════════════════════════════════════════════════════════

int DataProvider::idleTargetRpm() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.idle_target_rpm * 10;  // Stored as / 10, expand back
}

int DataProvider::idleValveDuty() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.idle_valve_duty;
}

// ═══════════════════════════════════════════════════════════════
// DIAGNOSTIC GETTERS
// ═══════════════════════════════════════════════════════════════

int DataProvider::errorCount() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.error_count;
}

bool DataProvider::synced() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.sync_status > 0;
}

// ═══════════════════════════════════════════════════════════════
// STATUS FLAG GETTERS
// ═══════════════════════════════════════════════════════════════

bool DataProvider::celOn() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isCelOn();
}

bool DataProvider::overheat() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isOverheat();
}

bool DataProvider::canConnected() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isCanOk();
}

bool DataProvider::engineRunning() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isEngineRunning();
}

bool DataProvider::revLimiterActive() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isRevLimiterActive();
}

bool DataProvider::launchControlActive() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isLaunching();
}

bool DataProvider::flatShiftActive() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isFlatShifting();
}

bool DataProvider::clutchIn() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isClutchIn();
}

bool DataProvider::brakeOn() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isBrakeOn();
}

bool DataProvider::cruiseOn() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isCruiseOn();
}

bool DataProvider::lowOilPressure() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isLowOilPressure();
}

bool DataProvider::lowFuelPressure() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isLowFuelPressure();
}

bool DataProvider::dfcoActive() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isDfcoActive();
}

bool DataProvider::fanOn() const {
    QMutexLocker locker(&m_dataMutex);
    return m_data.isFanOn();
}

bool DataProvider::reverseEngaged() const {
    QMutexLocker locker(&m_dataMutex);
    return m_reverseEngaged;
}

// ═══════════════════════════════════════════════════════════════
// LIFECYCLE MANAGEMENT
// ═══════════════════════════════════════════════════════════════

void DataProvider::start() {
    if (m_worker) {
        qWarning() << "[DataProvider] Already running";
        return;
    }

    m_worker             = std::make_unique<ZmqWorker>();
    ZmqWorker* workerPtr = m_worker.get();  // Raw pointer for Qt connections

    workerPtr->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::started, workerPtr, &ZmqWorker::process);
    // Note: Don't use deleteLater with unique_ptr - we manage lifetime ourselves

    connect(workerPtr, &ZmqWorker::engineDataReceived, this, &DataProvider::onEngineDataReceived,
            Qt::QueuedConnection);
    connect(workerPtr, &ZmqWorker::reverseEventReceived, this,
            &DataProvider::onReverseEventReceived, Qt::QueuedConnection);
    connect(workerPtr, &ZmqWorker::steeringEventReceived, this,
            &DataProvider::onSteeringEventReceived, Qt::QueuedConnection);

    m_workerThread.start();
    qInfo() << "[DataProvider] Started";
}

void DataProvider::stop() {
    if (!m_worker) {
        return;
    }

    m_worker->stop();
    m_workerThread.quit();

    // Wait with timeout for clean shutdown
    if (!m_workerThread.wait(WORKER_THREAD_WAIT_TIMEOUT_MS)) {
        qWarning() << "[DataProvider] Worker thread did not stop in time";
        m_workerThread.terminate();
        m_workerThread.wait();
    }

    // Reset worker after thread is stopped (safe deletion)
    m_worker.reset();
    qInfo() << "[DataProvider] Stopped";
}

// ═══════════════════════════════════════════════════════════════
// DATA RECEPTION SLOTS
// ═══════════════════════════════════════════════════════════════

void DataProvider::onEngineDataReceived(const EngineData& data) {
    {
        QMutexLocker locker(&m_dataMutex);
        m_data = data;
    }
    emit dataChanged();
}

void DataProvider::onReverseEventReceived(const ReverseEvent& event) {
    bool changed = false;
    {
        QMutexLocker locker(&m_dataMutex);
        if (m_reverseEngaged != event.engaged) {
            m_reverseEngaged = event.engaged;
            changed          = true;
        }
    }
    if (changed) {
        emit reverseChanged();
    }
}

void DataProvider::onSteeringEventReceived(const SteeringEvent& event) {
    if (event.pressed) {
        emit steeringButtonPressed(event.button_id);
    }
}

}  // namespace speeduino

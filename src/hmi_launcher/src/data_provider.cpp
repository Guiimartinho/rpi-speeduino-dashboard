#include "hmi/data_provider.hpp"
#include <zmq.hpp>
#include <msgpack.hpp>
#include <QDebug>

namespace speeduino {

// ZmqWorker implementation
ZmqWorker::ZmqWorker(QObject* parent)
    : QObject(parent)
{
}

ZmqWorker::~ZmqWorker() {
    stop();
}

void ZmqWorker::stop() {
    m_running = false;
}

void ZmqWorker::process() {
    try {
        m_context = std::make_unique<zmq::context_t>(1);

        // Engine data subscriber
        m_engineSub = std::make_unique<zmq::socket_t>(*m_context, zmq::socket_type::sub);
        m_engineSub->set(zmq::sockopt::linger, 0);
        m_engineSub->set(zmq::sockopt::rcvtimeo, 100);
        m_engineSub->connect(endpoints::ENGINE_DATA);
        m_engineSub->set(zmq::sockopt::subscribe, topics::ENGINE);

        // Reverse event subscriber
        m_reverseSub = std::make_unique<zmq::socket_t>(*m_context, zmq::socket_type::sub);
        m_reverseSub->set(zmq::sockopt::linger, 0);
        m_reverseSub->set(zmq::sockopt::rcvtimeo, 100);
        m_reverseSub->connect(endpoints::REVERSE_TRIGGER);
        m_reverseSub->set(zmq::sockopt::subscribe, topics::REVERSE);

        emit connectionStatusChanged(true);

        while (m_running) {
            // Poll both sockets
            zmq::pollitem_t items[] = {
                {*m_engineSub, 0, ZMQ_POLLIN, 0},
                {*m_reverseSub, 0, ZMQ_POLLIN, 0}
            };

            zmq::poll(items, 2, std::chrono::milliseconds(50));

            // Engine data
            if (items[0].revents & ZMQ_POLLIN) {
                zmq::message_t topic, data;
                if (m_engineSub->recv(topic, zmq::recv_flags::none) &&
                    m_engineSub->recv(data, zmq::recv_flags::none)) {

                    try {
                        auto oh = msgpack::unpack(
                            static_cast<const char*>(data.data()), data.size());
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
                        auto oh = msgpack::unpack(
                            static_cast<const char*>(data.data()), data.size());
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
DataProvider::DataProvider(QObject* parent)
    : QObject(parent)
{
    // Register metatypes for cross-thread signals
    qRegisterMetaType<EngineData>("EngineData");
    qRegisterMetaType<ReverseEvent>("ReverseEvent");
    qRegisterMetaType<SteeringEvent>("SteeringEvent");
}

DataProvider::~DataProvider() {
    stop();
}

void DataProvider::start() {
    if (m_worker) {
        return; // Already running
    }

    m_worker = new ZmqWorker();
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::started, m_worker, &ZmqWorker::process);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &ZmqWorker::engineDataReceived,
            this, &DataProvider::onEngineDataReceived, Qt::QueuedConnection);
    connect(m_worker, &ZmqWorker::reverseEventReceived,
            this, &DataProvider::onReverseEventReceived, Qt::QueuedConnection);
    connect(m_worker, &ZmqWorker::steeringEventReceived,
            this, &DataProvider::onSteeringEventReceived, Qt::QueuedConnection);

    m_workerThread.start();
    qInfo() << "DataProvider started";
}

void DataProvider::stop() {
    if (!m_worker) {
        return;
    }

    m_worker->stop();
    m_workerThread.quit();
    m_workerThread.wait(1000);

    m_worker = nullptr;
    qInfo() << "DataProvider stopped";
}

void DataProvider::onEngineDataReceived(const EngineData& data) {
    m_data = data;
    emit dataChanged();
}

void DataProvider::onReverseEventReceived(const ReverseEvent& event) {
    if (m_reverseEngaged != event.engaged) {
        m_reverseEngaged = event.engaged;
        emit reverseChanged();
    }
}

void DataProvider::onSteeringEventReceived(const SteeringEvent& event) {
    if (event.pressed) {
        emit steeringButtonPressed(event.button_id);
    }
}

} // namespace speeduino

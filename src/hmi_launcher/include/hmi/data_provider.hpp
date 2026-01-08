#ifndef HMI_DATA_PROVIDER_HPP
#define HMI_DATA_PROVIDER_HPP

#include "common/zmq_messages.hpp"
#include <QObject>
#include <QThread>
#include <QString>
#include <memory>
#include <atomic>

namespace zmq {
    class context_t;
    class socket_t;
}

namespace speeduino {

// Worker thread for ZMQ reception
class ZmqWorker : public QObject {
    Q_OBJECT

public:
    explicit ZmqWorker(QObject* parent = nullptr);
    ~ZmqWorker();

    void stop();

public slots:
    void process();

signals:
    void engineDataReceived(const EngineData& data);
    void reverseEventReceived(const ReverseEvent& event);
    void steeringEventReceived(const SteeringEvent& event);
    void connectionStatusChanged(bool connected);

private:
    std::unique_ptr<zmq::context_t> m_context;
    std::unique_ptr<zmq::socket_t> m_engineSub;
    std::unique_ptr<zmq::socket_t> m_reverseSub;
    std::atomic<bool> m_running{true};
};

// QML-exposed data provider
class DataProvider : public QObject {
    Q_OBJECT

    // Engine data properties
    Q_PROPERTY(int rpm READ rpm NOTIFY dataChanged)
    Q_PROPERTY(int coolantTemp READ coolantTemp NOTIFY dataChanged)
    Q_PROPERTY(int intakeTemp READ intakeTemp NOTIFY dataChanged)
    Q_PROPERTY(int tps READ tps NOTIFY dataChanged)
    Q_PROPERTY(double mapKpa READ mapKpa NOTIFY dataChanged)
    Q_PROPERTY(double lambda READ lambda NOTIFY dataChanged)
    Q_PROPERTY(double ignitionAdvance READ ignitionAdvance NOTIFY dataChanged)
    Q_PROPERTY(int injectorDuty READ injectorDuty NOTIFY dataChanged)
    Q_PROPERTY(int gear READ gear NOTIFY dataChanged)
    Q_PROPERTY(double vehicleSpeed READ vehicleSpeed NOTIFY dataChanged)
    Q_PROPERTY(int fuelPressure READ fuelPressure NOTIFY dataChanged)
    Q_PROPERTY(int oilPressure READ oilPressure NOTIFY dataChanged)
    Q_PROPERTY(int oilTemp READ oilTemp NOTIFY dataChanged)

    // Status properties
    Q_PROPERTY(bool celOn READ celOn NOTIFY dataChanged)
    Q_PROPERTY(bool overheat READ overheat NOTIFY dataChanged)
    Q_PROPERTY(bool canConnected READ canConnected NOTIFY dataChanged)
    Q_PROPERTY(bool engineRunning READ engineRunning NOTIFY dataChanged)

    // Reverse state
    Q_PROPERTY(bool reverseEngaged READ reverseEngaged NOTIFY reverseChanged)

public:
    explicit DataProvider(QObject* parent = nullptr);
    ~DataProvider();

    // Property getters
    int rpm() const { return m_data.rpm; }
    int coolantTemp() const { return m_data.coolant_temp; }
    int intakeTemp() const { return m_data.intake_temp; }
    int tps() const { return m_data.tps; }
    double mapKpa() const { return m_data.map_kpa / 10.0; }
    double lambda() const { return m_data.lambda / 1000.0; }
    double ignitionAdvance() const { return m_data.ignition_advance / 10.0; }
    int injectorDuty() const { return m_data.injector_duty; }
    int gear() const { return m_data.gear; }
    double vehicleSpeed() const { return m_data.vehicle_speed / 10.0; }
    int fuelPressure() const { return m_data.fuel_pressure; }
    int oilPressure() const { return m_data.oil_pressure; }
    int oilTemp() const { return m_data.oil_temp; }

    bool celOn() const { return m_data.isCelOn(); }
    bool overheat() const { return m_data.isOverheat(); }
    bool canConnected() const { return m_data.isCanOk(); }
    bool engineRunning() const { return m_data.isEngineRunning(); }

    bool reverseEngaged() const { return m_reverseEngaged; }

    // Start/stop data reception
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

signals:
    void dataChanged();
    void reverseChanged();
    void steeringButtonPressed(int buttonId);

private slots:
    void onEngineDataReceived(const EngineData& data);
    void onReverseEventReceived(const ReverseEvent& event);
    void onSteeringEventReceived(const SteeringEvent& event);

private:
    EngineData m_data;
    bool m_reverseEngaged{false};

    QThread m_workerThread;
    ZmqWorker* m_worker{nullptr};
};

} // namespace speeduino

// Register metatypes for signal/slot
Q_DECLARE_METATYPE(speeduino::EngineData)
Q_DECLARE_METATYPE(speeduino::ReverseEvent)
Q_DECLARE_METATYPE(speeduino::SteeringEvent)

#endif // HMI_DATA_PROVIDER_HPP

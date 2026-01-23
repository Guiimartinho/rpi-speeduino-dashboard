#ifndef HMI_DATA_PROVIDER_HPP
#define HMI_DATA_PROVIDER_HPP

#include "common/zmq_messages.hpp"

#include <QThread>

#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

namespace zmq {
class context_t;
class socket_t;
}  // namespace zmq

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
    Q_PROPERTY(double batteryVoltage READ batteryVoltage NOTIFY dataChanged)
    Q_PROPERTY(double boostTarget READ boostTarget NOTIFY dataChanged)
    Q_PROPERTY(double fuelConsumption READ fuelConsumption NOTIFY dataChanged)
    Q_PROPERTY(int ve READ ve NOTIFY dataChanged)
    Q_PROPERTY(double afrTarget READ afrTarget NOTIFY dataChanged)

    // Per-cylinder trims
    Q_PROPERTY(int fuelTrimCyl1 READ fuelTrimCyl1 NOTIFY dataChanged)
    Q_PROPERTY(int fuelTrimCyl2 READ fuelTrimCyl2 NOTIFY dataChanged)
    Q_PROPERTY(int fuelTrimCyl3 READ fuelTrimCyl3 NOTIFY dataChanged)
    Q_PROPERTY(int fuelTrimCyl4 READ fuelTrimCyl4 NOTIFY dataChanged)
    Q_PROPERTY(double ignTrimCyl1 READ ignTrimCyl1 NOTIFY dataChanged)
    Q_PROPERTY(double ignTrimCyl2 READ ignTrimCyl2 NOTIFY dataChanged)
    Q_PROPERTY(double ignTrimCyl3 READ ignTrimCyl3 NOTIFY dataChanged)
    Q_PROPERTY(double ignTrimCyl4 READ ignTrimCyl4 NOTIFY dataChanged)

    // Idle control
    Q_PROPERTY(int idleTargetRpm READ idleTargetRpm NOTIFY dataChanged)
    Q_PROPERTY(int idleValveDuty READ idleValveDuty NOTIFY dataChanged)

    // Diagnostic data
    Q_PROPERTY(int errorCount READ errorCount NOTIFY dataChanged)
    Q_PROPERTY(bool synced READ synced NOTIFY dataChanged)

    // Status properties
    Q_PROPERTY(bool celOn READ celOn NOTIFY dataChanged)
    Q_PROPERTY(bool overheat READ overheat NOTIFY dataChanged)
    Q_PROPERTY(bool canConnected READ canConnected NOTIFY dataChanged)
    Q_PROPERTY(bool engineRunning READ engineRunning NOTIFY dataChanged)
    Q_PROPERTY(bool revLimiterActive READ revLimiterActive NOTIFY dataChanged)
    Q_PROPERTY(bool launchControlActive READ launchControlActive NOTIFY dataChanged)
    Q_PROPERTY(bool flatShiftActive READ flatShiftActive NOTIFY dataChanged)
    Q_PROPERTY(bool clutchIn READ clutchIn NOTIFY dataChanged)
    Q_PROPERTY(bool brakeOn READ brakeOn NOTIFY dataChanged)
    Q_PROPERTY(bool cruiseOn READ cruiseOn NOTIFY dataChanged)
    Q_PROPERTY(bool lowOilPressure READ lowOilPressure NOTIFY dataChanged)
    Q_PROPERTY(bool lowFuelPressure READ lowFuelPressure NOTIFY dataChanged)
    Q_PROPERTY(bool dfcoActive READ dfcoActive NOTIFY dataChanged)
    Q_PROPERTY(bool fanOn READ fanOn NOTIFY dataChanged)

    // Reverse state
    Q_PROPERTY(bool reverseEngaged READ reverseEngaged NOTIFY reverseChanged)

public:
    explicit DataProvider(QObject* parent = nullptr);
    ~DataProvider();

    // Thread-safe property getters (data may be updated from worker thread)
    int rpm() const;
    int coolantTemp() const;
    int intakeTemp() const;
    int tps() const;
    double mapKpa() const;
    double lambda() const;
    double ignitionAdvance() const;
    int injectorDuty() const;
    int gear() const;
    double vehicleSpeed() const;
    int fuelPressure() const;
    int oilPressure() const;
    int oilTemp() const;
    double batteryVoltage() const;
    double boostTarget() const;
    double fuelConsumption() const;
    int ve() const;
    double afrTarget() const;

    // Per-cylinder trims
    int fuelTrimCyl1() const;
    int fuelTrimCyl2() const;
    int fuelTrimCyl3() const;
    int fuelTrimCyl4() const;
    double ignTrimCyl1() const;
    double ignTrimCyl2() const;
    double ignTrimCyl3() const;
    double ignTrimCyl4() const;

    // Idle control
    int idleTargetRpm() const;
    int idleValveDuty() const;

    // Diagnostic data
    int errorCount() const;
    bool synced() const;

    // Status flags
    bool celOn() const;
    bool overheat() const;
    bool canConnected() const;
    bool engineRunning() const;
    bool revLimiterActive() const;
    bool launchControlActive() const;
    bool flatShiftActive() const;
    bool clutchIn() const;
    bool brakeOn() const;
    bool cruiseOn() const;
    bool lowOilPressure() const;
    bool lowFuelPressure() const;
    bool dfcoActive() const;
    bool fanOn() const;

    bool reverseEngaged() const;

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
    // Thread synchronization - protects m_data and m_reverseEngaged
    mutable QMutex m_dataMutex;

    // Shared state (protected by m_dataMutex)
    EngineData m_data;
    bool m_reverseEngaged{false};

    // Worker thread management
    QThread m_workerThread;
    std::unique_ptr<ZmqWorker> m_worker;
};

}  // namespace speeduino

// Register metatypes for signal/slot
Q_DECLARE_METATYPE(speeduino::EngineData)
Q_DECLARE_METATYPE(speeduino::ReverseEvent)
Q_DECLARE_METATYPE(speeduino::SteeringEvent)

#endif  // HMI_DATA_PROVIDER_HPP

#pragma once

#include <QObject>
#include <QTimer>
#include <QString>

namespace hmi {

/**
 * SystemMonitor - Fornece dados do sistema para o QML
 *
 * Coleta:
 * - Temperatura da CPU (Raspberry Pi)
 * - Uso de CPU (%)
 * - Uso de memória (%)
 * - Uptime do sistema
 * - Espaço livre em disco
 */
class SystemMonitor : public QObject {
    Q_OBJECT

    // Propriedades expostas ao QML
    Q_PROPERTY(float cpuTemp READ cpuTemp NOTIFY dataChanged)
    Q_PROPERTY(float cpuUsage READ cpuUsage NOTIFY dataChanged)
    Q_PROPERTY(float memUsage READ memUsage NOTIFY dataChanged)
    Q_PROPERTY(QString uptime READ uptime NOTIFY dataChanged)
    Q_PROPERTY(float diskUsage READ diskUsage NOTIFY dataChanged)
    Q_PROPERTY(QString hostname READ hostname CONSTANT)
    Q_PROPERTY(QString kernelVersion READ kernelVersion CONSTANT)

public:
    explicit SystemMonitor(QObject* parent = nullptr);
    ~SystemMonitor() override = default;

    // Getters
    float cpuTemp() const { return m_cpuTemp; }
    float cpuUsage() const { return m_cpuUsage; }
    float memUsage() const { return m_memUsage; }
    QString uptime() const { return m_uptime; }
    float diskUsage() const { return m_diskUsage; }
    QString hostname() const { return m_hostname; }
    QString kernelVersion() const { return m_kernelVersion; }

signals:
    void dataChanged();

private slots:
    void updateData();

private:
    // Funções de coleta
    float readCpuTemperature();
    float readCpuUsage();
    float readMemoryUsage();
    QString readUptime();
    float readDiskUsage();
    QString readHostname();
    QString readKernelVersion();

    // Dados
    float m_cpuTemp = 0.0f;
    float m_cpuUsage = 0.0f;
    float m_memUsage = 0.0f;
    QString m_uptime = "0:00:00";
    float m_diskUsage = 0.0f;
    QString m_hostname;
    QString m_kernelVersion;

    // Para cálculo de CPU usage
    unsigned long long m_prevCpuTotal = 0;
    unsigned long long m_prevCpuIdle = 0;

    // Timer para atualização periódica
    QTimer m_updateTimer;
};

} // namespace hmi

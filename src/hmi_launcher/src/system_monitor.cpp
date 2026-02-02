#include "hmi/system_monitor.hpp"

#include <QFile>
#include <QTextStream>
#include <QStorageInfo>
#include <QHostInfo>

#include <fstream>
#include <sstream>
#include <sys/utsname.h>

namespace hmi {

SystemMonitor::SystemMonitor(QObject* parent)
    : QObject(parent)
{
    // Lê valores constantes uma vez
    m_hostname = readHostname();
    m_kernelVersion = readKernelVersion();

    // Faz primeira leitura
    updateData();

    // Configura timer para atualizar a cada 2 segundos
    connect(&m_updateTimer, &QTimer::timeout, this, &SystemMonitor::updateData);
    m_updateTimer.start(2000);

    qDebug() << "[SystemMonitor] Started - hostname:" << m_hostname
             << "kernel:" << m_kernelVersion;
}

void SystemMonitor::updateData()
{
    m_cpuTemp = readCpuTemperature();
    m_cpuUsage = readCpuUsage();
    m_memUsage = readMemoryUsage();
    m_uptime = readUptime();
    m_diskUsage = readDiskUsage();

    emit dataChanged();
}

float SystemMonitor::readCpuTemperature()
{
#ifdef __linux__
    // Raspberry Pi thermal zone
    std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
    if (file.is_open()) {
        int temp;
        file >> temp;
        if (file.good()) {
            return static_cast<float>(temp) / 1000.0f;  // miligraus para graus
        }
    }
#endif
    return 0.0f;
}

float SystemMonitor::readCpuUsage()
{
#ifdef __linux__
    std::ifstream file("/proc/stat");
    if (!file.is_open()) return 0.0f;

    std::string cpu;
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;

    file >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq >> steal;

    if (cpu != "cpu") return 0.0f;

    unsigned long long total = user + nice + system + idle + iowait + irq + softirq + steal;
    unsigned long long idleTime = idle + iowait;

    // Calcula diferença desde última leitura
    unsigned long long totalDiff = total - m_prevCpuTotal;
    unsigned long long idleDiff = idleTime - m_prevCpuIdle;

    m_prevCpuTotal = total;
    m_prevCpuIdle = idleTime;

    if (totalDiff == 0) return 0.0f;

    float usage = 100.0f * (1.0f - static_cast<float>(idleDiff) / static_cast<float>(totalDiff));
    return std::max(0.0f, std::min(100.0f, usage));
#else
    return 0.0f;
#endif
}

float SystemMonitor::readMemoryUsage()
{
#ifdef __linux__
    std::ifstream file("/proc/meminfo");
    if (!file.is_open()) return 0.0f;

    std::string line;
    unsigned long memTotal = 0, memAvailable = 0;

    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        unsigned long value;
        std::string unit;

        iss >> key >> value >> unit;

        if (key == "MemTotal:") {
            memTotal = value;
        } else if (key == "MemAvailable:") {
            memAvailable = value;
        }

        if (memTotal > 0 && memAvailable > 0) break;
    }

    if (memTotal == 0) return 0.0f;

    float usage = 100.0f * (1.0f - static_cast<float>(memAvailable) / static_cast<float>(memTotal));
    return std::max(0.0f, std::min(100.0f, usage));
#else
    return 0.0f;
#endif
}

QString SystemMonitor::readUptime()
{
#ifdef __linux__
    std::ifstream file("/proc/uptime");
    if (!file.is_open()) return "0:00:00";

    float uptimeSeconds;
    file >> uptimeSeconds;

    int totalSeconds = static_cast<int>(uptimeSeconds);
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;

    return QString("%1:%2:%3")
        .arg(hours)
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
#else
    return "0:00:00";
#endif
}

float SystemMonitor::readDiskUsage()
{
    QStorageInfo storage = QStorageInfo::root();
    if (storage.isValid()) {
        qint64 total = storage.bytesTotal();
        qint64 free = storage.bytesFree();
        if (total > 0) {
            return 100.0f * (1.0f - static_cast<float>(free) / static_cast<float>(total));
        }
    }
    return 0.0f;
}

QString SystemMonitor::readHostname()
{
    return QHostInfo::localHostName();
}

QString SystemMonitor::readKernelVersion()
{
#ifdef __linux__
    struct utsname info;
    if (uname(&info) == 0) {
        return QString(info.release);
    }
#endif
    return "unknown";
}

} // namespace hmi

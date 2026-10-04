#include "Logger.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 4. Sistem Entegratörü (AI & Frontend)
 * ============================================================================
 */

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() {
    m_logFile.open("system_log.txt", std::ios::out | std::ios::app);
    log("SYSTEM", "=== ADAS Karar Motoru Başlatıldı ===");
}

Logger::~Logger() {
    if (m_logFile.is_open()) {
        log("SYSTEM", "=== ADAS Karar Motoru Kapatıldı ===");
        m_logFile.close();
    }
}

void Logger::log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Zaman damgası üretimi
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");

    std::string formattedLog = "[" + ss.str() + "] [" + level + "] " + message;

    std::cout << formattedLog << std::endl;

    if (m_logFile.is_open()) {
        m_logFile << formattedLog << std::endl;
        m_logFile.flush();
    }

    m_memoryLogs.push_back(formattedLog);
    if (m_memoryLogs.size() > 50) {
        m_memoryLogs.erase(m_memoryLogs.begin());
    }
}

std::vector<std::string> Logger::getRecentLogs(size_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_memoryLogs.size() <= count) return m_memoryLogs;
    return std::vector<std::string>(m_memoryLogs.end() - count, m_memoryLogs.end());
}

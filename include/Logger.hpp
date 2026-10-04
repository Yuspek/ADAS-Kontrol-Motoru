#ifndef LOGGER_HPP
#define LOGGER_HPP

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 4. Sistem Entegratörü (AI & Frontend)
 * ============================================================================
 * Sorumluluklar:
 * - C++ tarafında std::ofstream ile kararları zaman damgalı yazan Logger'ı yönetmek.
 * - HTML/JS/CSS Web Dashboard katmanını C++ sunucusuna entegre etmek.
 * - main.cpp dosyasında simülasyon döngüsünü bağlamak.
 */

#include <string>
#include <fstream>
#include <mutex>
#include <vector>

class Logger {
public:
    static Logger& getInstance();

    void log(const std::string& level, const std::string& message);
    std::vector<std::string> getRecentLogs(size_t count = 15);

private:
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::ofstream m_logFile;
    std::mutex m_mutex;
    std::vector<std::string> m_memoryLogs;
};

#endif // LOGGER_HPP

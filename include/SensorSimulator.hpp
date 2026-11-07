#ifndef SENSOR_SIMULATOR_HPP
#define SENSOR_SIMULATOR_HPP

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 1. Simülasyon Geliştirici (Data Generator)
 * ============================================================================
 * Sorumluluklar:
 * - Zamanlayıcı döngüsünü kurmak.
 * - SensorSimulator sınıfını doldurmak.
 * - W tuşuna (Gaz) göre ivmelenme / sürtünme fizik kurallarını kodlamak.
 * - Yol anomalilerini (çukur/tümsek) ve hız tabelalarını rastgele ürettirmek.
 */

#include <string>
#include <mutex>
#include <random>

struct TelemetryData {
    double speed_kmh = 0.0;             // Aracın anlık hızı (km/s)
    int speed_limit = 70;               // Okunan hız sınırı tabelası (50, 70, 90 km/s)
    double radar_distance = 50.0;       // Öndeki araca/engele olan radar mesafesi (m)
    
    std::string obstacle_type = "Yok"; // "Yok", "Cukur", "Tumsek"
    double obstacle_distance = 100.0;   // Engelin mesafesi (m)
    double lane_offset = 0.0;           // Şerit sapma miktarı (m)
    
    bool is_gas_pressed = false;        // W tuşu basılı mı?
    bool is_brake_pressed = false;      // Fren basılı mı?
    bool left_signal = false;           // Sol sinyal
    bool right_signal = false;          // Sağ sinyal
};

class SensorSimulator {
public:
    SensorSimulator();

    // Simülasyon fizik adımı (dt saniye cinsinden zaman adımı)
    void update(double dt_seconds);

    // Veri erişim metodları (Thread-safe)
    TelemetryData getData();
    void setGasPressed(bool pressed);
    void setBrakePressed(bool pressed);
    void setLeftSignal(bool active);
    void setRightSignal(bool active);
    
    // Manuel Test Tetikleyicileri
    void spawnObstacle(const std::string& type, double distance = 40.0);
    void setSpeedLimit(int limit);
    void resetState();

private:
    TelemetryData m_data;
    std::mutex m_mutex;

    // Rastgele olay üretimi ve zamanlayıcı değişkenleri
    std::mt19937 m_rng;
    double m_obstacleTimer = 0.0;
    double m_nextObstacleInterval = 10.0;
    double m_speedLimitTimer = 0.0;
    double m_nextSpeedLimitInterval = 15.0;
    double m_laneDriftTimer = 0.0;
    double m_targetLaneDrift = 0.0;
};

#endif // SENSOR_SIMULATOR_HPP

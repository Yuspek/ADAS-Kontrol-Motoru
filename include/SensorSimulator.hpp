#ifndef SENSOR_SIMULATOR_HPP
#define SENSOR_SIMULATOR_HPP

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 1. Simülasyon Geliştirici (Data Generator)
 * ============================================================================
 * Sorumluluklar:
 * - Zamanlayıcı döngüsünü kurmak.
 * - SensorSimulator sınıfını (IVehiclePlatform uygulayıcısı olarak) doldurmak.
 * - W tuşuna (Gaz) ve ACC komutlarına göre ivmelenme / sürtünme fizik kurallarını kodlamak.
 * - Yol anomalilerini (çukur/tümsek) ve hız tabelalarını rastgele ürettirmek.
 */

#include "VehicleInterfaces.hpp"
#include <string>
#include <mutex>
#include <random>

class SensorSimulator : public IVehiclePlatform {
public:
    SensorSimulator();
    ~SensorSimulator() override = default;

    // Simülasyon fizik adımı (dt saniye cinsinden zaman adımı)
    void update(double dt_seconds) override;

    // Veri erişim ve aktüatör geri bildirim metodları (Thread-safe, HAL uyumlu)
    TelemetryData getData() override;
    void applyActuatorCommand(const ActuatorCommand& command) override;

    // Sürücü Girdileri
    void setGasPressed(bool pressed);
    void setBrakePressed(bool pressed);
    void setLeftSignal(bool active);
    void setRightSignal(bool active);

    // ACC Kontrolleri
    void setAccEnabled(bool enabled);
    void setAccSetSpeed(double speed);

    // Manuel Test Tetikleyicileri
    void spawnObstacle(const std::string& type, double distance = 40.0);
    void setSpeedLimit(int limit);
    void resetState();

private:
    TelemetryData m_data;
    std::mutex m_mutex;

    // Aktüatör talepleri (ACC otonom hızlanma/frenleme için)
    double m_accThrottleDemand = 0.0;
    double m_accBrakeDemand = 0.0;

    // Rastgele olay üretimi ve zamanlayıcı değişkenleri
    std::mt19937 m_rng;
    double m_leadCarSpeed = 35.0;
    double m_obstacleTimer = 0.0;
    double m_nextObstacleInterval = 10.0;
    double m_speedLimitTimer = 0.0;
    double m_nextSpeedLimitInterval = 15.0;
    double m_laneDriftTimer = 0.0;
    double m_targetLaneDrift = 0.0;
};

#endif // SENSOR_SIMULATOR_HPP

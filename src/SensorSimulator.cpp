#include "SensorSimulator.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <cmath>

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 1. Simülasyon Geliştirici (Data Generator)
 * ============================================================================
 */

SensorSimulator::SensorSimulator()
    : m_rng(std::random_device{}()) {
    resetState();
}

void SensorSimulator::resetState() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.speed_kmh = 60.0;
    m_data.speed_limit = 70;
    m_data.radar_distance = 50.0;
    m_data.obstacle_type = "Yok";
    m_data.obstacle_distance = 100.0;
    m_data.lane_offset = 0.0;
    m_data.is_gas_pressed = false;
    m_data.is_brake_pressed = false;
    m_data.left_signal = false;
    m_data.right_signal = false;

    m_obstacleTimer = 0.0;
    m_nextObstacleInterval = 8.0;
    m_speedLimitTimer = 0.0;
    m_nextSpeedLimitInterval = 14.0;
    m_laneDriftTimer = 0.0;
    m_targetLaneDrift = 0.0;
}

void SensorSimulator::update(double dt_seconds) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 1. & 2. Gaz (W tuşu), Fren ve Sürtünme Fiziği ile Hız Hesabı
    const double ACCEL_RATE = 12.0;      // Gaz ivmesi (+12 km/s^2)
    const double BRAKE_RATE = 35.0;      // Fren yavaşlama ivmesi (-35 km/s^2)
    const double FRICTION_RATE = 2.5;    // Doğal sürtünme / motor freni (-2.5 km/s^2)
    const double MAX_SPEED_KMH = 140.0;  // Maksimum hız sınırı

    if (m_data.is_brake_pressed) {
        m_data.speed_kmh -= BRAKE_RATE * dt_seconds;
        if (m_data.speed_kmh <= 0.0) {
            m_data.speed_kmh = 0.0;
            m_data.is_brake_pressed = false;
        }
    } else if (m_data.is_gas_pressed) {
        m_data.speed_kmh += ACCEL_RATE * dt_seconds;
    } else {
        m_data.speed_kmh -= FRICTION_RATE * dt_seconds;
    }

    m_data.speed_kmh = std::clamp(m_data.speed_kmh, 0.0, MAX_SPEED_KMH);

    // Anlık hızın m/s cinsine çevrilmesi ve alınan yol (m)
    double v_ms = m_data.speed_kmh / 3.6;
    double distance_step = v_ms * dt_seconds;

    // 3. Radar ve Engel Mesafelerinin Hız ile Güncellenmesi
    if (v_ms > 0.0) {
        // Öndeki araca yaklaşma simülasyonu
        m_data.radar_distance -= distance_step * 0.45;
    }

    // Araç AEB ile durma noktasına geldiyse veya öndeki araca çok yaklaştıysa
    // yeni bir trafik senaryosu için radar mesafesini güvenli aralığa yenile
    if (m_data.radar_distance <= 5.0 || (m_data.radar_distance < 20.0 && m_data.speed_kmh <= 2.0)) {
        std::uniform_real_distribution<double> radarDist(60.0, 90.0);
        m_data.radar_distance = radarDist(m_rng);
        m_data.is_brake_pressed = false;
    }

    // Yol üzerindeki engel (Çukur / Tümsek) mesafesinin güncellenmesi
    if (m_data.obstacle_type != "Yok") {
        m_data.obstacle_distance -= distance_step;
        if (m_data.obstacle_distance <= 0.0) {
            m_data.obstacle_type = "Yok";
            m_data.obstacle_distance = 100.0;
        }
    } else if (m_data.speed_kmh > 10.0) {
        // Rastgele Yol Anomalisi (Çukur / Tümsek) Üretimi
        m_obstacleTimer += dt_seconds;
        if (m_obstacleTimer >= m_nextObstacleInterval) {
            m_obstacleTimer = 0.0;
            std::uniform_real_distribution<double> intervalDist(8.0, 14.0);
            std::uniform_real_distribution<double> obsDist(35.0, 55.0);
            std::uniform_int_distribution<int> typeDist(0, 1);

            m_nextObstacleInterval = intervalDist(m_rng);
            m_data.obstacle_type = (typeDist(m_rng) == 0) ? "Cukur" : "Tumsek";
            m_data.obstacle_distance = obsDist(m_rng);

            Logger::getInstance().log(
                "SENSOR",
                "Yol Anomalisi Algılandı: " + m_data.obstacle_type +
                " (" + std::to_string(static_cast<int>(m_data.obstacle_distance)) + "m)"
            );
        }
    }

    // Rastgele Hız Sınırı Tabelası (50, 70, 90 km/s) Üretimi
    if (m_data.speed_kmh > 5.0) {
        m_speedLimitTimer += dt_seconds;
        if (m_speedLimitTimer >= m_nextSpeedLimitInterval) {
            m_speedLimitTimer = 0.0;
            std::uniform_real_distribution<double> intervalDist(12.0, 20.0);
            std::uniform_int_distribution<int> limitIdxDist(0, 2);

            const int limits[3] = {50, 70, 90};
            int newLimit = limits[limitIdxDist(m_rng)];
            m_nextSpeedLimitInterval = intervalDist(m_rng);

            if (newLimit != m_data.speed_limit) {
                m_data.speed_limit = newLimit;
                Logger::getInstance().log(
                    "SENSOR",
                    "Hız Sınırı Tabelası Algılandı: " + std::to_string(m_data.speed_limit) + " km/s"
                );
            }
        }
    }

    // 4. Şerit Sapmasının (lane_offset) Sinyal Durumlarına ve Yol Salınımına Göre Güncellenmesi
    const double LANE_SHIFT_SPEED = 0.55;  // Sinyal verildiğinde şerit değiştirme hızı (m/s)
    const double LANE_RETURN_SPEED = 0.30; // Şerit merkezine dönüş hızı (m/s)

    if (m_data.left_signal) {
        m_data.lane_offset -= LANE_SHIFT_SPEED * dt_seconds;
        m_targetLaneDrift = 0.0;
    } else if (m_data.right_signal) {
        m_data.lane_offset += LANE_SHIFT_SPEED * dt_seconds;
        m_targetLaneDrift = 0.0;
    } else {
        // Sinyal kapalıyken periyodik hafif şerit gezinmesi / sapma simülasyonu
        if (m_data.speed_kmh > 15.0) {
            m_laneDriftTimer += dt_seconds;
            if (m_laneDriftTimer >= 10.0) {
                m_laneDriftTimer = 0.0;
                std::uniform_int_distribution<int> driftChoice(0, 2);
                int choice = driftChoice(m_rng);
                if (choice == 0) {
                    m_targetLaneDrift = -0.68; // Sol şerit ihlali testi için
                } else if (choice == 1) {
                    m_targetLaneDrift = 0.68;  // Sağ şerit ihlali testi için
                } else {
                    m_targetLaneDrift = 0.0;   // Şerit ortası
                }
            } else if (m_laneDriftTimer >= 4.0) {
                // Sapma sonrası tekrar şerit merkezine dön
                m_targetLaneDrift = 0.0;
            }
        } else {
            m_targetLaneDrift = 0.0;
        }

        double diff = m_targetLaneDrift - m_data.lane_offset;
        double maxStep = LANE_RETURN_SPEED * dt_seconds;
        if (std::abs(diff) <= maxStep) {
            m_data.lane_offset = m_targetLaneDrift;
        } else {
            m_data.lane_offset += (diff > 0.0 ? maxStep : -maxStep);
        }
    }

    m_data.lane_offset = std::clamp(m_data.lane_offset, -1.2, 1.2);
}

TelemetryData SensorSimulator::getData() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data;
}

void SensorSimulator::setGasPressed(bool pressed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.is_gas_pressed = pressed;
    if (pressed) {
        m_data.is_brake_pressed = false;
    }
}

void SensorSimulator::setBrakePressed(bool pressed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.is_brake_pressed = pressed;
}

void SensorSimulator::setLeftSignal(bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.left_signal = active;
    if (active) m_data.right_signal = false;
}

void SensorSimulator::setRightSignal(bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.right_signal = active;
    if (active) m_data.left_signal = false;
}

void SensorSimulator::spawnObstacle(const std::string& type, double distance) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.obstacle_type = type;
    m_data.obstacle_distance = distance;
    m_obstacleTimer = 0.0;
}

void SensorSimulator::setSpeedLimit(int limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.speed_limit = limit;
    m_speedLimitTimer = 0.0;
}

#include "../include/SensorSimulator.hpp"
#include "../include/Logger.hpp"
#include <cmath>

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 1. Simülasyon Geliştirici (Data Generator)
 * ============================================================================
 */

// Kurucu fonksiyon: Rastgele sayı üretecini başlatır ve başlangıç değerlerini atar
SensorSimulator::SensorSimulator()
    : m_rng(std::random_device{}()) {
    resetState();
}

// Simülasyonu başlangıç değerlerine sıfırlayan fonksiyon
void SensorSimulator::resetState() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Başlangıç telemetri değerleri
    m_data.speed_kmh = 60.0;
    m_data.speed_limit = 70;
    m_data.radar_distance = 55.0;
    m_data.obstacle_type = "Yok";
    m_data.obstacle_distance = 100.0;
    m_data.lane_offset = 0.0;

    // Sürücü kontrol durumları
    m_data.is_gas_pressed = false;
    m_data.is_brake_pressed = false;
    m_data.left_signal = false;
    m_data.right_signal = false;

    // ACC Durumlarının sıfırlanması
    m_data.acc_enabled = false;
    m_data.acc_set_speed = 70.0;
    m_accThrottleDemand = 0.0;
    m_accBrakeDemand = 0.0;

    // Zamanlayıcıların sıfırlanması
    m_leadCarSpeed = 35.0;
    m_data.lead_car_speed_kmh = m_leadCarSpeed;
    m_obstacleTimer = 0.0;
    m_nextObstacleInterval = 8.0;
    m_speedLimitTimer = 0.0;
    m_nextSpeedLimitInterval = 14.0;
    m_laneDriftTimer = 0.0;
    m_targetLaneDrift = 0.0;

    Logger::getInstance().log("SENSOR", "Simulasyon baslangic durumuna sifirlandi.");
}

// Her döngü adımında (dt_seconds = 0.05 sn) çalışan ana fizik ve sensör fonksiyonu
void SensorSimulator::update(double dt_seconds) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // ========================================================================
    // 1. ARAÇ HIZ FİZİĞİ (Gaz, Fren ve Yol Sürtünmesi)
    // ========================================================================
    const double GAZ_IVMESI = 14.0;       // W tuşuna basıldığında hızlanma ivmesi (km/s^2)
    const double FREN_IVMESI = 35.0;      // Fren yapıldığında yavaşlama ivmesi (km/s^2)
    const double SURTUNME_IVMESI = 2.5;   // Gaz bırakıldığında doğal yavaşlama (km/s^2)
    const double MAKS_HIZ = 140.0;        // Aracın çıkabileceği maksimum hız (km/s)

    if (m_data.is_brake_pressed) {
        // Fren basılıysa aracı hızla yavaşlat (v = v0 - a * t)
        m_data.speed_kmh -= FREN_IVMESI * dt_seconds;
        if (m_data.speed_kmh <= 0.0) {
            m_data.speed_kmh = 0.0;
            m_data.is_brake_pressed = false;
        }
    } else if (m_data.is_gas_pressed) {
        // Gaz (W) basılıysa aracı hızlandır (v = v0 + a * t)
        m_data.speed_kmh += GAZ_IVMESI * dt_seconds;
    } else if (m_data.acc_enabled && (m_accBrakeDemand > 0.0 || m_accThrottleDemand > 0.0)) {
        // ACC Otonom Hız Kontrolü (Gaz ve Fren talepleri)
        if (m_accBrakeDemand > 0.0) {
            m_data.speed_kmh -= (FREN_IVMESI * m_accBrakeDemand) * dt_seconds;
            if (m_data.speed_kmh < 0.0) m_data.speed_kmh = 0.0;
        } else if (m_accThrottleDemand > 0.0) {
            m_data.speed_kmh += (GAZ_IVMESI * m_accThrottleDemand) * dt_seconds;
        }
    } else {
        // Gaz ve fren basılı değilse sürtünme ve motor freni ile yavaşlat
        // Yüksek hızlarda hava direnci etkisiyle biraz daha fazla yavaşlar
        double havaDirenci = (m_data.speed_kmh / 100.0) * 1.2;
        m_data.speed_kmh -= (SURTUNME_IVMESI + havaDirenci) * dt_seconds;
    }

    // Hız sınırlarını (0 ile 140 km/s arası) koru
    if (m_data.speed_kmh < 0.0) {
        m_data.speed_kmh = 0.0;
    } else if (m_data.speed_kmh > MAKS_HIZ) {
        m_data.speed_kmh = MAKS_HIZ;
    }

    // Hızı km/s biriminden m/s birimine çevirme (1 m/s = 3.6 km/s)
    double aracHizi_ms = m_data.speed_kmh / 3.6;
    // Bu zaman adımında alınan yol: x = v * t (metre)
    double alinanYol_m = aracHizi_ms * dt_seconds;


    // ========================================================================
    // 2. RADAR MESAFESİ VE ÖNDEKİ ARAÇ SİMÜLASYONU
    // ========================================================================
    // Öndeki aracın hızını da m/s cinsine çevirip bağıl hız farkını buluyoruz
    double ondekiAracHizi_ms = m_leadCarSpeed / 3.6;
    double hizFarki_ms = aracHizi_ms - ondekiAracHizi_ms;

    if (m_data.speed_kmh > 0.0) {
        // Biz öndeki araçtan hızlıysak mesafe azalır, yavaşsak mesafe artar
        m_data.radar_distance -= hizFarki_ms * dt_seconds;
    }

    // Eğer radar mesafesi çok açıldıysa (100 metreyi geçtiyse) sabit tut
    if (m_data.radar_distance > 100.0) {
        m_data.radar_distance = 100.0;
    }

    // Araç AEB ile durduysa veya öndeki araca çok yaklaştıysa yeni trafik senaryosu başlat
    if (m_data.radar_distance <= 5.0 || (m_data.radar_distance < 20.0 && m_data.speed_kmh <= 5.0)) {
        std::uniform_real_distribution<double> yeniMesafe(60.0, 85.0);
        std::uniform_real_distribution<double> yeniOndekiHiz(30.0, 45.0);

        m_data.radar_distance = yeniMesafe(m_rng);
        m_leadCarSpeed = yeniOndekiHiz(m_rng);
        m_data.lead_car_speed_kmh = m_leadCarSpeed;
        m_data.is_brake_pressed = false;

        Logger::getInstance().log("SENSOR", "Radar: Yeni on arac algilandi (" + 
            std::to_string(static_cast<int>(m_data.radar_distance)) + "m).");
    }


    // ========================================================================
    // 3. YOL ANOMALİLERİ (ÇUKUR / TÜMSEK) ÜRETİMİ VE MESAFE TAKİBİ
    // ========================================================================
    if (m_data.obstacle_type != "Yok") {
        // Yolda aktif bir engel varsa, aracımız ilerledikçe engele olan mesafe azalır
        m_data.obstacle_distance -= alinanYol_m;

        // Engelin üzerinden geçildiyse engeli temizle
        if (m_data.obstacle_distance <= 0.0) {
            m_data.obstacle_type = "Yok";
            m_data.obstacle_distance = 100.0;
        }
    } else if (m_data.speed_kmh > 10.0) {
        // Yolda engel yoksa ve araç hareket halindeyse belirli aralıklarla rastgele engel üret
        m_obstacleTimer += dt_seconds;
        if (m_obstacleTimer >= m_nextObstacleInterval) {
            m_obstacleTimer = 0.0;

            std::uniform_real_distribution<double> sureAraligi(8.0, 14.0);
            std::uniform_real_distribution<double> engelMesafesi(35.0, 55.0);
            std::uniform_int_distribution<int> engelTipiSecimi(0, 1);

            m_nextObstacleInterval = sureAraligi(m_rng);
            m_data.obstacle_distance = engelMesafesi(m_rng);

            if (engelTipiSecimi(m_rng) == 0) {
                m_data.obstacle_type = "Cukur";
            } else {
                m_data.obstacle_type = "Tumsek";
            }

            Logger::getInstance().log("SENSOR", "Yol Anomalisi Algilandi: " + m_data.obstacle_type +
                " (" + std::to_string(static_cast<int>(m_data.obstacle_distance)) + "m)");
        }
    }


    // ========================================================================
    // 4. RASTGELE HIZ SINIRI TABELASI (TSR: 50, 70, 90 km/s) ÜRETİMİ
    // ========================================================================
    if (m_data.speed_kmh > 5.0) {
        m_speedLimitTimer += dt_seconds;
        if (m_speedLimitTimer >= m_nextSpeedLimitInterval) {
            m_speedLimitTimer = 0.0;

            std::uniform_real_distribution<double> tabelaSuresi(12.0, 20.0);
            std::uniform_int_distribution<int> tabelaSecimi(0, 2);

            const int hizTabelalari[3] = {50, 70, 90};
            int yeniTabela = hizTabelalari[tabelaSecimi(m_rng)];
            m_nextSpeedLimitInterval = tabelaSuresi(m_rng);

            // Sadece önceki tabeladan farklı bir tabela geldiyse güncelle ve logla
            if (yeniTabela != m_data.speed_limit) {
                m_data.speed_limit = yeniTabela;
                Logger::getInstance().log("SENSOR", "Hiz Siniri Tabelasi Algilandi: " +
                    std::to_string(m_data.speed_limit) + " km/s");
            }
        }
    }


    // ========================================================================
    // 5. ŞERİT TAKİBİ, SİNYAL VE ŞERİT SAPMASI (LDW) SİMÜLASYONU
    // ========================================================================
    const double SERIT_DEGISTIRME_HIZI = 0.55; // Sinyal verildiğinde yan şeride kayma hızı (m/s)
    const double SERIT_TOPARLAMA_HIZI = 0.30;  // Şerit merkezine dönüş / gezinme hızı (m/s)

    if (m_data.left_signal) {
        // Sol sinyal açıksa araç sola doğru (- yönde) şerit değiştirir
        m_data.lane_offset -= SERIT_DEGISTIRME_HIZI * dt_seconds;
        m_targetLaneDrift = 0.0;
    } else if (m_data.right_signal) {
        // Sağ sinyal açıksa araç sağa doğru (+ yönde) şerit değiştirir
        m_data.lane_offset += SERIT_DEGISTIRME_HIZI * dt_seconds;
        m_targetLaneDrift = 0.0;
    } else {
        // Sinyaller kapalıyken rüzgar veya yol eğimi sebebiyle rastgele şerit sapması
        if (m_data.speed_kmh > 15.0) {
            m_laneDriftTimer += dt_seconds;

            if (m_laneDriftTimer >= 10.0) {
                m_laneDriftTimer = 0.0;
                std::uniform_int_distribution<int> yonSecimi(0, 2);
                int secim = yonSecimi(m_rng);

                if (secim == 0) {
                    m_targetLaneDrift = -0.68; // Sola kayma (Şerit ihlali testi için < -0.5m)
                } else if (secim == 1) {
                    m_targetLaneDrift = 0.68;  // Sağa kayma (Şerit ihlali testi için > +0.5m)
                } else {
                    m_targetLaneDrift = 0.0;   // Şerit merkezinde kalma
                }
            } else if (m_laneDriftTimer >= 4.0) {
                // Sapma başladıktan bir süre sonra sürücü/sistem aracı tekrar şerit ortasına toplar
                m_targetLaneDrift = 0.0;
            }
        } else {
            m_targetLaneDrift = 0.0;
        }

        // Mevcut şerit konumunu (lane_offset) hedef sapma değerine (m_targetLaneDrift) yumuşakça yaklaştır
        double fark = m_targetLaneDrift - m_data.lane_offset;
        double adimMiktari = SERIT_TOPARLAMA_HIZI * dt_seconds;

        if (std::abs(fark) <= adimMiktari) {
            m_data.lane_offset = m_targetLaneDrift;
        } else if (fark > 0.0) {
            m_data.lane_offset += adimMiktari;
        } else {
            m_data.lane_offset -= adimMiktari;
        }
    }

    // Şerit sapma değerini -1.2m ile +1.2m arasında sınırla
    if (m_data.lane_offset < -1.2) {
        m_data.lane_offset = -1.2;
    } else if (m_data.lane_offset > 1.2) {
        m_data.lane_offset = 1.2;
    }
}


// ============================================================================
// GETTER VE SETTER (KONTROL) FONKSİYONLARI
// ============================================================================

TelemetryData SensorSimulator::getData() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data;
}

void SensorSimulator::applyActuatorCommand(const ActuatorCommand& command) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (command.aeb_brake_active) {
        m_data.is_brake_pressed = true;
    }
    if (command.throttle_override) {
        m_data.is_gas_pressed = false;
    }
    m_accThrottleDemand = command.acc_throttle_demand;
    m_accBrakeDemand = command.acc_brake_demand;
}

void SensorSimulator::setGasPressed(bool pressed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.is_gas_pressed = pressed;
    // Gaza basıldığında freni bırak
    if (pressed) {
        m_data.is_brake_pressed = false;
    }
}

void SensorSimulator::setBrakePressed(bool pressed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.is_brake_pressed = pressed;
    // Sürücü frene basarsa ACC güvenlik için devreden çıkar
    if (pressed) {
        m_data.acc_enabled = false;
        m_accThrottleDemand = 0.0;
        m_accBrakeDemand = 0.0;
    }
}

void SensorSimulator::setAccEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.acc_enabled = enabled;
    if (!enabled) {
        m_accThrottleDemand = 0.0;
        m_accBrakeDemand = 0.0;
    }
    Logger::getInstance().log("SENSOR", "ACC Durumu Guncellendi: " + std::string(enabled ? "Aktif" : "Pasif"));
}

void SensorSimulator::setAccSetSpeed(double speed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (speed < 30.0) speed = 30.0;
    if (speed > 130.0) speed = 130.0;
    m_data.acc_set_speed = speed;
    Logger::getInstance().log("SENSOR", "ACC Hedef Hizi Ayarlandi: " + std::to_string(static_cast<int>(speed)) + " km/s");
}

void SensorSimulator::setLeftSignal(bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.left_signal = active;
    // Sol sinyal açıldığında sağ sinyali kapat
    if (active) {
        m_data.right_signal = false;
    }
}

void SensorSimulator::setRightSignal(bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.right_signal = active;
    // Sağ sinyal açıldığında sol sinyali kapat
    if (active) {
        m_data.left_signal = false;
    }
}

// Manuel olarak yola Çukur veya Tümsek ekleme fonksiyonu
void SensorSimulator::spawnObstacle(const std::string& type, double distance) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.obstacle_type = type;
    m_data.obstacle_distance = distance;
    m_obstacleTimer = 0.0;

    Logger::getInstance().log("SENSOR", "Manuel Engel Olusturuldu: " + type +
        " (" + std::to_string(static_cast<int>(distance)) + "m)");
}

// Manuel olarak hız sınırı tabelasını değiştirme fonksiyonu
void SensorSimulator::setSpeedLimit(int limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.speed_limit = limit;
    m_speedLimitTimer = 0.0;

    Logger::getInstance().log("SENSOR", "Hiz Siniri Manuel Degistirildi: " +
        std::to_string(limit) + " km/s");
}

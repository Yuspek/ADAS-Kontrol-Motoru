#ifndef VEHICLE_INTERFACES_HPP
#define VEHICLE_INTERFACES_HPP

/**
 * ============================================================================
 * DONANIM VE SİMÜLASYON SOYUTLAMA KATMANI (HAL / SAL)
 * ============================================================================
 * Bu başlık dosyası, ADAS Karar Motorunun (AdasController) veri kaynağından
 * bağımsız çalışmasını sağlar:
 *   1. Web SIL Simülatörü (SensorSimulator)
 *   2. NVIDIA Isaac Sim / DRIVE Sim / CARLA (ROS 2 / DDS Bridge)
 *   3. Gerçek Araç Donanımı (CAN-Bus / SocketCAN / Radar & Kamera)
 */

#include <string>

// Sensörlerden veya simülasyondan okunan standart telemetri veri paketi
struct TelemetryData {
    double speed_kmh = 0.0;             // Aracın anlık hızı (km/s)
    double lead_car_speed_kmh = 35.0;   // Öndeki aracın hızı (km/s)
    int speed_limit = 70;               // Okunan hız sınırı tabelası (50, 70, 90 km/s)
    double radar_distance = 50.0;       // Öndeki araca/engele olan radar mesafesi (m)

    std::string obstacle_type = "Yok";  // "Yok", "Cukur", "Tumsek"
    double obstacle_distance = 100.0;   // Engelin mesafesi (m)
    double lane_offset = 0.0;           // Şerit merkezinden sapma miktarı (m, -Sol / +Sağ)

    bool is_gas_pressed = false;        // Gaz pedalı / W tuşu durumu
    bool is_brake_pressed = false;      // Fren pedalı / Space tuşu durumu
    bool left_signal = false;           // Sol sinyal durumu
    bool right_signal = false;          // Sağ sinyal durumu

    // ACC (Adaptif Hız Sabitleyici) Girdileri
    bool acc_enabled = false;           // ACC devrede mi (kullanıcı/dashboard açtı mı?)
    double acc_set_speed = 70.0;        // ACC hedef hızı (km/s)
};

// ADAS Karar Motorunun ürettiği standart eyleyici (aktüatör) komut paketi
// (Simülatöre, ROS 2 kontrol topiğine veya gerçek araç CAN-Bus hattına iletilir)
struct ActuatorCommand {
    bool aeb_brake_active = false;          // Acil frenleme talebi
    double target_brake_pressure = 0.0;     // Hedef fren basıncı (0.0 - 1.0 normalize)
    bool throttle_override = false;         // Sürücü gaz komutunu kesme (Manual Override)
    int target_suspension_stiffness = 50;   // Hedef süspansiyon sertliği (%20, %50, %80)

    // ACC Aktüatör Talepleri (Otonom boyuna sürüş kontrolü)
    double acc_throttle_demand = 0.0;       // ACC gaz talebi (0.0 - 1.0)
    double acc_brake_demand = 0.0;          // ACC fren talebi (0.0 - 1.0)
};

// Sensör Okuma Soyutlama Arayüzü (Sensor Abstraction Layer)
class ISensorSource {
public:
    virtual ~ISensorSource() = default;
    virtual TelemetryData getData() = 0;
};

// Aktüatör Yazma Soyutlama Arayüzü (Hardware Abstraction Layer)
class IActuatorSink {
public:
    virtual ~IActuatorSink() = default;
    virtual void applyActuatorCommand(const ActuatorCommand& command) = 0;
};

// Tam Araç Platformu Arayüzü (Sensör + Aktüatör + Fizik/Yaşam Döngüsü)
class IVehiclePlatform : public ISensorSource, public IActuatorSink {
public:
    virtual ~IVehiclePlatform() = default;
    virtual void update(double dt_seconds) = 0;
};

#endif // VEHICLE_INTERFACES_HPP

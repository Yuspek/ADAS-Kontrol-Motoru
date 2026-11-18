#ifndef ADAS_CONTROLLER_HPP
#define ADAS_CONTROLLER_HPP

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 2. Karar Algoritmacısı (Core Engine)
 * ============================================================================
 * Sorumluluklar:
 * - AdasController sınıfında donanımdan/simülatörden bağımsız (Pure Logic) karar mantıklarını kodlamak.
 * - AEB (Acil Fren) ve Sürücüyü Ezme (Manual Override) koşullarını yazmak.
 * - ACC (Adaptif Hız Sabitleyici: Hız koruma ve öndeki aracı güvenli mesafeden takip) mantığını yönetmek.
 * - Akıllı Süspansiyon (Çukur -> %20 yumuşak, Tümsek -> %80 sert, t < 1.5s) matematiğini kurmak.
 * - TSR Hız uyarısı bayrağını (%10 aşımında) kaldırmak.
 * - LDW Şerit Uyarısı ve Sinyal kontrollü "Bilinçli Sollama" mantığını kodlamak.
 */

#include "VehicleInterfaces.hpp"
#include <string>
#include <mutex>

struct AdasDecisionState {
    bool aeb_active = false;            // AEB aktif mi?
    bool driver_override = false;       // Sürücü gaz komutu ezildi mi?
    double ttc_seconds = 999.0;         // Çarpışmaya kalan süre (Time-To-Collision)

    // ACC Durumları
    bool acc_active = false;            // ACC şu anda aracı yönetiyor mu?
    std::string acc_mode = "Kapali";    // "Kapali", "Hiz Sabitleme", "Mesafe Takibi", "AEB Mudahale", "Surucu Mudahalesi"
    double acc_target_speed = 70.0;     // Hedef ACC seyir hızı (km/s)
    double acc_safe_distance = 35.0;    // Dinamik hesaplanan güvenli takip mesafesi (m)
    double acc_throttle_demand = 0.0;   // ACC gaz talebi
    double acc_brake_demand = 0.0;      // ACC fren talebi

    int suspension_stiffness = 50;      // Süspansiyon sertliği (%)
    std::string suspension_mode = "Normal";

    bool speed_warning = false;         // Hız ihlali var mı?
    double speed_overshoot = 0.0;       // Sınır aşım miktarı (km/s)

    bool ldw_warning = false;           // Şerit uyarısı var mı?
    bool ldw_suppressed = false;        // Sinyal sebebiyle uyarı bastırıldı mı?
    std::string ldw_status_text = "Serit Ortasinda";
};

class AdasController {
public:
    // Hem SensorSimulator hem de gelecekteki ROS2/IsaacSim/CAN-Bus platformlarıyla çalışır
    explicit AdasController(IVehiclePlatform& platform);

    // Her periyotta çalışacak ana karar döngüsü (Sensör Oku -> Karar Ver -> Aktüatöre Uygula)
    void processLogic(double dt_seconds);

    // Saf (Pure) karar fonksiyonu: Girdi olarak TelemetryData alır, ActuatorCommand üretir
    ActuatorCommand evaluate(const TelemetryData& telemetry);

    // Güncel karar ve aktüatör durumunu döndürür (Thread-safe)
    AdasDecisionState getDecisionState();
    ActuatorCommand getLastActuatorCommand();

private:
    IVehiclePlatform& m_platform;
    AdasDecisionState m_state;
    ActuatorCommand m_lastCommand;
    std::mutex m_mutex;

    // Alt karar algoritmaları (Saf / Pure fonksiyonlar)
    void processAEB(const TelemetryData& telemetry, AdasDecisionState& nextState, ActuatorCommand& cmd);
    void processACC(const TelemetryData& telemetry, AdasDecisionState& nextState, ActuatorCommand& cmd);
    void processSuspension(const TelemetryData& telemetry, AdasDecisionState& nextState, ActuatorCommand& cmd);
    void processTSR(const TelemetryData& telemetry, AdasDecisionState& nextState);
    void processLDW(const TelemetryData& telemetry, AdasDecisionState& nextState);
    void logStateTransitions(const AdasDecisionState& prevState, const AdasDecisionState& newState, const TelemetryData& telemetry);
};

#endif // ADAS_CONTROLLER_HPP

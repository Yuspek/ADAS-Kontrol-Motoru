#ifndef ADAS_CONTROLLER_HPP
#define ADAS_CONTROLLER_HPP

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 2. Karar Algoritmacısı (Core Engine)
 * ============================================================================
 * Sorumluluklar:
 * - AdasController sınıfında karar mantıklarını kodlamak.
 * - AEB (Acil Fren) ve Sürücüyü Ezme (Manual Override) koşullarını yazmak.
 * - Akıllı Süspansiyon (Çukur -> %20 yumuşak, Tümsek -> %80 sert, t < 1.5s) matematiğini kurmak.
 * - TSR Hız uyarısı bayrağını (%10 veya +5 km/s aşımında) kaldırmak.
 * - LDW Şerit Uyarısı ve Sinyal kontrollü "Bilinçli Sollama" mantığını kodlamak.
 */

#include "SensorSimulator.hpp"
#include <string>

struct AdasDecisionState {
    bool aeb_active = false;            // AEB aktif mi?
    bool driver_override = false;      // Sürücü gaz komutu ezildi mi?
    
    int suspension_stiffness = 50;      // Süspansiyon sertliği (%)
    std::string suspension_mode = "Normal";
    
    bool speed_warning = false;         // Hız ihlali var mı?
    double speed_overshoot = 0.0;       // Sınır aşım miktarı (km/s)
    
    bool ldw_warning = false;           // Şerit uyarısı var mı?
    bool ldw_suppressed = false;        // Sinyal sebebiyle uyarı bastırıldı mı?
    std::string ldw_status_text = "Şerit Ortasında";
};

class AdasController {
public:
    AdasController(SensorSimulator& simulator);

    // Her periyotta çalışacak ana karar algoritması
    void processLogic(double dt_seconds);

    // Güncel karar durumunu döndürür
    AdasDecisionState getDecisionState();

private:
    SensorSimulator& m_simulator;
    AdasDecisionState m_state;

    // TODO (Geliştirici 2): Algoritma alt fonksiyonlarını tanımlayınız.
    void processAEB(const TelemetryData& telemetry);
    void processSuspension(const TelemetryData& telemetry);
    void processTSR(const TelemetryData& telemetry);
    void processLDW(const TelemetryData& telemetry);
};

#endif // ADAS_CONTROLLER_HPP

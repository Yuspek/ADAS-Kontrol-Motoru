#include "AdasController.hpp"
#include "Logger.hpp"

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 2. Karar Algoritmacısı (Core Engine)
 * ============================================================================
 */

AdasController::AdasController(SensorSimulator& simulator)
    : m_simulator(simulator) {}

void AdasController::processLogic(double dt_seconds) {
    TelemetryData telemetry = m_simulator.getData();

    // Algoritma adımlarının çağrılması
    processAEB(telemetry);
    processSuspension(telemetry);
    processTSR(telemetry);
    processLDW(telemetry);
}

void AdasController::processAEB(const TelemetryData& telemetry) {
    // TODO (Geliştirici 2): 
    // Öndeki araca mesafe <= 15m ise ve gaz basılıysa:
    // m_state.driver_override = true, m_simulator.setGasPressed(false), m_simulator.setBrakePressed(true)
}

void AdasController::processSuspension(const TelemetryData& telemetry) {
    // TODO (Geliştirici 2): 
    // t = obstacle_distance / v_ms hesaplayınız.
    // t < 1.5s ise; "Çukur" -> %20 sertlik, "Tümsek" -> %80 sertlik ayarlayınız.
}

void AdasController::processTSR(const TelemetryData& telemetry) {
    // TODO (Geliştirici 2): 
    // Araç hızı, hız sınırını %10 veya +5 km/s aşarsa m_state.speed_warning = true yapınız.
}

void AdasController::processLDW(const TelemetryData& telemetry) {
    // TODO (Geliştirici 2): 
    // Şerit sapması > 0.5m ve ilgili yöne sinyal verilmişse bastırınız (Bilinçli Sollama).
    // Sinyal verilmediyse m_state.ldw_warning = true yapınız.
}

AdasDecisionState AdasController::getDecisionState() {
    return m_state;
}

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
    double v_ms = telemetry.speed_kmh / 3.6;
    double ttc = 999.0;
    if (v_ms > 0.5) {
        ttc = telemetry.radar_distance / v_ms;
    }

    if (ttc <= 1.5) {
        m_state.aeb_active = true;
        m_simulator.setBrakePressed(true);

        if (telemetry.is_gas_pressed) {
            m_state.driver_override = true;
            m_simulator.setGasPressed(false);
        } else {
            m_state.driver_override = false;
        }
    } else {
        m_state.aeb_active = false;
        m_state.driver_override = false;
    }
}

void AdasController::processSuspension(const TelemetryData& telemetry) {
    // TODO (Geliştirici 2): 
    // t = obstacle_distance / v_ms hesaplayınız.
    // t < 1.5s ise; "Çukur" -> %20 sertlik, "Tümsek" -> %80 sertlik ayarlayınız.
    double v_ms=telemetry.speed_kmh/3.6;
    double t=999.0;
    if(v_ms>0.5){
        t=telemetry.obstacle_distance/v_ms;
    }
    if(t<1.5){
        if(telemetry.obstacle_type=="Cukur"){
            m_state.suspension_stiffness=20;
            m_state.suspension_mode="Yumusak";
        }
        else if(telemetry.obstacle_type=="Tumsek"){
            m_state.suspension_stiffness=80;
            m_state.suspension_mode="Sert";
        }
    }
    else{
        m_state.suspension_stiffness=50;
        m_state.suspension_mode="Normal";
    }
}

void AdasController::processTSR(const TelemetryData& telemetry) {
    double limit_esigi = telemetry.speed_limit * 1.10;
    if (telemetry.speed_kmh > limit_esigi) {
        m_state.speed_warning = true;
        m_state.speed_overshoot = telemetry.speed_kmh - telemetry.speed_limit;
    } else {
        m_state.speed_warning = false;
        m_state.speed_overshoot = 0.0;
    }
}

void AdasController::processLDW(const TelemetryData& telemetry) {
    if (telemetry.lane_offset < -0.5) {
        if (telemetry.left_signal) {
            m_state.ldw_warning = false;
            m_state.ldw_suppressed = true;
            m_state.ldw_status_text = "Bilincli Serit Degisimi";
        } else {
            m_state.ldw_warning = true;
            m_state.ldw_suppressed = false;
            m_state.ldw_status_text = "Serit Ihlali";
        }
    } else if (telemetry.lane_offset > 0.5) {
        if (telemetry.right_signal) {
            m_state.ldw_warning = false;
            m_state.ldw_suppressed = true;
            m_state.ldw_status_text = "Bilincli Serit Degisimi";
        } else {
            m_state.ldw_warning = true;
            m_state.ldw_suppressed = false;
            m_state.ldw_status_text = "Serit Ihlali";
        }
    } else {
        m_state.ldw_warning = false;
        m_state.ldw_suppressed = false;
        m_state.ldw_status_text = "Serit Ortasinda";
    }
}

AdasDecisionState AdasController::getDecisionState() {
    return m_state;
}

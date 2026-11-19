#include "../include/AdasController.hpp"
#include "../include/Logger.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 2. Karar Algoritmacısı (Core Engine)
 * ============================================================================
 */

AdasController::AdasController(IVehiclePlatform &platform)
    : m_platform(platform) {}

void AdasController::processLogic(double /*dt_seconds*/) {
  // 1. Sensör soyutlama katmanından telemetri verisini oku
  TelemetryData telemetry = m_platform.getData();

  // 2. Saf karar motorunu çalıştır ve eyleyici (aktüatör) komutunu üret
  ActuatorCommand command = evaluate(telemetry);

  // 3. Üretilen aktüatör komutunu donanım/simülasyon katmanına ilet
  m_platform.applyActuatorCommand(command);
}

ActuatorCommand AdasController::evaluate(const TelemetryData &telemetry) {
  AdasDecisionState nextState;
  ActuatorCommand cmd;

  // Alt karar algoritmalarını çalıştır
  processAEB(telemetry, nextState, cmd);
  processACC(telemetry, nextState, cmd);
  processSuspension(telemetry, nextState, cmd);
  processTSR(telemetry, nextState);
  processLDW(telemetry, nextState);

  AdasDecisionState prevState;
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    prevState = m_state;
    m_state = nextState;
    m_lastCommand = cmd;
  }

  // Yalnızca durum değişimlerinde log üret (20 Hz log taşmasını önler)
  logStateTransitions(prevState, nextState, telemetry);

  return cmd;
}

void AdasController::processAEB(const TelemetryData &telemetry,
                                AdasDecisionState &nextState,
                                ActuatorCommand &cmd) {
  // Kritik mesafe (15.0 m) veya Çarpışma Süresi (TTC <= 1.5 sn) kontrolü
  const double AEB_CRITICAL_DISTANCE_M = 15.0;
  const double AEB_CRITICAL_TTC_S = 1.5;

  double v_ms = telemetry.speed_kmh / 3.6;
  double ttc = 999.0;
  if (v_ms > 0.5) {
    ttc = telemetry.radar_distance / v_ms;
  }
  nextState.ttc_seconds = ttc;

  bool dangerDetected =
      (v_ms > 0.5) && (telemetry.radar_distance <= AEB_CRITICAL_DISTANCE_M ||
                       ttc <= AEB_CRITICAL_TTC_S);

  if (dangerDetected) {
    nextState.aeb_active = true;
    cmd.aeb_brake_active = true;
    cmd.target_brake_pressure = 1.0; // %100 tam acil fren basıncı

    if (telemetry.is_gas_pressed) {
      nextState.driver_override = true;
      cmd.throttle_override = true;
    } else {
      nextState.driver_override = false;
      cmd.throttle_override = false;
    }
  } else {
    nextState.aeb_active = false;
    nextState.driver_override = false;
    cmd.aeb_brake_active = false;
    cmd.target_brake_pressure = 0.0;
    cmd.throttle_override = false;
  }
}

void AdasController::processACC(const TelemetryData &telemetry,
                                AdasDecisionState &nextState,
                                ActuatorCommand &cmd) {
  if (!telemetry.acc_enabled) {
    nextState.acc_active = false;
    nextState.acc_mode = "Kapali";
    nextState.acc_target_speed = telemetry.acc_set_speed;
    nextState.acc_safe_distance = 35.0;
    nextState.acc_throttle_demand = 0.0;
    nextState.acc_brake_demand = 0.0;
    cmd.acc_throttle_demand = 0.0;
    cmd.acc_brake_demand = 0.0;
    return;
  }

  // 1. Güvenlik Önceliği: Eğer AEB aktifse ACC anında beklemeye geçer
  if (nextState.aeb_active) {
    nextState.acc_active = false;
    nextState.acc_mode = "AEB Mudahale";
    nextState.acc_target_speed = telemetry.acc_set_speed;
    cmd.acc_throttle_demand = 0.0;
    cmd.acc_brake_demand = 0.0;
    return;
  }

  // 2. Sürücü Fren Müdahalesi: Sürücü frene dokunduğu an ACC emniyet için
  // devreden çıkar
  if (telemetry.is_brake_pressed) {
    nextState.acc_active = false;
    nextState.acc_mode = "Surucu Freni (Devre Disi)";
    nextState.acc_target_speed = telemetry.acc_set_speed;
    cmd.acc_throttle_demand = 0.0;
    cmd.acc_brake_demand = 0.0;
    return;
  }

  // 3. Sürücü Gaz Müdahalesi: Sürücü gaza basarken ACC devrede kalır ama aracı
  // hızlandırmayı sürücüye bırakır
  if (telemetry.is_gas_pressed) {
    nextState.acc_active = true;
    nextState.acc_mode = "Surucu Hizlaniyor";
    nextState.acc_target_speed = telemetry.acc_set_speed;
    cmd.acc_throttle_demand = 0.0;
    cmd.acc_brake_demand = 0.0;
    return;
  }

  nextState.acc_active = true;
  nextState.acc_target_speed = telemetry.acc_set_speed;

  // Dinamik Güvenli Takip Mesafesi (Time Headway kuralı: d_safe = d0 + v *
  // t_gap)
  double v_ms = telemetry.speed_kmh / 3.6;
  const double timeGapSeconds = 1.4;     // 1.4 saniyelik güvenli zaman aralığı
  const double standStillMarginM = 10.0; // Durma anı tampon payı
  double safeDistance = standStillMarginM + (v_ms * timeGapSeconds);
  nextState.acc_safe_distance = safeDistance;

  // Öndeki lider aracın radar menzilinde olup olmadığı (< 80 m)
  bool leadCarInRadar = (telemetry.radar_distance < 80.0);

  if (leadCarInRadar && telemetry.radar_distance <= (safeDistance + 5.0)) {
    // ÖNDEKİ ARACI TAKİP MODU (Distance Tracking)
    nextState.acc_mode = "Mesafe Takibi";
    double distanceDelta = telemetry.radar_distance - safeDistance;
    double relativeSpeed = telemetry.speed_kmh - telemetry.lead_car_speed_kmh;

    if (distanceDelta < -2.0 || relativeSpeed > 4.0) {
      // Öndeki araca çok yaklaşıldıysa yumuşak frenleme
      double brakeFactor =
          std::min(0.65, (std::abs(distanceDelta) / safeDistance) +
                             (relativeSpeed > 0 ? relativeSpeed / 40.0 : 0.0));
      cmd.acc_throttle_demand = 0.0;
      cmd.acc_brake_demand = std::max(0.15, brakeFactor);
    } else if (std::abs(distanceDelta) <= 2.0) {
      // Hızı öndeki lider aracın hızına sabitle
      if (telemetry.speed_kmh < telemetry.lead_car_speed_kmh - 1.5) {
        cmd.acc_throttle_demand = 0.25;
        cmd.acc_brake_demand = 0.0;
      } else if (telemetry.speed_kmh > telemetry.lead_car_speed_kmh + 1.5) {
        cmd.acc_throttle_demand = 0.0;
        cmd.acc_brake_demand = 0.15;
      } else {
        cmd.acc_throttle_demand = 0.08; // Sabit hız koruma
        cmd.acc_brake_demand = 0.0;
      }
    } else {
      // Güvenli mesafeye yumuşakça yaklaş
      cmd.acc_throttle_demand = 0.25;
      cmd.acc_brake_demand = 0.0;
    }
  } else {
    // YOL AÇIK: HIZ SABİTLEME MODU (Cruise Control)
    nextState.acc_mode = "Hiz Sabitleme";
    double speedDiff = nextState.acc_target_speed - telemetry.speed_kmh;

    if (speedDiff > 2.0) {
      // Hedef hıza çıkmak için ivmelen
      double throttleFactor = std::min(0.85, 0.20 + (speedDiff / 35.0));
      cmd.acc_throttle_demand = throttleFactor;
      cmd.acc_brake_demand = 0.0;
    } else if (speedDiff < -3.0) {
      // Hedef hız aşıldıysa kontrollü fren
      double brakeFactor = std::min(0.35, std::abs(speedDiff) / 25.0);
      cmd.acc_throttle_demand = 0.0;
      cmd.acc_brake_demand = brakeFactor;
    } else {
      // Hedef hızda seyir (yol sürtünmesini dengeleyecek kadar hafif gaz)
      cmd.acc_throttle_demand = 0.12;
      cmd.acc_brake_demand = 0.0;
    }
  }

  nextState.acc_throttle_demand = cmd.acc_throttle_demand;
  nextState.acc_brake_demand = cmd.acc_brake_demand;
}

void AdasController::processSuspension(const TelemetryData &telemetry,
                                       AdasDecisionState &nextState,
                                       ActuatorCommand &cmd) {
  // Engele varış süresi: t = obstacle_distance / v_ms
  // t < 1.5s ise; "Cukur" -> %20 Yumuşak, "Tumsek" -> %80 Sert
  double v_ms = telemetry.speed_kmh / 3.6;
  double t = 999.0;
  if (v_ms > 0.5 && telemetry.obstacle_type != "Yok") {
    t = telemetry.obstacle_distance / v_ms;
  }

  if (t < 1.5) {
    if (telemetry.obstacle_type == "Cukur") {
      nextState.suspension_stiffness = 20;
      nextState.suspension_mode = "Yumusak";
    } else if (telemetry.obstacle_type == "Tumsek") {
      nextState.suspension_stiffness = 80;
      nextState.suspension_mode = "Sert";
    } else {
      nextState.suspension_stiffness = 50;
      nextState.suspension_mode = "Normal";
    }
  } else {
    nextState.suspension_stiffness = 50;
    nextState.suspension_mode = "Normal";
  }

  cmd.target_suspension_stiffness = nextState.suspension_stiffness;
}

void AdasController::processTSR(const TelemetryData &telemetry,
                                AdasDecisionState &nextState) {
  // Hız sınırı %10 aşıldığında uyarı bayrağını kaldır
  double limit_esigi = telemetry.speed_limit * 1.10;
  if (telemetry.speed_kmh > limit_esigi) {
    nextState.speed_warning = true;
    nextState.speed_overshoot = telemetry.speed_kmh - telemetry.speed_limit;
  } else {
    nextState.speed_warning = false;
    nextState.speed_overshoot = 0.0;
  }
}

void AdasController::processLDW(const TelemetryData &telemetry,
                                AdasDecisionState &nextState) {
  if (telemetry.lane_offset < -0.5) {
    if (telemetry.left_signal) {
      nextState.ldw_warning = false;
      nextState.ldw_suppressed = true;
      nextState.ldw_status_text = "Bilincli Serit Degisimi";
    } else {
      nextState.ldw_warning = true;
      nextState.ldw_suppressed = false;
      nextState.ldw_status_text = "Serit Ihlali";
    }
  } else if (telemetry.lane_offset > 0.5) {
    if (telemetry.right_signal) {
      nextState.ldw_warning = false;
      nextState.ldw_suppressed = true;
      nextState.ldw_status_text = "Bilincli Serit Degisimi";
    } else {
      nextState.ldw_warning = true;
      nextState.ldw_suppressed = false;
      nextState.ldw_status_text = "Serit Ihlali";
    }
  } else {
    nextState.ldw_warning = false;
    nextState.ldw_suppressed = false;
    nextState.ldw_status_text = "Serit Ortasinda";
  }
}

void AdasController::logStateTransitions(const AdasDecisionState &prevState,
                                         const AdasDecisionState &newState,
                                         const TelemetryData &telemetry) {
  if (!prevState.aeb_active && newState.aeb_active) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1)
        << "KRİTİK: AEB Acil Fren Devrede! Radar Mesafesi: "
        << telemetry.radar_distance << "m";
    Logger::getInstance().log("ADAS", oss.str());
  }

  if (!prevState.driver_override && newState.driver_override) {
    Logger::getInstance().log(
        "ADAS", "KRİTİK: Sürücü Gaz Komutu Ezildi (Manual Override)!");
  }

  if (prevState.acc_mode != newState.acc_mode) {
    std::ostringstream oss;
    oss << "ACC Modu: " << newState.acc_mode;
    if (newState.acc_active) {
      oss << " (Hedef Hız: " << static_cast<int>(newState.acc_target_speed)
          << " km/s)";
    }
    Logger::getInstance().log("ADAS", oss.str());
  }

  if (prevState.suspension_stiffness != newState.suspension_stiffness) {
    Logger::getInstance().log(
        "ADAS", "Süspansiyon Sertliği Ayarlandı: %" +
                    std::to_string(newState.suspension_stiffness) + " (" +
                    newState.suspension_mode + ")");
  }

  if (!prevState.speed_warning && newState.speed_warning) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << "HIZ İHLALİ: Hız sınırı ("
        << telemetry.speed_limit << " km/s) aşıldı! (+"
        << newState.speed_overshoot << " km/s)";
    Logger::getInstance().log("ADAS", oss.str());
  }

  if (!prevState.ldw_warning && newState.ldw_warning) {
    Logger::getInstance().log("ADAS",
                              "ŞERİT İHLALİ: Sinyalsiz şerit dışına çıkıldı!");
  }

  if (!prevState.ldw_suppressed && newState.ldw_suppressed) {
    Logger::getInstance().log(
        "ADAS", "LDW: Sinyal Algılandı - Bilinçli Şerit Değişimi.");
  }
}

AdasDecisionState AdasController::getDecisionState() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_state;
}

ActuatorCommand AdasController::getLastActuatorCommand() {
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_lastCommand;
}

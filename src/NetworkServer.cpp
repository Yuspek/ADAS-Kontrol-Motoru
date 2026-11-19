#include "../include/NetworkServer.hpp"
#include "../include/Logger.hpp"
#include "../include/httplib.h"
#include "../include/json.hpp"
#include <filesystem>

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 3. API & Ağ Sorumlusu (Network Integration)
 * ============================================================================
 */

using json = nlohmann::json;

NetworkServer::NetworkServer(SensorSimulator& simulator, AdasController& controller, int port)
    : m_simulator(simulator), m_controller(controller), m_port(port) {
    m_server = std::make_unique<httplib::Server>();
}

NetworkServer::~NetworkServer() {
    stop();
}

void NetworkServer::start() {
    if (m_running) return;

    setupRoutes();
    m_running = true;

    m_serverThread = std::thread([this]() {
        Logger::getInstance().log("NETWORK", "HTTP Sunucusu Dinleniyor: http://localhost:" + std::to_string(m_port));
        m_server->listen("0.0.0.0", m_port);
    });
}

void NetworkServer::stop() {
    if (m_running && m_server) {
        m_server->stop();
        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
        m_running = false;
    }
}

void NetworkServer::setupRoutes() {
    // Çalışma dizinine göre statik Web klasörünü (./web, ../web, ../../web) otomatik bul ve bağla
    std::string webDir = "./web";
    if (std::filesystem::exists("./web/index.html")) {
        webDir = "./web";
    } else if (std::filesystem::exists("../web/index.html")) {
        webDir = "../web";
    } else if (std::filesystem::exists("../../web/index.html")) {
        webDir = "../../web";
    }
    m_server->set_mount_point("/", webDir);

    // CORS Ön-Kontrol (Preflight OPTIONS) desteği
    m_server->Options("/api/(.*)", [](const httplib::Request& /*req*/, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        res.status = 204;
    });

    // Araç telemetri, ADAS karar, aktüatör ve log verilerini sunan GET endpoint'i
    m_server->Get("/api/telemetry", [this](const httplib::Request& /*req*/, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        TelemetryData data = this->m_simulator.getData();
        AdasDecisionState decision = this->m_controller.getDecisionState();
        ActuatorCommand actuator = this->m_controller.getLastActuatorCommand();
        std::vector<std::string> recentLogs = Logger::getInstance().getRecentLogs(15);

        json sensorJson = {
            {"speed_kmh", data.speed_kmh},
            {"lead_car_speed_kmh", data.lead_car_speed_kmh},
            {"speed_limit", data.speed_limit},
            {"radar_distance", data.radar_distance},
            {"obstacle_type", data.obstacle_type},
            {"obstacle_distance", data.obstacle_distance},
            {"lane_offset", data.lane_offset},
            {"is_gas_pressed", data.is_gas_pressed},
            {"is_brake_pressed", data.is_brake_pressed},
            {"left_signal", data.left_signal},
            {"right_signal", data.right_signal},
            {"acc_enabled", data.acc_enabled},
            {"acc_set_speed", data.acc_set_speed}
        };

        json adasJson = {
            {"aeb_active", decision.aeb_active},
            {"driver_override", decision.driver_override},
            {"ttc_seconds", decision.ttc_seconds},
            {"acc_active", decision.acc_active},
            {"acc_mode", decision.acc_mode},
            {"acc_target_speed", decision.acc_target_speed},
            {"acc_safe_distance", decision.acc_safe_distance},
            {"acc_throttle_demand", decision.acc_throttle_demand},
            {"acc_brake_demand", decision.acc_brake_demand},
            {"suspension_stiffness", decision.suspension_stiffness},
            {"suspension_mode", decision.suspension_mode},
            {"speed_warning", decision.speed_warning},
            {"speed_overshoot", decision.speed_overshoot},
            {"ldw_warning", decision.ldw_warning},
            {"ldw_suppressed", decision.ldw_suppressed},
            {"ldw_status_text", decision.ldw_status_text}
        };

        json actuatorJson = {
            {"aeb_brake_active", actuator.aeb_brake_active},
            {"target_brake_pressure", actuator.target_brake_pressure},
            {"throttle_override", actuator.throttle_override},
            {"target_suspension_stiffness", actuator.target_suspension_stiffness},
            {"acc_throttle_demand", actuator.acc_throttle_demand},
            {"acc_brake_demand", actuator.acc_brake_demand}
        };

        // Hem Web Dashboard'un beklediği hiyerarşik yapıyı (sensor, adas, actuator, logs)
        // hem de geriye dönük uyumluluk için üst düzey alanları döndürür.
        json response = {
            {"sensor", sensorJson},
            {"adas", adasJson},
            {"actuator", actuatorJson},
            {"logs", recentLogs},
            {"speed_kmh", data.speed_kmh},
            {"speed_limit", data.speed_limit},
            {"radar_distance", data.radar_distance},
            {"obstacle_type", data.obstacle_type},
            {"obstacle_distance", data.obstacle_distance},
            {"lane_offset", data.lane_offset},
            {"is_gas_pressed", data.is_gas_pressed},
            {"is_brake_pressed", data.is_brake_pressed},
            {"left_signal", data.left_signal},
            {"right_signal", data.right_signal},
            {"aeb_active", decision.aeb_active},
            {"driver_override", decision.driver_override},
            {"suspension_stiffness", decision.suspension_stiffness},
            {"suspension_mode", decision.suspension_mode},
            {"speed_warning", decision.speed_warning},
            {"speed_overshoot", decision.speed_overshoot},
            {"ldw_warning", decision.ldw_warning},
            {"ldw_suppressed", decision.ldw_suppressed},
            {"ldw_status_text", decision.ldw_status_text}
        };

        res.set_content(response.dump(), "application/json");
    });

    // Web arayüzünden gelen araç kontrol ve simülasyon test komutlarını karşılayan POST endpoint'i
    m_server->Post("/api/control", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");

        try {
            json requestJson = json::parse(req.body);
            if (!requestJson.is_object()) {
                res.status = 400;
                res.set_content("Gecersiz istek: JSON nesnesi bekleniyor.", "text/plain");
                return;
            }

            // 1. Gaz pedalı (W tuşu) komutu
            if (requestJson.contains("gas")) {
                if (!requestJson["gas"].is_boolean()) {
                    res.status = 400;
                    res.set_content("Gecersiz gaz komutu.", "text/plain");
                    return;
                }
                m_simulator.setGasPressed(requestJson["gas"].get<bool>());
            }

            // 2. Fren pedalı (Space tuşu) komutu
            if (requestJson.contains("brake")) {
                if (!requestJson["brake"].is_boolean()) {
                    res.status = 400;
                    res.set_content("Gecersiz fren komutu.", "text/plain");
                    return;
                }
                m_simulator.setBrakePressed(requestJson["brake"].get<bool>());
            }

            // 3. Sol sinyal komutu
            if (requestJson.contains("left_signal")) {
                if (!requestJson["left_signal"].is_boolean()) {
                    res.status = 400;
                    res.set_content("Gecersiz sol sinyal komutu.", "text/plain");
                    return;
                }
                m_simulator.setLeftSignal(requestJson["left_signal"].get<bool>());
            }

            // 4. Sağ sinyal komutu
            if (requestJson.contains("right_signal")) {
                if (!requestJson["right_signal"].is_boolean()) {
                    res.status = 400;
                    res.set_content("Gecersiz sag sinyal komutu.", "text/plain");
                    return;
                }
                m_simulator.setRightSignal(requestJson["right_signal"].get<bool>());
            }

            // 5. Manuel Yol Anomalisi (Çukur / Tümsek) oluşturma komutu
            if (requestJson.contains("spawn_obstacle")) {
                if (!requestJson["spawn_obstacle"].is_string()) {
                    res.status = 400;
                    res.set_content("Gecersiz engel tipi komutu.", "text/plain");
                    return;
                }
                double dist = 40.0;
                if (requestJson.contains("distance") && requestJson["distance"].is_number()) {
                    dist = requestJson["distance"].get<double>();
                }
                m_simulator.spawnObstacle(requestJson["spawn_obstacle"].get<std::string>(), dist);
            }

            // 6. Manuel Hız Sınırı Tabelası (TSR) değiştirme komutu
            if (requestJson.contains("set_speed_limit")) {
                if (!requestJson["set_speed_limit"].is_number_integer()) {
                    res.status = 400;
                    res.set_content("Gecersiz hiz siniri komutu.", "text/plain");
                    return;
                }
                m_simulator.setSpeedLimit(requestJson["set_speed_limit"].get<int>());
            }

            // 7. ACC (Adaptif Hız Sabitleyici) Açma / Kapatma komutu
            if (requestJson.contains("acc_enabled")) {
                if (!requestJson["acc_enabled"].is_boolean()) {
                    res.status = 400;
                    res.set_content("Gecersiz acc_enabled komutu.", "text/plain");
                    return;
                }
                m_simulator.setAccEnabled(requestJson["acc_enabled"].get<bool>());
            }

            // 8. ACC Hedef Hız Ayarı komutu
            if (requestJson.contains("acc_set_speed")) {
                if (!requestJson["acc_set_speed"].is_number()) {
                    res.status = 400;
                    res.set_content("Gecersiz acc_set_speed komutu.", "text/plain");
                    return;
                }
                m_simulator.setAccSetSpeed(requestJson["acc_set_speed"].get<double>());
            }

            // 9. Simülasyonu başlangıç durumuna sıfırlama komutu
            if (requestJson.contains("reset")) {
                if (requestJson["reset"].is_boolean() && requestJson["reset"].get<bool>()) {
                    m_simulator.resetState();
                }
            }

            json okRes = {{"status", "ok"}, {"message", "Kontroller guncellendi"}};
            res.set_content(okRes.dump(), "application/json");
        } catch (const json::parse_error&) {
            res.status = 400;
            res.set_content("Gecersiz JSON verisi.", "text/plain");
        }
    });
}

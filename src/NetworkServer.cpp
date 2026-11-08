#include "NetworkServer.hpp"
#include "Logger.hpp"
#include "httplib.h"
#include "json.hpp"

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
    // Statik Web Dosyaları (./web)
    m_server->set_mount_point("/", "./web");

    //Araç telemetri verilerini sunmak için GET endpoint'ini tanımlar.
    m_server->Get("/api/telemetry", [this](const httplib::Request& req, httplib::Response& res){

    //Sensör simülatöründen aracın güncel telemetri verilerini alır.
    TelemetryData data = this->m_simulator.getData();

    //ADAS kontrolcüsünden güncel karar durumlarını alır.
    AdasDecisionState decision = this->m_controller.getDecisionState();

    //Sensör simülatöründen gelen verileri JSON nesnesine aktarır.
    json response = {
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
    //Hazırlanan JSON verisini HTTP cevabı olarak istemciye gönderir.
    res.set_content(response.dump(), "application/json");

    });

    // Web arayüzünden gelen araç kontrol komutlarını karşılamak için POST endpoint'ini tanımlar.
    m_server->Post("/api/control", [this](const httplib::Request& req, httplib::Response& res){

            try{
            //1. Web arayüzünden gelen JSON verisini ayrıştırır.
            json requestJson = json::parse(req.body); 
            // Gelen verinin bir JSON nesnesi olup olmadığını kontrol eder.
            if(!requestJson.is_object()){
                res.status = 400;
                res.set_content("Geçersiz istek: JSON nesnesi bekleniyor.", "text/plain");
                return;
            }
            // 2. Gaz komutunun gelip gelmediğini kontrol eder.
            if(requestJson.contains("gas")){
                // Gaz komutunu boolean (true/false) olarak kontrol eder.
                if(!requestJson["gas"].is_boolean()){
                    res.status = 400;
                    res.set_content("Geçersiz gaz komutu.", "text/plain");
                    return;
                }

                m_simulator.setGasPressed(requestJson["gas"].get<bool>());
            }
            // Fren komutunun gelip gelmediğini kontrol eder.
            if(requestJson.contains("brake")){
                
                if(!requestJson["brake"].is_boolean()){
                    res.status = 400;
                    res.set_content("Geçersiz fren komutu.", "text/plain");
                    return;
                }

                m_simulator.setBrakePressed(requestJson["brake"].get<bool>());
            }
            // Sol sinyal komutunun gelip gelmediğini kontrol eder.
            if(requestJson.contains("left_signal")){
                
                if(!requestJson["left_signal"].is_boolean()){
                    res.status = 400;
                    res.set_content("Geçersiz sol sinyal komutu.", "text/plain");
                    return;
                }

                m_simulator.setLeftSignal(requestJson["left_signal"].get<bool>());
            }
            // Sağ sinyal komutunun gelip gelmediğini kontrol eder.

            if(requestJson.contains("right_signal")){
                
                if(!requestJson["right_signal"].is_boolean()){
                    res.status = 400;
                    res.set_content("Geçersiz sağ sinyal komutu.", "text/plain");
                    return;
                }

                m_simulator.setRightSignal(requestJson["right_signal"].get<bool>());
            }
            res.set_content("Kontroller güncellendi", "text/plain");



        }catch(const json::parse_error& e){

            //JSON verisi ayrıştırılamazsa HTTP 400 hata kodu gönderir.
            res.status = 400;

            //İstemciye JSON verisinin hatalı olduğunu bildirir.
            res.set_content("Geçersiz JSON verisi.", "text/plain");
        }

        




    });
}

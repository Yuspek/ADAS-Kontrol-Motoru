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

    // TODO (Geliştirici 3): 
    // 1. GET /api/telemetry route'unu yazınız. SensorSimulator ve AdasController verilerini JSON'a çeviriniz.
    // 2. POST /api/control route'unu yazınız. Arayüzden gelen "gas", "brake", "left_signal" vb. parametreleri işleyiniz.
}

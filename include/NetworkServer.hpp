#ifndef NETWORK_SERVER_HPP
#define NETWORK_SERVER_HPP

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 3. API & Ağ Sorumlusu (Network Integration)
 * ============================================================================
 * Sorumluluklar:
 * - cpp-httplib ve nlohmann/json başlık dosyalarıyla C++ sunucusu kurmak.
 * - GET /api/telemetry endpoint'i ile telemetri ve karar verilerini JSON döndürmek.
 * - POST /api/control endpoint'i ile Web Arayüzden gelen tuş/komut girdilerini karşılamak.
 */

#include "SensorSimulator.hpp"
#include "AdasController.hpp"
#include <memory>
#include <thread>
#include <atomic>

namespace httplib {
    class Server;
}

class NetworkServer {
public:
    NetworkServer(SensorSimulator& simulator, AdasController& controller, int port = 8080);
    ~NetworkServer();

    void start();
    void stop();

private:
    SensorSimulator& m_simulator;
    AdasController& m_controller;
    int m_port;

    std::unique_ptr<httplib::Server> m_server;
    std::thread m_serverThread;
    std::atomic<bool> m_running{false};

    void setupRoutes();
};

#endif // NETWORK_SERVER_HPP

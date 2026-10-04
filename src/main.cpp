#include "SensorSimulator.hpp"
#include "AdasController.hpp"
#include "NetworkServer.hpp"
#include "Logger.hpp"

#include <iostream>
#include <chrono>
#include <thread>

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 4. Sistem Entegratörü (AI & Frontend)
 * ============================================================================
 */

int main() {
    std::cout << "=====================================================" << std::endl;
    std::cout << "   OTONOM ARAÇ SIL KARAR MOTORU (ADAS v2.0)          " << std::endl;
    std::cout << "=====================================================" << std::endl;

    // 1. Modül Nesnelerinin Oluşturulması
    SensorSimulator simulator;
    AdasController controller(simulator);
    NetworkServer server(simulator, controller, 8080);

    // 2. HTTP Sunucunun Başlatılması
    server.start();

    std::cout << "[BİLGİ] Web Dashboard Adresi: http://localhost:8080" << std::endl;

    // 3. Ana Simülasyon Döngüsü (20 Hz -> dt = 0.05s)
    double dt = 0.05;
    while (true) {
        simulator.update(dt);
        controller.processLogic(dt);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    server.stop();
    return 0;
}

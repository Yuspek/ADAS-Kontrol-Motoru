#include "SensorSimulator.hpp"

/**
 * ============================================================================
 * GÖREVLİ EKİP ÜYESİ: 1. Simülasyon Geliştirici (Data Generator)
 * ============================================================================
 */

SensorSimulator::SensorSimulator() {
    resetState();
}

void SensorSimulator::resetState() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.speed_kmh = 60.0;
    m_data.speed_limit = 70;
    m_data.radar_distance = 50.0;
    m_data.obstacle_type = "Yok";
    m_data.obstacle_distance = 100.0;
    m_data.lane_offset = 0.0;
    m_data.is_gas_pressed = false;
    m_data.is_brake_pressed = false;
    m_data.left_signal = false;
    m_data.right_signal = false;
}

void SensorSimulator::update(double dt_seconds) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // TODO (Geliştirici 1): 
    // 1. m_data.is_gas_pressed durumuna göre aracın hızını ivmelendiriniz (ör. +12 km/s^2).
    // 2. Fren basılı ise hızı düşürünüz, basılı değilse sürtünmeyle hızı yavaşlatınız.
    // 3. m_data.radar_distance ve m_data.obstacle_distance mesafelerini hız ile azaltınız.
    // 4. Şerit sapmasını (m_data.lane_offset) sinyal durumlarına göre güncelleyiniz.
}

TelemetryData SensorSimulator::getData() {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_data;
}

void SensorSimulator::setGasPressed(bool pressed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.is_gas_pressed = pressed;
}

void SensorSimulator::setBrakePressed(bool pressed) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.is_brake_pressed = pressed;
}

void SensorSimulator::setLeftSignal(bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.left_signal = active;
    if (active) m_data.right_signal = false;
}

void SensorSimulator::setRightSignal(bool active) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.right_signal = active;
    if (active) m_data.left_signal = false;
}

void SensorSimulator::spawnObstacle(const std::string& type, double distance) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.obstacle_type = type;
    m_data.obstacle_distance = distance;
}

void SensorSimulator::setSpeedLimit(int limit) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.speed_limit = limit;
}

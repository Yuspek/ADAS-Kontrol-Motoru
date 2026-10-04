cmake_minimum_required(VERSION 3.14)
project(ADAS_Decision_Engine VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header Dizinleri
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Kaynak Dosyalar
set(SOURCES
    src/main.cpp
    src/SensorSimulator.cpp
    src/AdasController.cpp
    src/NetworkServer.cpp
    src/Logger.cpp
)

# Çalıştırılabilir Hedef
add_executable(AdasDecisionEngine ${SOURCES})

# Windows Socket Kütüphaneleri (cpp-httplib için gereklidir)
if(WIN32)
    target_link_libraries(AdasDecisionEngine PRIVATE ws2_32 iphlpapi)
endif()

# Thread Kütüphanesi
find_package(Threads REQUIRED)
target_link_libraries(AdasDecisionEngine PRIVATE Threads::Threads)

cmake_minimum_required(VERSION 3.15)
project(esp32_irrigacao LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -DSERIAL_MOCK=ON  -> sem Arduino: você digita os quadros no terminal
option(SERIAL_MOCK "Usa a SerialMock (teclado) em vez da porta serial" OFF)

add_executable(ESP32Mock
src/main.cpp
src/serial/serial.cpp
src/esp32.cpp
src/esp32.h
src/utils/utils.h
src/utils/utils.cpp
src/serial/serialMock.cpp
src/serial/serialMock.h
src/mqtt/mqtt.cpp
src/mqtt/mqtt.h
src/protocolo/protocolo.cpp
src/protocolo/protocolo.h
)

target_include_directories(ESP32Mock PRIVATE src)

if(SERIAL_MOCK)
    target_compile_definitions(ESP32Mock PRIVATE USAR_SERIAL_MOCK)
endif()

target_link_libraries(ESP32Mock
    PRIVATE
    paho-mqttpp3
    paho-mqtt3as
)

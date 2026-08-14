cmake_minimum_required(VERSION 3.10)
project(esp32_cam_streamer_cpp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(Threads REQUIRED)

set(SOURCES
    src/main.cpp
    src/mjpeg_client.cpp
)

add_executable(esp32_cam_streamer ${SOURCES})

target_link_libraries(esp32_cam_streamer PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(esp32_cam_streamer PRIVATE ws2_32)
endif()

message(STATUS "[+] Configured esp32_cam_streamer for ${CMAKE_SYSTEM_NAME}")

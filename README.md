# 📷 ESP32-CAM MJPEG Streamer & Snapshot App (Rust, C++ & ESP32 Firmware)

A high-performance, cross-platform video streaming, frame capture, and ESP32 hardware package for **ESP32-CAM** modules, built in **Rust**, **C++**, and **Arduino/ESP32 C++**.

---

## 📁 Project Directory Structure (`/home/computer/esp32`)

```
/home/computer/esp32/
├── README.md                           <-- Main project guide
├── firmware/                           <-- ESP32-CAM Hardware Firmware
│   └── esp32_cam_firmware/             <-- Arduino / PlatformIO firmware package
│       ├── esp32_cam_firmware.ino      <-- Main sketch (.ino)
│       ├── camera_pins.h               <-- Camera pin mapping header (.h)
│       ├── app_httpd.h                 <-- Server interface header (.h)
│       ├── app_httpd.cpp               <-- Streamer implementation (.cpp)
│       └── README_FLASHING.md          <-- Wiring & Flashing guide
├── rust/                               <-- Rust Host App
│   ├── Cargo.toml
│   └── src/
│       └── main.rs                     <-- egui GUI & Ctrl+X listener
├── cpp/                                <-- C++ Host App
│   ├── CMakeLists.txt
│   ├── build/
│   │   └── esp32_cam_streamer          <-- Compiled C++ binary
│   ├── include/
│   │   ├── stb_image.h
│   │   └── stb_image_write.h
│   └── src/
│       ├── main.cpp                    <-- Terminal & socket loop
│       ├── mjpeg_client.hpp
│       └── mjpeg_client.cpp
└── simulator/                          <-- Local ESP32 stream simulator
    ├── venv/                           <-- Simulator Python virtual environment
    └── esp32_simulator.py              <-- MJPEG HTTP stream server on :8080
```

---

## ⚡ Quick Start: Hardware Setup

1. Open [`/home/computer/esp32/firmware/esp32_cam_firmware`](file:///home/computer/esp32/firmware/esp32_cam_firmware) in **Arduino IDE**.
2. Open [`esp32_cam_firmware.ino`](file:///home/computer/esp32/firmware/esp32_cam_firmware/esp32_cam_firmware.ino) and enter your WiFi `SSID` and `Password`.
3. Select board **AI Thinker ESP32-CAM** and flash using an FTDI USB-to-TTL programmer.
4. Note your ESP32's assigned IP address from the Serial Monitor (e.g., `http://192.168.1.105:81/stream`).

---

## 💻 Running the Host Apps

### Rust GUI App:
```bash
/home/computer/esp32/rust/target/release/esp32-cam-streamer
```
- Enter your ESP32 IP address and press **`Ctrl + X`** to capture a snapshot!

### C++ App:
```bash
/home/computer/esp32/cpp/build/esp32_cam_streamer 192.168.1.105:81 /stream
```
- Stream live video and press **`Ctrl + X`** in terminal anytime to capture snapshot images directly into `/home/computer/pictures/`!

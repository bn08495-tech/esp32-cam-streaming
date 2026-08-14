# 🔌 ESP32-CAM Firmware Flashing & Hardware Guide

This directory contains the complete C++/Arduino source code needed to flash your **ESP32-CAM** board so it streams real-time MJPEG video over WiFi to your Rust and C++ host apps.

---

## 📂 Firmware Package Contents

```
/home/computer/esp32/firmware/esp32_cam_firmware/
├── esp32_cam_firmware.ino       <-- Main Arduino sketch (Edit WiFi SSID & Pass here)
├── camera_pins.h                <-- Pin maps for AI-Thinker and other ESP32-CAM boards
├── app_httpd.h                  <-- Camera HTTP Server header
├── app_httpd.cpp                <-- MJPEG stream & snapshot HTTP server implementation
└── README_FLASHING.md           <-- This guide
```

---

## 🛠️ Step 1: FTDI USB-to-Serial Wiring

Because the ESP32-CAM does not have a built-in USB port, use an **FTDI USB-to-TTL Serial adapter** to connect it to your computer.

| FTDI Adapter Pin | ESP32-CAM Pin | Notes |
| :--- | :--- | :--- |
| **5V / 3.3V** | **5V / 3V3** | Use 5V if possible for stable power |
| **GND** | **GND** | Connect ground |
| **TX** | **U0R (GPIO 3)** | Transmit to Receive |
| **RX** | **U0T (GPIO 1)** | Receive to Transmit |
| **GND** | **GPIO 0** | ⚠️ **Connect GPIO 0 to GND ONLY while flashing!** |

---

## 💻 Step 2: Arduino IDE Setup

1. Open **Arduino IDE**.
2. Go to **File -> Preferences** and add this URL to **Additional Boards Manager URLs**:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools -> Board -> Boards Manager**, search for `esp32` and click **Install**.
4. Open `esp32_cam_firmware.ino`.
5. Update your WiFi credentials in lines 25-26:
   ```cpp
   const char* ssid     = "YOUR_WIFI_SSID";
   const char* password = "YOUR_WIFI_PASSWORD";
   ```
6. Select Board settings in **Tools** menu:
   - **Board**: `AI Thinker ESP32-CAM`
   - **CPU Frequency**: `240MHz (WiFi/BT)`
   - **Flash Frequency**: `80MHz`
   - **Flash Mode**: `QIO`
   - **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)`
   - **PSRAM**: `Enabled` (if your board has PSRAM chip)
   - **Port**: Select your FTDI Serial COM/TTY port (e.g. `/dev/ttyUSB0` or `COM3`).

---

## ⚡ Step 3: Flash Firmware

1. Verify **GPIO 0** is jumpered to **GND**.
2. Press the **RST (Reset)** button on the back of the ESP32-CAM board.
3. Click the **Upload** button (Right Arrow) in Arduino IDE.
4. Wait for the upload to complete (*Writing at 0x00010000... 100%*).
5. **Disconnect the GPIO 0 jumper from GND**.
6. Press the **RST (Reset)** button to boot into normal streaming mode.
7. Open **Tools -> Serial Monitor** at `115200 baud` to view your board's assigned IP address:
   ```
   ✅ WiFi Connected!
   IP Address: http://192.168.1.105

   📹 Stream URL   : http://192.168.1.105:81/stream
   📸 Snapshot URL : http://192.168.1.105/capture
   ```

---

## 🎯 Step 4: Stream to Rust & C++ Desktop Apps

Now launch your host apps created in `/home/computer/esp32`:

### Using the Rust App:
```bash
/home/computer/esp32/rust/target/release/esp32-cam-streamer
```
1. Type your ESP32-CAM IP (e.g. `192.168.1.105:81`) into the IP box.
2. Click **Connect**.
3. Press **`Ctrl + X`** to capture a snapshot!

### Using the C++ App:
```bash
/home/computer/esp32/cpp/build/esp32_cam_streamer 192.168.1.105:81 /stream
```
Press **`Ctrl + X`** in terminal anytime to capture snapshot images to `/home/computer/pictures/`!

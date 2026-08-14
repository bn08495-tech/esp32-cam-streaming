/*
 * ESP32-CAM MJPEG Video Streamer & Snapshot Firmware
 * Target Hardware: AI-Thinker ESP32-CAM (OV2640 Camera)
 *
 * Flashing Instructions:
 * 1. Open this folder in Arduino IDE or PlatformIO.
 * 2. Select Board: "AI Thinker ESP32-CAM"
 * 3. Select Partition Scheme: "Huge APP (3MB No OTA/1MB SPIFFS)"
 * 4. Enter your WiFi SSID and Password below.
 * 5. Connect FTDI Programmer (GND to IO0 for flashing).
 * 6. Upload sketch, remove IO0 jumper, press RESET button.
 */

#include "esp_camera.h"
#include <WiFi.h>

// Select Camera Model
#define CAMERA_MODEL_AI_THINKER // Standard AI-Thinker ESP32-CAM board
//#define CAMERA_MODEL_ESP_EYE
//#define CAMERA_MODEL_WROVER_KIT
//#define CAMERA_MODEL_M5STACK_PSRAM

#include "camera_pins.h"
#include "app_httpd.h"

// ================================================
// REPLACE WITH YOUR WIFI CREDENTIALS
// ================================================
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  Serial.println("=========================================");
  Serial.println("  ESP32-CAM MJPEG Streamer Firmware      ");
  Serial.println("=========================================");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // Check if board has PSRAM (External SPI RAM)
  if (psramFound()) {
    Serial.println("[+] PSRAM detected! Setting high quality mode (VGA/SVGA).");
    config.frame_size = FRAMESIZE_VGA;   // 640x480 resolution
    config.jpeg_quality = 10;            // 0-63 (lower = higher quality)
    config.fb_count = 2;                 // Double buffering for smooth FPS
  } else {
    Serial.println("[!] No PSRAM found. Setting lower resolution (CIF).");
    config.frame_size = FRAMESIZE_CIF;   // 400x296 resolution
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  // Camera Initialization
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  if (s != NULL) {
    s->set_brightness(s, 0);     // -2 to 2
    s->set_contrast(s, 0);       // -2 to 2
    s->set_saturation(s, 0);     // -2 to 2
    s->set_vflip(s, 0);          // 0 = normal, 1 = flipped vertical
    s->set_hmirror(s, 0);        // 0 = normal, 1 = mirrored horizontal
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("✅ WiFi Connected!");
  Serial.print("IP Address: http://");
  Serial.println(WiFi.localIP());

  // Start MJPEG & Snapshot HTTP Server
  startCameraServer();

  Serial.println("\n---------------------------------------------------------");
  Serial.print("📹 Stream URL   : http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream  (or http://<IP>/stream)");
  Serial.print("📸 Snapshot URL : http://");
  Serial.print(WiFi.localIP());
  Serial.println("/capture");
  Serial.println("---------------------------------------------------------");
  Serial.println("Ready to stream to Rust & C++ Desktop Apps!");
}

void loop() {
  // Main server runs asynchronously in background tasks
  delay(10000);
}

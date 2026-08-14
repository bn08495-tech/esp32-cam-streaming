/*
 * ESP32-CAM MJPEG Video Streamer & Snapshot Firmware
 * Features:
 *  1. Live MJPEG Streaming & Snapshot Server
 *  2. Remote Flash LED Toggle Control (GPIO 4)
 *  3. mDNS Auto-Discovery (http://esp32-cam.local)
 *  4. ArduinoOTA Wireless Firmware Updates over WiFi
 *
 * Target Hardware: AI-Thinker ESP32-CAM (OV2640 Camera)
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

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
  Serial.println("  ESP32-CAM MJPEG Streamer + OTA + mDNS  ");
  Serial.println("=========================================");

  // Initialize Flash LED Pin
  #ifdef FLASH_LED_PIN
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW); // Start with Flash LED Off
  #endif

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

  // Setup mDNS Auto-Discovery (http://esp32-cam.local)
  if (MDNS.begin("esp32-cam")) {
    Serial.println("[+] mDNS responder started: http://esp32-cam.local");
  }

  // Setup ArduinoOTA Wireless Firmware Flashing
  ArduinoOTA.setHostname("esp32-cam");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else {
      type = "filesystem";
    }
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("[+] Wireless OTA updates ready!");

  // Start MJPEG & Snapshot HTTP Server
  startCameraServer();

  Serial.println("\n---------------------------------------------------------");
  Serial.print("📹 Stream URL    : http://");
  Serial.print(WiFi.localIP());
  Serial.println(":81/stream  (or http://esp32-cam.local:81/stream)");
  Serial.print("📸 Snapshot URL  : http://");
  Serial.print(WiFi.localIP());
  Serial.println("/capture");
  Serial.print("⚡ Flash Control : http://");
  Serial.print(WiFi.localIP());
  Serial.println("/flash");
  Serial.println("---------------------------------------------------------");
}

void loop() {
  // Handle Wireless OTA Update requests
  ArduinoOTA.handle();
  delay(10);
}

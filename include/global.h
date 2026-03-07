#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// ====== Giá trị cảm biến toàn cục ======
#define LED_GPIO 48
#define NEO_PIN 45
#define LED_COUNT 1 
extern float glob_temperature;
extern float glob_humidity;


#define MQ2_AO_PIN 4
#define MQ2_DO_PIN 5
#define MQ2_THRESHOLD 1800

// ====== Ngưỡng & mức nhiệt độ / độ ẩm (giá trị mặc định) ======
#define TEMP_COLD_THRESHOLD   24.0f
#define TEMP_HOT_THRESHOLD    32.0f

#define HUMI_DRY_THRESHOLD    30.0f
#define HUMI_HUMID_THRESHOLD  80.0f

enum TempLevel : uint8_t {
  TEMP_LEVEL_COLD = 0,
  TEMP_LEVEL_NORMAL,
  TEMP_LEVEL_HOT
};

enum HumiLevel : uint8_t {
  HUMI_LEVEL_DRY = 0,
  HUMI_LEVEL_OK,
  HUMI_LEVEL_HUMID
};

enum DisplayState : uint8_t {
  DISPLAY_STATE_NORMAL = 0,
  DISPLAY_STATE_WARNING,
  DISPLAY_STATE_CRITICAL
};

// Cấu hình nháy LED theo nhiệt độ (ms)
struct TempLedConfig {
  uint16_t on_ms;
  uint16_t off_ms;
};

// Cấu hình màu NeoPixel theo độ ẩm (RGB)
struct NeoColorConfig {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

// Các mức hiện tại, được temp_humi_monitor cập nhật
extern volatile uint8_t glob_temp_level;
extern volatile uint8_t glob_humi_level;
extern volatile uint8_t glob_display_state;

// Ngưỡng runtime có thể chỉnh từ WebUI
extern float tempColdThreshold;
extern float tempHotThreshold;
extern float humiDryThreshold;
extern float humiHumidThreshold;

// Cấu hình LED runtime
extern TempLedConfig tempLedConfig[3];
extern NeoColorConfig neoColorConfig[3];

// Cho phép bật/tắt LED từ Web UI
extern volatile bool glob_temp_led_enabled;
extern volatile bool glob_humi_led_enabled;

// ====== TinyML runtime result (cho CoreIoT & WebServer) ======
extern float tinyml_score;
extern float tinyml_accuracy;
extern bool  tinyml_pred_anomaly;
extern bool  tinyml_gt_anomaly;

// ====== WiFi / CoreIoT config ======
extern String WIFI_SSID;
extern String WIFI_PASS;
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;

// ====== WiFi AP dùng cho main_server_task ======
extern String ssid;
extern String password;

// ====== WiFi STA ======
extern String wifi_ssid;
extern String wifi_password;

// Cờ báo đã có Internet
extern bool isWifiConnected;

// Semaphore báo có internet 
extern SemaphoreHandle_t xBinarySemaphoreInternet;

// ====== Semaphore đồng bộ các task  ======
// Được temp_humi_monitor "give" khi mức nhiệt thay đổi → Task LED dùng.
extern SemaphoreHandle_t xTempLedSemaphore;
// Được temp_humi_monitor "give" khi mức ẩm thay đổi → Task NeoPixel dùng.
extern SemaphoreHandle_t xHumiNeoSemaphore;

#endif

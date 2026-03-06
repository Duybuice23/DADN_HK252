#include "global.h"

// ====== Biến toàn cục cảm biến ======
float glob_temperature = 0.0f;
float glob_humidity    = 0.0f;

// Mức nhiệt/ẩm & trạng thái hiển thị
volatile uint8_t glob_temp_level     = TEMP_LEVEL_NORMAL;
volatile uint8_t glob_humi_level     = HUMI_LEVEL_OK;
volatile uint8_t glob_display_state  = DISPLAY_STATE_NORMAL;

// ====== Ngưỡng runtime có thể chỉnh từ WebUI ======
float tempColdThreshold  = TEMP_COLD_THRESHOLD;
float tempHotThreshold   = TEMP_HOT_THRESHOLD;
float humiDryThreshold   = HUMI_DRY_THRESHOLD;
float humiHumidThreshold = HUMI_HUMID_THRESHOLD;

// ====== Cấu hình nháy LED theo nhiệt độ (ms) ======
TempLedConfig tempLedConfig[3] = {
    {1000, 1000}, // LẠNH
    {200, 800},  // BÌNH THƯỜNG
    {150, 150}   // NÓNG 
};

// ====== Màu NeoPixel theo độ ẩm (RGB) ======
NeoColorConfig neoColorConfig[3] = {
    {0, 0, 255}, // DRY   → xanh dương
    {0, 255, 0}, // OK    → xanh lá
    {255, 0, 0}  // HUMID → đỏ
};

// ====== Bật/tắt hiển thị LED từ WebUI ======
volatile bool glob_temp_led_enabled = true;
volatile bool glob_humi_led_enabled = true;

// ====== TinyML runtime result ======
float tinyml_score        = 0.0f;
float tinyml_accuracy     = 0.0f;
bool  tinyml_pred_anomaly = false;
bool  tinyml_gt_anomaly   = false;

// ====== WiFi / CoreIoT config ======
String WIFI_SSID;
String WIFI_PASS;
String CORE_IOT_TOKEN  = "b3y9zdp1ewheh4v0469i";
String CORE_IOT_SERVER = "thingsboard.cloud";
String CORE_IOT_PORT   = "1883";

// ====== WiFi AP mặc định ======
String ssid     = "ESP32 LOCAL";
String password = "12345678";

// ====== WiFi STA ======
String wifi_ssid     = "Yen Nguyen A";
String wifi_password = "123456789";

// Cờ báo đã có Internet
bool isWifiConnected = false;

// ====== Semaphore cho Internet ) ======
SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();

// ====== Semaphore cho các task ======
SemaphoreHandle_t xTempLedSemaphore = xSemaphoreCreateBinary();
SemaphoreHandle_t xHumiNeoSemaphore = xSemaphoreCreateBinary();

#include "task_handler.h"
#include "global.h"
#include "task_webserver.h"

static uint16_t clampMs(uint16_t value)
{
  if (value < 10) return 10;
  if (value > 10000) return 10000;
  return value;
}

static bool parseHexColor(const String &colorHex, uint8_t &r, uint8_t &g, uint8_t &b)
{
  if (colorHex.length() != 7 || colorHex.charAt(0) != '#')
    return false;

  char buf[3];
  buf[2] = '\0';

  buf[0] = colorHex.charAt(1);
  buf[1] = colorHex.charAt(2);
  r = strtoul(buf, nullptr, 16);

  buf[0] = colorHex.charAt(3);
  buf[1] = colorHex.charAt(4);
  g = strtoul(buf, nullptr, 16);

  buf[0] = colorHex.charAt(5);
  buf[1] = colorHex.charAt(6);
  b = strtoul(buf, nullptr, 16);

  return true;
}

static String rgbToHex(uint8_t r, uint8_t g, uint8_t b)
{
  char buf[8];
  snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
  return String(buf);
}

void handleWebSocketMessage(String message)
{
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error)
  {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.c_str());
    return;
  }

  String page = doc["page"] | "";
  JsonObject value = doc["value"].isNull() ? JsonObject() : doc["value"].as<JsonObject>();

  // =========== DEVICE: Bật/tắt relay/LED ===========
  if (page == "device")
  {
    String name   = value["name"] | "";
    String status = value["status"] | "";
    int    gpio   = value["gpio"]   | -1;
    bool isOn = (status == "ON");
    int targetGpio = gpio;

    if (name == "LED1")
    {
      glob_temp_led_enabled = isOn;
      targetGpio = LED_GPIO;
    }
    else if (name == "LED2")
    {
      glob_humi_led_enabled = isOn;
      targetGpio = NEO_PIN;

      if (xHumiNeoSemaphore != nullptr)
      {
        xSemaphoreGive(xHumiNeoSemaphore);
      }
    }
    else if (name == "LED Gas")
    {
      glob_gas_led_enabled = isOn;
      targetGpio = GAS_LED_GPIO;
    }

    if (targetGpio >= 0)
    {
      pinMode(targetGpio, OUTPUT);
      digitalWrite(targetGpio, isOn ? HIGH : LOW);
    }

    Serial.printf("[WebUI] Device %s (GPIO %d) -> %s\n",
                  name.c_str(), targetGpio, isOn ? "ON" : "OFF");
  }

  // =========== SETTING: Lưu WiFi / CoreIoT vào LittleFS ===========
  else if (page == "setting")
  {
    String WIFI_SSID_local  = value["ssid"]     | "";
    String WIFI_PASS_local  = value["password"] | "";
    String CORE_TOKEN_local = value["token"]    | "";
    String CORE_SERV_local  = value["server"]   | "";
    String CORE_PORT_local  = value["port"]     | "";

    WIFI_SSID      = WIFI_SSID_local;
    WIFI_PASS      = WIFI_PASS_local;
    CORE_IOT_TOKEN = CORE_TOKEN_local;
    CORE_IOT_SERVER= CORE_SERV_local;
    CORE_IOT_PORT  = CORE_PORT_local;

    Serial.println("Nhận cấu hình từ WebSocket:");
    Serial.println("SSID: " + WIFI_SSID);
    Serial.println("PASS: " + WIFI_PASS);
    Serial.println("TOKEN: " + CORE_IOT_TOKEN);
    Serial.println("SERVER: " + CORE_IOT_SERVER);
    Serial.println("PORT: " + CORE_IOT_PORT);

    // Gọi hàm lưu cấu hình
    Save_info_File(WIFI_SSID, WIFI_PASS, CORE_IOT_TOKEN, CORE_IOT_SERVER, CORE_IOT_PORT);

    // Phản hồi lại client
    String msg = "{\"status\":\"ok\",\"page\":\"setting_saved\"}";
    ws.textAll(msg);
  }

  // =========== THRESHOLD: Cập nhật ngưỡng nhiệt/ẩm ===========
  else if (page == "threshold")
  {
    float tCold  = value["tempCold"]  | tempColdThreshold;
    float tHot   = value["tempHot"]   | tempHotThreshold;
    float hDry   = value["humiDry"]   | humiDryThreshold;
    float hHumid = value["humiHumid"] | humiHumidThreshold;

    // Giới hạn độ ẩm 0–100
    if (hDry   < 0.0f)   hDry   = 0.0f;
    if (hDry   > 100.0f) hDry   = 100.0f;
    if (hHumid < 0.0f)   hHumid = 0.0f;
    if (hHumid > 100.0f) hHumid = 100.0f;

    // Đảm bảo thứ tự hợp lý cho nhiệt độ
    if (tCold > tHot)
    {
      float mid = (tCold + tHot) * 0.5f;
      tCold = mid - 0.5f;
      tHot  = mid + 0.5f;
    }

    // Đảm bảo thứ tự hợp lý cho độ ẩm
    if (hDry > hHumid)
    {
      float mid = (hDry + hHumid) * 0.5f;
      hDry   = mid - 1.0f;
      hHumid = mid + 1.0f;
      if (hDry < 0.0f)   hDry = 0.0f;
      if (hHumid > 100.0f) hHumid = 100.0f;
    }

    tempColdThreshold  = tCold;
    tempHotThreshold   = tHot;
    humiDryThreshold   = hDry;
    humiHumidThreshold = hHumid;

    Serial.println("Cập nhật ngưỡng nhiệt/ẩm từ WebUI:");
    Serial.printf("  TEMP_COLD = %.1f\n", tempColdThreshold);
    Serial.printf("  TEMP_HOT  = %.1f\n", tempHotThreshold);
    Serial.printf("  HUMI_DRY  = %.1f\n", humiDryThreshold);
    Serial.printf("  HUMI_HUMID= %.1f\n", humiHumidThreshold);

    ws.textAll("{\"page\":\"threshold_saved\"}");
  }

  // =========== LED_PATTERN: Cập nhật pattern blink nhiệt độ ===========
  else if (page == "led_pattern")
  {
    uint16_t coldOn    = clampMs(value["coldOn"]   | tempLedConfig[TEMP_LEVEL_COLD].on_ms);
    uint16_t coldOff   = clampMs(value["coldOff"]  | tempLedConfig[TEMP_LEVEL_COLD].off_ms);
    uint16_t normalOn  = clampMs(value["normalOn"] | tempLedConfig[TEMP_LEVEL_NORMAL].on_ms);
    uint16_t normalOff = clampMs(value["normalOff"]| tempLedConfig[TEMP_LEVEL_NORMAL].off_ms);
    uint16_t hotOn     = clampMs(value["hotOn"]    | tempLedConfig[TEMP_LEVEL_HOT].on_ms);
    uint16_t hotOff    = clampMs(value["hotOff"]   | tempLedConfig[TEMP_LEVEL_HOT].off_ms);

    tempLedConfig[TEMP_LEVEL_COLD].on_ms    = coldOn;
    tempLedConfig[TEMP_LEVEL_COLD].off_ms   = coldOff;
    tempLedConfig[TEMP_LEVEL_NORMAL].on_ms  = normalOn;
    tempLedConfig[TEMP_LEVEL_NORMAL].off_ms = normalOff;
    tempLedConfig[TEMP_LEVEL_HOT].on_ms     = hotOn;
    tempLedConfig[TEMP_LEVEL_HOT].off_ms    = hotOff;

    Serial.println("Cập nhật pattern LED nhiệt độ:");
    Serial.printf("  COLD   : %u / %u ms\n", coldOn, coldOff);
    Serial.printf("  NORMAL : %u / %u ms\n", normalOn, normalOff);
    Serial.printf("  HOT    : %u / %u ms\n", hotOn, hotOff);

    ws.textAll("{\"page\":\"led_pattern_saved\"}");
  }

  // =========== NEO_COLOR: Cập nhật màu NeoPixel ===========
  else if (page == "neo_color")
  {
    String dryHex   = value["dry"]   | "#0000FF";
    String okHex    = value["ok"]    | "#00FF00";
    String humidHex = value["humid"] | "#FF0000";

    uint8_t r, g, b;
    if (parseHexColor(dryHex, r, g, b))
    {
      neoColorConfig[HUMI_LEVEL_DRY].r = r;
      neoColorConfig[HUMI_LEVEL_DRY].g = g;
      neoColorConfig[HUMI_LEVEL_DRY].b = b;
    }

    if (parseHexColor(okHex, r, g, b))
    {
      neoColorConfig[HUMI_LEVEL_OK].r = r;
      neoColorConfig[HUMI_LEVEL_OK].g = g;
      neoColorConfig[HUMI_LEVEL_OK].b = b;
    }

    if (parseHexColor(humidHex, r, g, b))
    {
      neoColorConfig[HUMI_LEVEL_HUMID].r = r;
      neoColorConfig[HUMI_LEVEL_HUMID].g = g;
      neoColorConfig[HUMI_LEVEL_HUMID].b = b;
    }

    Serial.println("🌈 Cập nhật màu NeoPixel từ WebUI.");

    // Kích task NeoPixel cập nhật màu mới
    if (xHumiNeoSemaphore != nullptr)
      xSemaphoreGive(xHumiNeoSemaphore);

    ws.textAll("{\"page\":\"neo_color_saved\"}");
  }

  // =========== GET_CONFIG: Gửi toàn bộ cấu hình hiện tại về Web UI ===========
  else if (page == "get_config")
  {
    StaticJsonDocument<768> resp;
    resp["page"] = "config";
    JsonObject v = resp.createNestedObject("value");

    // Ngưỡng nhiệt/ẩm
    JsonObject thr = v.createNestedObject("thresholds");
    thr["tempCold"]  = tempColdThreshold;
    thr["tempHot"]   = tempHotThreshold;
    thr["humiDry"]   = humiDryThreshold;
    thr["humiHumid"] = humiHumidThreshold;

    // Pattern LED
    JsonObject lp = v.createNestedObject("ledPattern");
    lp["coldOn"]    = tempLedConfig[TEMP_LEVEL_COLD].on_ms;
    lp["coldOff"]   = tempLedConfig[TEMP_LEVEL_COLD].off_ms;
    lp["normalOn"]  = tempLedConfig[TEMP_LEVEL_NORMAL].on_ms;
    lp["normalOff"] = tempLedConfig[TEMP_LEVEL_NORMAL].off_ms;
    lp["hotOn"]     = tempLedConfig[TEMP_LEVEL_HOT].on_ms;
    lp["hotOff"]    = tempLedConfig[TEMP_LEVEL_HOT].off_ms;

    // Màu NeoPixel
    JsonObject neo = v.createNestedObject("neoColors");
    neo["dry"]   = rgbToHex(neoColorConfig[HUMI_LEVEL_DRY].r,
                            neoColorConfig[HUMI_LEVEL_DRY].g,
                            neoColorConfig[HUMI_LEVEL_DRY].b);
    neo["ok"]    = rgbToHex(neoColorConfig[HUMI_LEVEL_OK].r,
                            neoColorConfig[HUMI_LEVEL_OK].g,
                            neoColorConfig[HUMI_LEVEL_OK].b);
    neo["humid"] = rgbToHex(neoColorConfig[HUMI_LEVEL_HUMID].r,
                            neoColorConfig[HUMI_LEVEL_HUMID].g,
                            neoColorConfig[HUMI_LEVEL_HUMID].b);

    // Trạng thái thiết bị (cho nút gạt LED1, LED2, LED Gas)
    // Trạng thái thiết bị (cho nút gạt LED1, LED2)
    JsonArray devs = v.createNestedArray("devices");
    JsonObject d1 = devs.createNestedObject();
    d1["name"]   = "LED1";
    d1["gpio"]   = LED_GPIO;
    d1["status"] = glob_temp_led_enabled ? "ON" : "OFF";

    JsonObject d2 = devs.createNestedObject();
    d2["name"]   = "LED2";
    d2["gpio"]   = NEO_PIN;
    d2["status"] = glob_humi_led_enabled ? "ON" : "OFF";

    JsonObject d3 = devs.createNestedObject();
    d3["name"]   = "LED Gas";
    d3["gpio"]   = GAS_LED_GPIO;
    d3["status"] = glob_gas_led_enabled ? "ON" : "OFF";

    // Cấu hình WiFi/CoreIoT để pre-fill vào form Cài đặt
    JsonObject s = v.createNestedObject("settings");
    s["ssid"]     = WIFI_SSID;
    s["password"] = WIFI_PASS;
    s["token"]    = CORE_IOT_TOKEN;
    s["server"]   = CORE_IOT_SERVER;
    s["port"]     = CORE_IOT_PORT;

    String out;
    serializeJson(resp, out);
    Webserver_sendata(out);
  }

  // =========== RESET_FACTORY: Xóa file cấu hình & restart ===========
  else if (page == "reset_factory")
  {
    Serial.println("Error:Yêu cầu Reset Factory từ Web UI");
    Delete_info_File();
  }
}




// #include "task_handler.h"
// #include "global.h"
// #include "task_webserver.h"

// void handleWebSocketMessage(String message)
// {
//   StaticJsonDocument<512> doc;
//   DeserializationError error = deserializeJson(doc, message);
//   if (error)
//   {
//     Serial.print("deserializeJson() failed: ");
//     Serial.println(error.c_str());
//     return;
//   }

//   String page = doc["page"] | "";
//   JsonObject value = doc["value"].isNull() ? JsonObject() : doc["value"].as<JsonObject>();

//   if (page == "device")
//   {
//     String name   = value["name"] | "";
//     String status = value["status"] | "";
//     int    gpio   = value["gpio"]   | -1;
//     bool isOn = (status == "ON");

//     if (name == "LED1")
//     {
//       glob_temp_led_enabled = isOn;
//     }
//     else if (name == "LED2")
//     {
//       glob_humi_led_enabled = isOn;
//     }

//     if (gpio >= 0)
//     {
//       pinMode(gpio, OUTPUT);
//       digitalWrite(gpio, isOn ? HIGH : LOW);
//     }

//     Serial.printf("[WebUI] Device %s (GPIO %d) -> %s\n",
//                   name.c_str(), gpio, isOn ? "ON" : "OFF");
//   }

//   // =========== SETTING: Luu WiFi / CoreIoT vao LittleFS ===========
//   else if (page == "setting")
//   {
//     String WIFI_SSID_local  = value["ssid"]     | "";
//     String WIFI_PASS_local  = value["password"] | "";
//     String CORE_TOKEN_local = value["token"]    | "";
//     String CORE_SERV_local  = value["server"]   | "";
//     String CORE_PORT_local  = value["port"]     | "";

//     WIFI_SSID      = WIFI_SSID_local;
//     WIFI_PASS      = WIFI_PASS_local;
//     CORE_IOT_TOKEN = CORE_TOKEN_local;
//     CORE_IOT_SERVER= CORE_SERV_local;
//     CORE_IOT_PORT  = CORE_PORT_local;

//     Serial.println("Nhan cau hinh tu WebSocket:");
//     Serial.println("SSID: " + WIFI_SSID);
//     Serial.println("PASS: " + WIFI_PASS);
//     Serial.println("TOKEN: " + CORE_IOT_TOKEN);
//     Serial.println("SERVER: " + CORE_IOT_SERVER);
//     Serial.println("PORT: " + CORE_IOT_PORT);

//     // Goi ham luu cau hinh
//     Save_info_File(WIFI_SSID, WIFI_PASS, CORE_IOT_TOKEN, CORE_IOT_SERVER, CORE_IOT_PORT);

//     // Phan hoi lai client
//     String msg = "{\"status\":\"ok\",\"page\":\"setting_saved\"}";
//     ws.textAll(msg);
//   }

//   // =========== THRESHOLD: Cap nhat nguong nhiet/am ===========
//   else if (page == "threshold")
//   {
//     float tCold  = value["tempCold"]  | tempColdThreshold;
//     float tHot   = value["tempHot"]   | tempHotThreshold;
//     float hDry   = value["humiDry"]   | humiDryThreshold;
//     float hHumid = value["humiHumid"] | humiHumidThreshold;

//     // Gioi han do am 0-100
//     if (hDry   < 0.0f)   hDry   = 0.0f;
//     if (hDry   > 100.0f) hDry   = 100.0f;
//     if (hHumid < 0.0f)   hHumid = 0.0f;
//     if (hHumid > 100.0f) hHumid = 100.0f;

//     // Dam bao thu tu hop ly cho nhiet do
//     if (tCold > tHot)
//     {
//       float mid = (tCold + tHot) * 0.5f;
//       tCold = mid - 0.5f;
//       tHot  = mid + 0.5f;
//     }

//     // Dam bao thu tu hop ly cho do am
//     if (hDry > hHumid)
//     {
//       float mid = (hDry + hHumid) * 0.5f;
//       hDry   = mid - 1.0f;
//       hHumid = mid + 1.0f;
//       if (hDry < 0.0f)   hDry = 0.0f;
//       if (hHumid > 100.0f) hHumid = 100.0f;
//     }

//     tempColdThreshold  = tCold;
//     tempHotThreshold   = tHot;
//     humiDryThreshold   = hDry;
//     humiHumidThreshold = hHumid;

//     Serial.println("Cap nhat nguong nhiet/am tu WebUI:");
//     Serial.printf("  TEMP_COLD = %.1f\n", tempColdThreshold);
//     Serial.printf("  TEMP_HOT  = %.1f\n", tempHotThreshold);
//     Serial.printf("  HUMI_DRY  = %.1f\n", humiDryThreshold);
//     Serial.printf("  HUMI_HUMID= %.1f\n", humiHumidThreshold);

//     ws.textAll("{\"page\":\"threshold_saved\"}");
//   }

//   // =========== GET_CONFIG: Gui toan bo cau hinh hien tai ve Web UI ===========
//   else if (page == "get_config")
//   {
//     StaticJsonDocument<512> resp;
//     resp["page"] = "config";
//     JsonObject v = resp.createNestedObject("value");

//     // Trang thai thiet bi (cho nut gat LED1, LED2)
//     JsonArray devs = v.createNestedArray("devices");
//     JsonObject d1 = devs.createNestedObject();
//     d1["name"]   = "LED1";
//     d1["gpio"]   = LED_GPIO;
//     d1["status"] = glob_temp_led_enabled ? "ON" : "OFF";

//     JsonObject d2 = devs.createNestedObject();
//     d2["name"]   = "LED2";
//     d2["gpio"]   = NEO_PIN;
//     d2["status"] = glob_humi_led_enabled ? "ON" : "OFF";

//     // Cau hinh WiFi/CoreIoT de pre-fill vao form Cai dat
//     JsonObject s = v.createNestedObject("settings");
//     s["ssid"]     = WIFI_SSID;
//     s["password"] = WIFI_PASS;
//     s["token"]    = CORE_IOT_TOKEN;
//     s["server"]   = CORE_IOT_SERVER;
//     s["port"]     = CORE_IOT_PORT;

//     String out;
//     serializeJson(resp, out);
//     Webserver_sendata(out);
//   }

//   // =========== RESET_FACTORY: Xoa file cau hinh & restart ===========
//   else if (page == "reset_factory")
//   {
//     Serial.println("Yeu cau Reset Factory tu Web UI");
//     Delete_info_File();
//   }
// }

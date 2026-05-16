#include "task_handler.h"
#include "global.h"
#include "task_webserver.h"
#include "led_blinky.h"
#include "neo_blinky.h"
#include "coreiot.h"

// RGB -> "#RRGGBB"
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

  // =========== DEVICE: Bật/tắt LED1/LED2 ===========
  if (page == "device")
  {
    String name   = value["name"] | "";
    String status = value["status"] | "";
    int gpio      = value["gpio"] | -1;
    bool isOn     = (status == "ON");

    if (name == "LED1")
    {
      glob_led01_enabled = isOn;
    }
    else if (name == "LED2")
    {
      glob_led02_enabled = isOn;
      if (xLed02Semaphore != nullptr)
      {
        xSemaphoreGive(xLed02Semaphore);
      }
    }

    if (gpio >= 0)
    {
      pinMode(gpio, OUTPUT);
      digitalWrite(gpio, isOn ? HIGH : LOW);
    }

    Serial.printf("[WebUI] Device %s (GPIO %d) -> %s\n",
                  name.c_str(), gpio, isOn ? "ON" : "OFF");

    // Đồng bộ trạng thái mới lên CoreIoT để server/web luôn cùng trạng thái.
    coreiot_publish_led_states();
  }

  // =========== SETTING: Lưu WiFi / CoreIoT vào LittleFS ===========
  else if (page == "setting")
  {
    String WIFI_SSID_local  = value["ssid"]     | "";
    String WIFI_PASS_local  = value["password"] | "";
    String CORE_TOKEN_local = value["token"]    | "";
    String CORE_SERV_local  = value["server"]   | "";
    String CORE_PORT_local  = value["port"]     | "";

    WIFI_SSID = WIFI_SSID_local;
    WIFI_PASS = WIFI_PASS_local;
    CORE_IOT_TOKEN = CORE_TOKEN_local;
    CORE_IOT_SERVER = CORE_SERV_local;
    CORE_IOT_PORT = CORE_PORT_local;

    Serial.println("Nhận cấu hình từ WebSocket:");
    Serial.println("SSID: " + WIFI_SSID);
    Serial.println("PASS: " + WIFI_PASS);
    Serial.println("TOKEN: " + CORE_IOT_TOKEN);
    Serial.println("SERVER: " + CORE_IOT_SERVER);
    Serial.println("PORT: " + CORE_IOT_PORT);

    Save_info_File(WIFI_SSID, WIFI_PASS, CORE_IOT_TOKEN, CORE_IOT_SERVER, CORE_IOT_PORT);

    String msg = "{\"status\":\"ok\",\"page\":\"setting_saved\"}";
    ws.textAll(msg);
  }

  // =========== GET_CONFIG: Gửi toàn bộ cấu hình hiện tại về Web UI ===========
  else if (page == "get_config")
  {
    StaticJsonDocument<512> resp;
    resp["page"] = "config";
    JsonObject v = resp.createNestedObject("value");

    // Trạng thái thiết bị (cho nút gạt LED1, LED2)
    JsonArray devs = v.createNestedArray("devices");
    JsonObject d1 = devs.createNestedObject();
    d1["name"] = "LED1";
    d1["gpio"] = LED_GPIO;
    d1["status"] = glob_led01_enabled ? "ON" : "OFF";

    JsonObject d2 = devs.createNestedObject();
    d2["name"] = "LED2";
    d2["gpio"] = NEO_PIN;
    d2["status"] = glob_led02_enabled ? "ON" : "OFF";

    // Cấu hình WiFi/CoreIoT để pre-fill vào form Cài đặt
    JsonObject s = v.createNestedObject("settings");
    s["ssid"] = WIFI_SSID;
    s["password"] = WIFI_PASS;
    s["token"] = CORE_IOT_TOKEN;
    s["server"] = CORE_IOT_SERVER;
    s["port"] = CORE_IOT_PORT;

   // Vẫn trả cấu hình hiển thị để UI cũ không lỗi parse.
    JsonObject thr = v.createNestedObject("thresholds");
    thr["tempCold"] = tempColdThreshold;
    thr["tempHot"] = tempHotThreshold;
    thr["humiDry"] = humiDryThreshold;
    thr["humiHumid"] = humiHumidThreshold;

    JsonObject lp = v.createNestedObject("ledPattern");
    lp["coldOn"] = led01Config[TEMP_LEVEL_COLD].on_ms;
    lp["coldOff"] = led01Config[TEMP_LEVEL_COLD].off_ms;
    lp["normalOn"] = led01Config[TEMP_LEVEL_NORMAL].on_ms;
    lp["normalOff"] = led0 1Config[TEMP_LEVEL_NORMAL].off_ms;
    lp["hotOn"] = led01Config[TEMP_LEVEL_HOT].on_ms;
    lp["hotOff"] = led01Config[TEMP_LEVEL_HOT].off_ms;

    JsonObject neo = v.createNestedObject("neoColors");
    neo["dry"] = rgbToHex(neoColorConfig[HUMI_LEVEL_DRY].r,
                          neoColorConfig[HUMI_LEVEL_DRY].g,
                          neoColorConfig[HUMI_LEVEL_DRY].b);
    neo["ok"] = rgbToHex(neoColorConfig[HUMI_LEVEL_OK].r,
                         neoColorConfig[HUMI_LEVEL_OK].g,
                         neoColorConfig[HUMI_LEVEL_OK].b);
    neo["humid"] = rgbToHex(neoColorConfig[HUMI_LEVEL_HUMID].r,
                            neoColorConfig[HUMI_LEVEL_HUMID].g,
                            neoColorConfig[HUMI_LEVEL_HUMID].b);

    String out;
    serializeJson(resp, out);
    Webserver_sendata(out);
  }

  // =========== RESET_FACTORY: Xóa file cấu hình & restart ===========
  else if (page == "reset_factory")
  {
    Serial.println("Yêu cầu Reset Factory từ Web UI");
    Delete_info_File();
  }
}

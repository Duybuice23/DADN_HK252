#include "coreiot.h"
#include "task_webserver.h"
#include <ctype.h>
#include <string.h>  

WiFiClient   espClient;
PubSubClient client(espClient);

static bool equalsIgnoreCase(const char *a, const char *b)
{
  if (!a || !b) return false;
  while (*a && *b)
  {
    if (toupper((unsigned char)*a) != toupper((unsigned char)*b)) return false;
    ++a; ++b;
  }
  return (*a == '\0' && *b == '\0');
}

static bool rpcParamToBool(const JsonVariantConst &param)
{
  if (param.is<bool>())  return param.as<bool>();
  if (param.is<int>())   return param.as<int>() != 0;

  const char *s = param.as<const char*>();
  if (!s) return false;

  if (equalsIgnoreCase(s, "on")   ||
      equalsIgnoreCase(s, "true") ||
      strcmp(s, "1") == 0)
  {
    return true;
  }
  return false;
}

// Lấy requestId từ topic "v1/devices/me/rpc/request/<id>"
static const char* extractRequestId(const char *topic)
{
  const char *p = strrchr(topic, '/');
  if (!p) return nullptr;
  if (*(p + 1) == '\0') return nullptr; 
  return p + 1;                        
}

// Publish response RPC
static void sendRpcResponse(const char *requestId, const StaticJsonDocument<128> &doc)
{
  if (!requestId)
  {
    Serial.println("[CoreIoT] (no requestId) Skip RPC response");
    return;
  }

  char respTopic[64];
  snprintf(respTopic, sizeof(respTopic),
           "v1/devices/me/rpc/response/%s", requestId);

  String payload;
  serializeJson(doc, payload);

  bool ok = client.publish(respTopic, payload.c_str());
  Serial.print("[CoreIoT] RPC response -> ");
  Serial.println(ok ? "OK" : "FAILED");
}

void coreiot_publish_led_states()
{
  StaticJsonDocument<128> doc;
  doc["led01"] = glob_led01_enabled;
  doc["led02"] = glob_led02_enabled;
  doc["buzzer"] = glob_buzzer_enabled;
  String json;
  serializeJson(doc, json);
  client.publish("v1/devices/me/attributes", json.c_str());
}

static void broadcastDeviceStateToWebUI(const char *name, bool isOn, int gpio)
{
  StaticJsonDocument<128> wsDoc;
  wsDoc["page"] = "device";
  JsonObject v = wsDoc.createNestedObject("value");
  v["name"] = name;
  v["status"] = isOn ? "ON" : "OFF";
  v["gpio"] = gpio;

  String out;
  serializeJson(wsDoc, out);
  Webserver_sendata(out);
}

void callback(char* topic, byte* payload, unsigned int length)
{
  // Lấy requestId từ topic
  const char *requestId = extractRequestId(topic);

  // Copy payload sang buffer tạm
  char message[256];
  length = (length > sizeof(message) - 1) ? (sizeof(message) - 1) : length;
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.printf("[CoreIoT] RPC Recv: %s\n", message);

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.println("JSON Error");
    return;
  }

  const char *method = doc["method"];
  JsonVariantConst params = doc["params"];

  if (!method) return;
  if (strcmp(method, "setLed01") == 0)
  {
    bool newState = rpcParamToBool(params);
    glob_led01_enabled = newState;
    
    // Gửi response NGAY LẬP TỨC
    coreiot_publish_led_states();
    broadcastDeviceStateToWebUI("LED1", glob_led01_enabled, LED_GPIO);
    StaticJsonDocument<128> resp;
    resp["method"]  = "setLed01";
    resp["success"] = true;
    resp["led01"] = glob_led01_enabled;
    sendRpcResponse(requestId, resp);
  }
  // ----- RPC SET: Bật/tắt NeoPixel -----
  else if (strcmp(method, "setLed02") == 0)
  {
    bool newState = rpcParamToBool(params);
    glob_led02_enabled = newState;
    
    // Kích hoạt semaphore ngay để task LED phản hồi
    if (xLed02Semaphore != nullptr)
      xSemaphoreGive(xLed02Semaphore);

    coreiot_publish_led_states();
    broadcastDeviceStateToWebUI("LED2", glob_led02_enabled, NEO_PIN);
    StaticJsonDocument<128> resp;
    resp["method"]  = "setLed02";
    resp["success"] = true;
    resp["led02"] = glob_led02_enabled;
    sendRpcResponse(requestId, resp);
  }
  // ----- RPC SET: Buzzer -----
  else if (strcmp(method, "setBuzzer") == 0)
  {
    bool newState = rpcParamToBool(params);
    glob_buzzer_enabled = newState;

    coreiot_publish_led_states();
    broadcastDeviceStateToWebUI("Buzzer", glob_buzzer_enabled, BUZZER_PIN);

    StaticJsonDocument<128> resp;
    resp["method"]  = "setBuzzer";
    resp["success"] = true;
    resp["buzzer"] = glob_buzzer_enabled;
    sendRpcResponse(requestId, resp);
  }
  // ----- RPC GET -----
  else if (strcmp(method, "getLed01") == 0)
  {
    StaticJsonDocument<128> resp;
    resp["method"]  = "getLed01";
    resp["led01"] = glob_led01_enabled;
    sendRpcResponse(requestId, resp);
  }
  else if (strcmp(method, "getLed02") == 0)
  {
    StaticJsonDocument<128> resp;
    resp["method"]  = "getLed02";
    resp["led02"] = glob_led02_enabled;
    sendRpcResponse(requestId, resp);
  }
}

static void setup_coreiot()
{
  Serial.println("[CoreIoT] Waiting for internet...");
  if (xBinarySemaphoreInternet != nullptr)
  {
    // Chờ tối đa 30s, nếu không có internet thì vẫn chạy để reconnect sau
    xSemaphoreTake(xBinarySemaphoreInternet, pdMS_TO_TICKS(30000));
  }
  Serial.println("[CoreIoT] Internet check done.");
  client.setServer(CORE_IOT_SERVER.c_str(), CORE_IOT_PORT.toInt());
  client.setCallback(callback);
}

static void reconnect()
{
  if (!client.connected())
  {
    Serial.print("[CoreIoT] Reconnecting...");
    String clientId = "ESP32-" + String(random(0xffff), HEX);

    if (client.connect(clientId.c_str(), CORE_IOT_TOKEN.c_str(), nullptr))
    {
      Serial.println(" Connected!");
      client.subscribe("v1/devices/me/rpc/request/+");
      coreiot_publish_led_states();
    }
    else
    {
      Serial.print(" Failed (rc=");
      Serial.print(client.state());
      Serial.println(")");
     }
  }
}

void coreiot_task(void *pvParameters)
{
  setup_coreiot();

  // Biến dùng cho timer không chặn (Non-blocking)
  unsigned long lastTelemetrySend = 0;
  const unsigned long TELEMETRY_INTERVAL = 3000; // 3 giây gửi 1 lần

  for (;;)
  {
    if (!client.connected())
    {
      reconnect();
      // Nếu reconnect thất bại, delay 5s TRƯỚC khi thử lại để tránh spam
      if (!client.connected()) {
          vTaskDelay(pdMS_TO_TICKS(5000));
          continue; 
      }
    }
    
    // [QUAN TRỌNG] Phải gọi hàm này liên tục để nhận tin nhắn RPC
    client.loop();

    // Kiểm tra thời gian để gửi Telemetry (Không dùng delay)
    unsigned long now = millis();
    if (now - lastTelemetrySend > TELEMETRY_INTERVAL)
    {
        lastTelemetrySend = now;

        StaticJsonDocument<256> doc;
        doc["temperature"] = glob_temperature;
        doc["humidity"]    = glob_humidity;
        doc["gas"] = glob_gas;
        String payload;
        serializeJson(doc, payload);
        client.publish("v1/devices/me/telemetry", payload.c_str());
    }

    // Delay cực ngắn để nhường CPU cho các task khác, nhưng đủ nhanh để nhận RPC
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}


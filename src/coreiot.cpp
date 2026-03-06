#include "coreiot.h"
#include <ctype.h>
#include <string.h>  

WiFiClient   espClient;
PubSubClient client(espClient);

// Helper: so sánh chuỗi không phân biệt hoa thường
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

// Helper: convert params RPC => bool
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

static void publishLedStates()
{
  StaticJsonDocument<128> doc;
  doc["tempLed"] = glob_temp_led_enabled;
  doc["humiLed"] = glob_humi_led_enabled;

  String json;
  serializeJson(doc, json);
  client.publish("v1/devices/me/attributes", json.c_str());
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

  // ----- RPC SET: Bật/tắt LED nhiệt độ -----
  if (strcmp(method, "setTempLed") == 0)
  {
    bool newState = rpcParamToBool(params);
    glob_temp_led_enabled = newState;
    
    // Gửi response NGAY LẬP TỨC
    publishLedStates();
    StaticJsonDocument<128> resp;
    resp["method"]  = "setTempLed";
    resp["success"] = true;
    resp["tempLed"] = glob_temp_led_enabled;
    sendRpcResponse(requestId, resp);
  }
  // ----- RPC SET: Bật/tắt NeoPixel -----
  else if (strcmp(method, "setHumiLed") == 0)
  {
    bool newState = rpcParamToBool(params);
    glob_humi_led_enabled = newState;
    
    // Kích hoạt semaphore ngay để task LED phản hồi
    if (xHumiNeoSemaphore != nullptr)
      xSemaphoreGive(xHumiNeoSemaphore);

    publishLedStates();
    StaticJsonDocument<128> resp;
    resp["method"]  = "setHumiLed";
    resp["success"] = true;
    resp["humiLed"] = glob_humi_led_enabled;
    sendRpcResponse(requestId, resp);
  }
  // ----- RPC GET -----
  else if (strcmp(method, "getTempLed") == 0)
  {
    StaticJsonDocument<128> resp;
    resp["method"]  = "getTempLed";
    resp["tempLed"] = glob_temp_led_enabled;
    sendRpcResponse(requestId, resp);
  }
  else if (strcmp(method, "getHumiLed") == 0)
  {
    StaticJsonDocument<128> resp;
    resp["method"]  = "getHumiLed";
    resp["humiLed"] = glob_humi_led_enabled;
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
      publishLedStates();
    }
    else
    {
      Serial.print(" Failed (rc=");
      Serial.print(client.state());
      Serial.println(")");
      // Không delay 5s ở đây để tránh block các task khác quá lâu
      // Task loop sẽ lo việc chờ đợi
    }
  }
}

void coreiot_task(void *pvParameters)
{
  setup_coreiot();

  // Biến dùng cho timer không chặn (Non-blocking)
  unsigned long lastTelemetrySend = 0;
  const unsigned long TELEMETRY_INTERVAL = 5000; // 5 giây gửi 1 lần

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
        String payload;
        serializeJson(doc, payload);
        client.publish("v1/devices/me/telemetry", payload.c_str());
    }

    // Delay cực ngắn để nhường CPU cho các task khác, nhưng đủ nhanh để nhận RPC
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
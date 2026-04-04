#include "task_core_iot.h"
#include "global.h"
constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

constexpr char LED_STATE_ATTR[] = "ledState";

volatile int ledMode = 0;
volatile bool ledState = false;

constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U;
volatile uint16_t blinkingInterval = 1000U;

constexpr int16_t telemetrySendInterval = 10000U;

constexpr std::array<const char *, 2U> SHARED_ATTRIBUTES_LIST = {
    LED_STATE_ATTR,
};

void processSharedAttributes(const Shared_Attribute_Data &data)
{
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        // if (strcmp(it->key().c_str(), BLINKING_INTERVAL_ATTR) == 0)
        // {
        //     const uint16_t new_interval = it->value().as<uint16_t>();
        //     if (new_interval >= BLINKING_INTERVAL_MS_MIN && new_interval <= BLINKING_INTERVAL_MS_MAX)
        //     {
        //         blinkingInterval = new_interval;
        //         Serial.print("Blinking interval is set to: ");
        //         Y
        //             Serial.println(new_interval);
        //     }
        // }
        // if (strcmp(it->key().c_str(), LED_STATE_ATTR) == 0)
        // {
        //     ledState = it->value().as<bool>();
        // digitalWrite(LED_PIN, ledState);
        // Serial.print("LED state is set to: ");
        // Serial.println(ledState);
        // }
    }
}

RPC_Response handleSetLogic(const char* methodName, bool state) {
    if (strcmp(methodName, "setTempLed") == 0) {
        glob_temp_led_enabled = state;
        // Bổ sung code điều khiển phần cứng nếu có: digitalWrite(PIN_1, state);
    } 
    else if (strcmp(methodName, "setHumiLed") == 0) {
        glob_humi_led_enabled = state;
        // Bổ sung code điều khiển phần cứng nếu có: digitalWrite(PIN_2, state);
    }

    Serial.printf("[%s] State changed to: %s\n", methodName, state ? "ON" : "OFF");
    
    // Trả về đúng tên method và trạng thái
    return RPC_Response(methodName, state);
}

// --- HÀM XỬ LÝ CHUNG CHO LỆNH GET (LẤY TRẠNG THÁI) ---
RPC_Response handleGetLogic(const char* methodName) {
    bool currentState = false;
    
    if (strcmp(methodName, "getTempLed") == 0) {
        currentState = glob_temp_led_enabled;
    } 
    else if (strcmp(methodName, "getHumiLed") == 0) {
        currentState = glob_humi_led_enabled;
    }
    
    return RPC_Response(methodName, currentState);
}


RPC_Response setTempLedSwitch(const RPC_Data &data) { return handleSetLogic("setTempLed", data); }
RPC_Response setHumiLedSwitch(const RPC_Data &data) { return handleSetLogic("setHumiLed", data); }

RPC_Response getTempLedStatus(const RPC_Data &data) { return handleGetLogic("getTempLed"); }
RPC_Response getHumiLedStatus(const RPC_Data &data) { return handleGetLogic("getHumiLed"); }

RPC_Response setLedSwitchValue(const RPC_Data &data)
{
    Serial.println("Received Switch state");
    bool newState = data;
    Serial.print("Switch state change: ");
    Serial.println(newState);
    return RPC_Response("setLedSwitchValue", newState);
}

const std::array<RPC_Callback, 4U> callbacks = {
    RPC_Callback{"setTempLed", setTempLedSwitch},
    RPC_Callback{"setHumiLed", setHumiLedSwitch},
    RPC_Callback{"getTempLed", getTempLedStatus},
    RPC_Callback{"getHumiLed", getHumiLedStatus}
};

const Shared_Attribute_Callback attributes_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback attribute_shared_request_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());

void CORE_IOT_sendata(String mode, String feed, String data)
{
    if (mode == "attribute")
    {
        tb.sendAttributeData(feed.c_str(), data);
    }
    else if (mode == "telemetry")
    {
        float value = data.toFloat();
        tb.sendTelemetryData(feed.c_str(), value);
    }
    else
    {
        // handle unknown mode
    }
}

void CORE_IOT_reconnect()
{
    if (!tb.connected())
    {
        if (!tb.connect(CORE_IOT_SERVER.c_str(), CORE_IOT_TOKEN.c_str(), CORE_IOT_PORT.toInt()))
        {
            // Serial.println("Failed to connect");
            return;
        }

        tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());

        Serial.println("Subscribing for RPC...");
        if (!tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend()))
        {
            // Serial.println("Failed to subscribe for RPC");
            return;
        }

        if (!tb.Shared_Attributes_Subscribe(attributes_callback))
        {
            // Serial.println("Failed to subscribe for shared attribute updates");
            return;
        }

        Serial.println("Subscribe done");

        if (!tb.Shared_Attributes_Request(attribute_shared_request_callback))
        {
            // Serial.println("Failed to request for shared attributes");
            return;
        }
        tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
    }
    else if (tb.connected())
    {
        tb.loop();
    }
}
#include "task_core_iot.h"
#include "global.h"
#include "buzzer.h"
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
    if (strcmp(methodName, "setLed01") == 0) {
        glob_led01_enabled = state;
        // Bổ sung code điều khiển phần cứng nếu có: digitalWrite(PIN_1, state);
    } 
    else if (strcmp(methodName, "setLed02") == 0) {
        glob_led02_enabled = state;
        // Bổ sung code điều khiển phần cứng nếu có: digitalWrite(PIN_2, state);
    }

    else if (strcmp(methodName, "setBuzzer") == 0) {
        glob_buzzer_enabled = state;
        // digitalWrite(BUZZER_PIN, state ? HIGH : LOW);
    }

    Serial.printf("[%s] State changed to: %s\n", methodName, state ? "ON" : "OFF");
    return RPC_Response(methodName, state);
}

RPC_Response handleGetLogic(const char* methodName) {
    bool currentState = false;
    
    if (strcmp(methodName, "getLed01") == 0) {
        currentState = glob_led01_enabled;
    } 
    else if (strcmp(methodName, "getLed02") == 0) {
        currentState = glob_led02_enabled;
    }

    else if (strcmp(methodName, "getBuzzer") == 0) {
        currentState = glob_buzzer_enabled;
    }
    
    return RPC_Response(methodName, currentState);
}


RPC_Response setLed01Switch(const RPC_Data &data) { return handleSetLogic("setLed01", data); }
RPC_Response setLed02Switch(const RPC_Data &data) { return handleSetLogic("setLed02", data); }
RPC_Response setBuzzerSwitch(const RPC_Data &data) { return handleSetLogic("setBuzzer", data); }

RPC_Response getLed01Status(const RPC_Data &data) { return handleGetLogic("getLed01"); }
RPC_Response getLed02Status(const RPC_Data &data) { return handleGetLogic("getLed02"); }
RPC_Response getBuzzerStatus(const RPC_Data &data) { return handleGetLogic("getBuzzer"); }


RPC_Response setLedSwitchValue(const RPC_Data &data)
{
    Serial.println("Received Switch state");
    bool newState = data;
    Serial.print("Switch state change: ");
    Serial.println(newState);
    return RPC_Response("setLedSwitchValue", newState);
}

const std::array<RPC_Callback, 6U> callbacks = {
    RPC_Callback{"setLed01", setLed01Switch},
    RPC_Callback{"setLed02", setLed02Switch},
    RPC_Callback{"getLed01", getLed01Status},
    RPC_Callback{"getLed02", getLed02Status},
    RPC_Callback{"setBuzzer", setBuzzerSwitch},
    RPC_Callback{"getBuzzer", getBuzzerStatus}
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

        // Removed sending device metadata attributes to keep only device states

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
        // localIp attribute omitted; only sending led/buzzer states as attributes
    }
    else if (tb.connected())
    {
        tb.loop();
    }
}


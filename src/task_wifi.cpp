#include "task_wifi.h"

void startAP()
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(String(SSID_AP), String(PASS_AP));
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

void startSTA()
{
    if (WIFI_SSID.isEmpty())
    {
        // Không có SSID thì khỏi kết nối STA
        return;
    }

    WiFi.mode(WIFI_STA);

    if (WIFI_PASS.isEmpty()) WiFi.begin(WIFI_SSID.c_str());
    else                     WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (millis() - t0 > 15000) {   // timeout 15s để không treo vĩnh viễn
            Serial.println("❌ STA connect timeout");
            return;
        }
    }

    Serial.print("✅ STA IP: ");
    Serial.println(WiFi.localIP());

    xSemaphoreGive(xBinarySemaphoreInternet);
}

bool Wifi_reconnect()
{
    if (WiFi.status() == WL_CONNECTED) return true;

    startSTA();
    return (WiFi.status() == WL_CONNECTED);
}

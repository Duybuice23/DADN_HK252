#include "temp_humi_monitor.h"
#include <Wire.h>
#include <ArduinoJson.h>
#include "task_webserver.h"
#include <mq2.h>
DHT20 dht20;
// I2C LCD: address 33 (0x21), 16x2
LiquidCrystal_I2C lcd(33, 16, 2);
// Khai báo lại các hàm cho đúng
static void updateLcd(float temperature, float humidity, float gas);
static void sendSensorToWeb(float temperature, float humidity, int gas);

void temp_humi_monitor(void *pvParameters)
{
  Wire.begin(11, 12);
  dht20.begin();
  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  lcd.setCursor(0, 1);
  lcd.print("Please wait");
  vTaskDelay(pdMS_TO_TICKS(1500));

  for (;;)
  {
    dht20.read();
    float temperature = dht20.getTemperature();
    float humidity    = dht20.getHumidity();
    int gas = analogRead(MQ2_PIN);

    if (isnan(temperature) || isnan(humidity))
    {
      Serial.println("Failed to read from DHT20!");
      temperature = -1.0f;
      humidity    = -1.0f;
    }

    glob_temperature = temperature;
    glob_humidity    = humidity;
    glob_gas = gas;

    updateLcd(temperature, humidity, gas);

    sendSensorToWeb(temperature, humidity, gas);

    Serial.print("[DHT20] H: ");
    Serial.print(humidity);
    Serial.print("%  T: ");
    Serial.print(temperature);
    Serial.print(" C ");
    Serial.print("G: ");
    Serial.println(gas);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void updateLcd(float temperature, float humidity, float gas)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Gas: ");
  lcd.print(gas, 0); 

  // Dòng 2: In nhiệt độ & độ ẩm
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C ");
  lcd.print("H:");
  lcd.print(humidity, 0);
  lcd.print("%");
}

static void sendSensorToWeb(float temperature, float humidity, int gas)
{
  StaticJsonDocument<192> doc;
  doc["page"] = "sensor";
  doc["temp"] = temperature;
  doc["humi"] = humidity;
  doc["gas"] = gas;

  String json;
  serializeJson(doc, json);
  Webserver_sendata(json);
}
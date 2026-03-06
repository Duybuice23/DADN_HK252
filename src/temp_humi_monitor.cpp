#include "temp_humi_monitor.h"
#include <Wire.h>
#include <ArduinoJson.h>
#include "task_webserver.h"

DHT20 dht20;
// I2C LCD: address 33 (0x21), 16x2
LiquidCrystal_I2C lcd(33, 16, 2);

// Khai báo lại các hàm cho đúng
static void updateLcd(float temperature, float humidity);
static void sendSensorToWeb(float temperature, float humidity);

void temp_humi_monitor(void *pvParameters)
{
  Wire.begin(11, 12);
  dht20.begin();

  lcd.begin();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("DHT20 starting...");
  lcd.setCursor(0, 1);
  lcd.print("Please wait");
  vTaskDelay(pdMS_TO_TICKS(1500));

  for (;;)
  {
    dht20.read();
    float temperature = dht20.getTemperature();
    float humidity    = dht20.getHumidity();

    if (isnan(temperature) || isnan(humidity))
    {
      Serial.println("Failed to read from DHT20!");
      temperature = -1.0f;
      humidity    = -1.0f;
    }

    // Cập nhật giá trị thô toàn cục
    glob_temperature = temperature;
    glob_humidity    = humidity;

    // GỌI HÀM CẬP NHẬT LCD (Đã fix lỗi màn hình đơ)
    updateLcd(temperature, humidity);

    // Gửi dữ liệu lên webserver qua WebSocket
    sendSensorToWeb(temperature, humidity);

    Serial.print("[DHT20] H: ");
    Serial.print(humidity);
    Serial.print("%  T: ");
    Serial.print(temperature);
    Serial.println(" C");

    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

// Bỏ tham số DisplayState đi vì không dùng nữa
static void updateLcd(float temperature, float humidity)
{
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Monitoring..."); 

  // Dòng 2: In nhiệt độ & độ ẩm
  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C ");
  lcd.print("H:");
  lcd.print(humidity, 0);
  lcd.print("%");
}

static void sendSensorToWeb(float temperature, float humidity)
{
  StaticJsonDocument<128> doc;
  doc["page"] = "sensor";
  doc["temp"] = temperature;
  doc["humi"] = humidity;

  String json;
  serializeJson(doc, json);
  Webserver_sendata(json);
}
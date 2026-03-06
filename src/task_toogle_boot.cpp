#include "task_toogle_boot.h"
#include "global.h"
#include <WiFi.h>

#define BOOT_BUTTON_PIN 0
#define HOLD_TIME_MS    5000

void Task_Toogle_BOOT(void *pvParameters)
{
  pinMode(BOOT_BUTTON_PIN, INPUT_PULLUP); 

  unsigned long buttonPressStartTime = 0;
  bool isPressed = false;

  Serial.println("BOOT: San sang. Nhan giu > 3s de Factory Reset.");

  for (;;)
  {
    // Nút BOOT kích hoạt mức THẤP (LOW)
    if (digitalRead(BOOT_BUTTON_PIN) == LOW)
    {
      if (!isPressed)
      {
        isPressed = true;
        buttonPressStartTime = millis();
        Serial.println(">> Nut BOOT dang duoc nhan...");
      }
      else
      {
        unsigned long holdDuration = millis() - buttonPressStartTime;

        // Nếu giữ quá 3 giây
        if (holdDuration > HOLD_TIME_MS)
        {
          Serial.println("\n[SYSTEM] === FACTORY RESET KICH HOAT ===");
          
          // 1. Nháy LED báo hiệu
          pinMode(LED_GPIO, OUTPUT); 
          for(int i=0; i<5; i++){
              digitalWrite(LED_GPIO, !digitalRead(LED_GPIO));
              vTaskDelay(pdMS_TO_TICKS(100));
          }
          digitalWrite(LED_GPIO, LOW);

          // 2. Xóa file cấu hình trong LittleFS
          Delete_info_File();
          Serial.println("[SYSTEM] Da xoa file config.");

          // 3. [QUAN TRỌNG] Xóa WiFi lưu trong bộ nhớ NVS của ESP32
          // Tham số true thứ nhất: WiFi OFF
          // Tham số true thứ hai: Erase Configurations (Xóa SSID/Pass lưu trong Flash)
          WiFi.disconnect(true, true);
          vTaskDelay(pdMS_TO_TICKS(500)); // Đợi chip xử lý xóa Flash
          Serial.println("[SYSTEM] Da xoa WiFi NVS.");

          // 4. Khởi động lại
          Serial.println("[SYSTEM] Dang khoi dong lai...");
          Serial.flush();
          ESP.restart(); 
        }
      }
    }
    else
    {
      // Nhả nút
      if (isPressed)
      {
        isPressed = false;
        buttonPressStartTime = 0;
        Serial.println(">> Da nha nut BOOT.");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
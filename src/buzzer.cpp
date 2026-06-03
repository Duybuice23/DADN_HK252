#include "buzzer.h"
#include "global.h"

void buzzer_blinky(void *pvParameters)
{
  // Cấu hình PWM cho còi (Buzzer) trên ESP32
  const int buzzerChannel = 0;    // Chọn kênh PWM (0-15)
  const int buzzerFreq = 2000;    // Tần số âm thanh (Hz) - có thể thay đổi từ 1000 đến 4000 để đổi âm sắc
  const int buzzerResolution = 8; // Độ phân giải 8-bit (0-255)

  // Gắn chân BUZZER_PIN vào kênh PWM đã chọn
  ledcSetup(buzzerChannel, buzzerFreq, buzzerResolution);
  ledcAttachPin(BUZZER_PIN, buzzerChannel);

  for (;;)
  {
    if (!glob_buzzer_enabled)
    {
      // Tắt còi bằng cách đưa duty cycle về 0
      ledcWrite(buzzerChannel, 0); 
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    ledcWrite(buzzerChannel, 256); 
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
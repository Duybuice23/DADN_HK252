#include "led_blinky.h"
#include "global.h" // Bắt buộc include để đọc biến từ RPC

void led_blinky(void *pvParameters)
{
  pinMode(LED_GPIO, OUTPUT);

  for (;;)
  {
    // Kiểm tra cờ do RPC (ThingsBoard) điều khiển
    if (!glob_led01_enabled)
    {
      // Nếu tắt: Kéo chân GPIO xuống LOW
      digitalWrite(LED_GPIO, LOW);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue; 
    }

    // ==========================================
    // TRẠNG THÁI ON (LỆNH TỪ RPC ĐÃ BẬT TRUE)
    // ==========================================
    
    // Nếu bạn chỉ muốn sáng liên tục:
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(100)); // Vẫn phải delay để nhường CPU
    
    /* // Nếu bạn muốn nhấp nháy liên tục khi bật, hãy dùng đoạn code này:
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(500)); 
    digitalWrite(LED_GPIO, LOW);
    vTaskDelay(pdMS_TO_TICKS(500)); 
    */
  }
}

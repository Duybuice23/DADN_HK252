#include "neo_blinky.h"
#include "global.h" // Bắt buộc include để đọc biến từ RPC

static Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

void neo_blinky(void *pvParameters)
{
  strip.begin();
  strip.clear();
  strip.show();

  while (1)
  {
    // Kiểm tra cờ do RPC (ThingsBoard) điều khiển
    if (!glob_humi_led_enabled)
    {
      // Nếu tắt: Clear toàn bộ dải NeoPixel
      strip.clear();
      strip.show();
      
      // Nhường CPU 100ms
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // ==========================================
    // TRẠNG THÁI ON (LỆNH TỪ RPC ĐÃ BẬT TRUE)
    // ==========================================
    
    // Ví dụ: Khi bật từ Web, LED sáng màu Đỏ
    // Bạn có thể tùy biến logic đổi màu ở đây
    for(int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(255, 0, 0)); // R, G, B
    }
    strip.show();

    // Vẫn phải có Delay để không chiếm dụng 100% CPU
    vTaskDelay(pdMS_TO_TICKS(100)); 
  }
}
#include "led_blinky.h"

static void blinkPatternCold();
static void blinkPatternNormal();
static void blinkPatternHot();

void led_blinky(void *pvParameters)
{
  pinMode(LED_GPIO, OUTPUT);

  uint8_t currentLevel = glob_temp_level;

  for (;;)
  {
    // Nếu người dùng tắt LED từ web => giữ tắt và bỏ qua pattern
    if (!glob_temp_led_enabled)
    {
      digitalWrite(LED_GPIO, LOW);
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Nếu có thông báo mức nhiệt mới => cập nhật pattern
    if (xTempLedSemaphore != nullptr && xSemaphoreTake(xTempLedSemaphore, 0) == pdTRUE)
    {
      currentLevel = glob_temp_level;
    }

    switch (currentLevel)
    {
    case TEMP_LEVEL_COLD:
      // Lạnh
      blinkPatternCold();
      break;
    case TEMP_LEVEL_HOT:
      // Nóng
      blinkPatternHot();
      break;
    case TEMP_LEVEL_NORMAL:
    default:
      // Bình thường
      blinkPatternNormal();
      break;
    }
  }
}

// LẠNH
static void blinkPatternCold()
{
  TempLedConfig cfg = tempLedConfig[TEMP_LEVEL_COLD];
  digitalWrite(LED_GPIO, HIGH);
  vTaskDelay(pdMS_TO_TICKS(cfg.on_ms));
  digitalWrite(LED_GPIO, LOW);
  vTaskDelay(pdMS_TO_TICKS(cfg.off_ms));
}

// BÌNH THƯỜNG
static void blinkPatternNormal()
{
  TempLedConfig cfg = tempLedConfig[TEMP_LEVEL_NORMAL];
  digitalWrite(LED_GPIO, HIGH);
  vTaskDelay(pdMS_TO_TICKS(cfg.on_ms));
  digitalWrite(LED_GPIO, LOW);
  vTaskDelay(pdMS_TO_TICKS(cfg.off_ms));
}

// NÓNG
static void blinkPatternHot()
{
  TempLedConfig cfg = tempLedConfig[TEMP_LEVEL_HOT];

  for (int i = 0; i < 3; ++i)
  {
    digitalWrite(LED_GPIO, HIGH);
    vTaskDelay(pdMS_TO_TICKS(cfg.on_ms));
    digitalWrite(LED_GPIO, LOW);
    vTaskDelay(pdMS_TO_TICKS(cfg.off_ms));
  }
  
  vTaskDelay(pdMS_TO_TICKS(700));
}

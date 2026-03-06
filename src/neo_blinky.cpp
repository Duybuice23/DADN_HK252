#include "neo_blinky.h"
#include "global.h"

static Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);

static void applyHumiColor(uint8_t level, float humi);

void neo_blinky(void *pvParameters)
{
  strip.begin();
  strip.clear();
  strip.show();

  applyHumiColor(glob_humi_level, glob_humidity);

  while (1)
  {
    // Người dùng tắt NeoPixel từ web => luôn tắt
    if (!glob_humi_led_enabled)
    {
      strip.clear();
      strip.show();
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    if (xHumiNeoSemaphore != nullptr && xSemaphoreTake(xHumiNeoSemaphore, portMAX_DELAY) == pdTRUE)
    {
      applyHumiColor(glob_humi_level, glob_humidity);
    }
  }
}

static uint8_t mapBrightness(float x, float in_min, float in_max, uint8_t out_min, uint8_t out_max)
{
  if (in_max - in_min == 0)
  {
    return out_min;
  }

  if (x < in_min) x = in_min;
  if (x > in_max) x = in_max;

  float ratio = (x - in_min) / (in_max - in_min);
  float val = out_min + ratio * (out_max - out_min);

  if (val < 0)   val = 0;
  if (val > 255) val = 255;

  return (uint8_t)val;
}

static void applyHumiColor(uint8_t level, float humi)
{
  uint8_t r = 0, g = 0, b = 0;
  uint8_t brightness = 150;

  const float HUMI_MIN = 0.0f;
  const float HUMI_MAX = 100.0f;

  switch (level)
  {
  case HUMI_LEVEL_DRY:
    r = neoColorConfig[HUMI_LEVEL_DRY].r;
    g = neoColorConfig[HUMI_LEVEL_DRY].g;
    b = neoColorConfig[HUMI_LEVEL_DRY].b;
    brightness = mapBrightness(humi, HUMI_MIN, humiDryThreshold, 255, 80);
    break;

  case HUMI_LEVEL_OK:
    r = neoColorConfig[HUMI_LEVEL_OK].r;
    g = neoColorConfig[HUMI_LEVEL_OK].g;
    b = neoColorConfig[HUMI_LEVEL_OK].b;
    brightness = mapBrightness(humi, humiDryThreshold, humiHumidThreshold, 80, 200);
    break;

  case HUMI_LEVEL_HUMID:
    r = neoColorConfig[HUMI_LEVEL_HUMID].r;
    g = neoColorConfig[HUMI_LEVEL_HUMID].g;
    b = neoColorConfig[HUMI_LEVEL_HUMID].b;
    brightness = mapBrightness(humi, humiHumidThreshold, HUMI_MAX, 80, 255);
    break;

  default:
    r = neoColorConfig[HUMI_LEVEL_OK].r;
    g = neoColorConfig[HUMI_LEVEL_OK].g;
    b = neoColorConfig[HUMI_LEVEL_OK].b;
    brightness = 150;
    break;
  }

  strip.setBrightness(brightness);
  uint32_t color = strip.Color(r, g, b);
  strip.setPixelColor(0, color);
  strip.show();
}

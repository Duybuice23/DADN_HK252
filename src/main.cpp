#include "global.h"

// #include "led_blinky.h"
// #include "neo_blinky.h"
#include "temp_humi_monitor.h"
// #include "tinyml.h"
#include "coreiot.h"

// include task
#include "task_check_info.h"
#include "task_toogle_boot.h"
#include "task_wifi.h"
#include "task_webserver.h"
#include "task_core_iot.h"
// #include "mainserver.h"

void setup()
{
  Serial.begin(115200);

  
  check_info_File(false);

  // Task 1: LED theo nhiệt độ
  // xTaskCreate(led_blinky,
  //             "Task LED Blink",
  //             2048,
  //             nullptr,
  //             2,
  //             nullptr);

  // Task 2: NeoPixel theo độ ẩm
  // xTaskCreate(neo_blinky,
  //             "Task NEO Blink",
  //             2048,
  //             nullptr,
  //             2,
  //             nullptr);

  // Task 3: Đọc DHT20 + LCD + phát semaphore + gửi WebSocket
  xTaskCreate(temp_humi_monitor,
              "Task TEMP HUMI Monitor",
              4096,
              nullptr,
              3,   // ưu tiên nhỉnh hơn vì feed dữ liệu cho các task khác
              nullptr);

  // // Task 4: WebServer + AP + STA mode
  // xTaskCreate(
  //     main_server_task,
  //     "Main WebServer Task",
  //     8192,
  //     nullptr,
  //     4,
  //     nullptr
  // );
  // // Task 5: TinyML chạy mô hình & đánh giá accuracy
  // xTaskCreate(tiny_ml_task,
  //             "Tiny ML Task",
  //             8192,
  //             nullptr,
  //             2,
  //             nullptr);

  // CoreIoT MQTT client
  xTaskCreate(coreiot_task,
              "CoreIOT Task",
              4096,
              nullptr,
              2,
              nullptr);

  // Nút BOOT giữ lâu để xoá config WiFi
  xTaskCreate(Task_Toogle_BOOT,
              "Task_Toogle_BOOT",
              2048,
              nullptr,
              1,
              nullptr);
}

void loop()
{
  // Nếu đã có cấu hình WiFi => thử reconnect ở chế độ STA
  if (check_info_File(true))
  {
    Wifi_reconnect(); 
  }

  // Đảm bảo WebServer (AP/WebSocket) luôn chạy
  Webserver_reconnect();
}

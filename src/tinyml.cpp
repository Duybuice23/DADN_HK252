#include "tinyml.h"
#include "task_webserver.h"

// Buffer & đối tượng TFLM
namespace {
  constexpr int kTensorArenaSize = 40 * 1024;
  alignas(16) uint8_t tensor_arena[kTensorArenaSize];

  tflite::ErrorReporter *error_reporter = nullptr;
  const tflite::Model *model            = nullptr;
  tflite::MicroInterpreter *interpreter = nullptr;
  TfLiteTensor *input  = nullptr;
  TfLiteTensor *output = nullptr;
}

static bool computeGroundTruthAnomaly(float temp, float humi);
static void sendResultToWeb(float score, bool predictedAnomaly,
                            bool groundTruthAnomaly, float onlineAccuracy);

void setupTinyML()
{
  static tflite::MicroErrorReporter micro_error_reporter;
  error_reporter = &micro_error_reporter;

  model = tflite::GetModel(dht_anomaly_model_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION)
  {
    error_reporter->Report("Model schema %d not equal to supported %d.",
                           model->version(), TFLITE_SCHEMA_VERSION);
    return;
  }

  static tflite::AllOpsResolver resolver;

  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk)
  {
    error_reporter->Report("AllocateTensors() failed");
    return;
  }

  input  = interpreter->input(0);
  output = interpreter->output(0);

  Serial.println("TensorFlow Lite Micro initialized on ESP32.");
}

void tiny_ml_task(void *pvParameters)
{
  setupTinyML();

  uint32_t totalSamples   = 0;
  uint32_t correctSamples = 0;

  for (;;)
  {
    // Chuẩn bị input: dùng glob_temperature & glob_humidity
    if (input != nullptr &&
        input->type == kTfLiteFloat32 &&
        input->bytes >= 2 * sizeof(float))
    {
      input->data.f[0] = glob_temperature;
      input->data.f[1] = glob_humidity;
    }

    // Chạy suy luận
    if (interpreter == nullptr || interpreter->Invoke() != kTfLiteOk)
    {
      if (error_reporter)
        error_reporter->Report("Invoke failed");
      vTaskDelay(pdMS_TO_TICKS(2000));
      continue;
    }

    float result = output->data.f[0];            
    bool predictedAnomaly   = (result > 0.6f);    // >0.6 => bất thường
    bool groundTruthAnomaly = computeGroundTruthAnomaly(glob_temperature,
                                                        glob_humidity);

    totalSamples++;
    if (predictedAnomaly == groundTruthAnomaly)
      correctSamples++;

    float onlineAccuracy = (totalSamples > 0)
                               ? (100.0f * (float)correctSamples / (float)totalSamples)
                               : 0.0f;                      

    Serial.print("[TinyML] score=");
    Serial.print(result, 3);
    Serial.print(" pred=");
    Serial.print(predictedAnomaly ? "ANOM" : "OK");
    Serial.print(" gt=");
    Serial.print(groundTruthAnomaly ? "ANOM" : "OK");
    Serial.print(" acc=");
    Serial.print(onlineAccuracy, 1);
    Serial.println("%");

    // Gửi sang Web UI và cập nhật biến global cho CoreIoT
    sendResultToWeb(result, predictedAnomaly, groundTruthAnomaly, onlineAccuracy);

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

static bool computeGroundTruthAnomaly(float temp, float humi)
{
  bool tempBad = (temp < tempColdThreshold) || (temp > tempHotThreshold);
  bool humiBad = (humi < humiDryThreshold) || (humi > humiHumidThreshold);
  return tempBad || humiBad;
}

static void sendResultToWeb(float score, bool predictedAnomaly,
                            bool groundTruthAnomaly, float onlineAccuracy)
{
  // Cập nhật biến toàn cục TinyML (cho CoreIoT / dashboard khác)
  tinyml_score        = score;
  tinyml_accuracy     = onlineAccuracy;
  tinyml_pred_anomaly = predictedAnomaly;
  tinyml_gt_anomaly   = groundTruthAnomaly;

  // Gửi sang WebServer
  StaticJsonDocument<192> doc;
  doc["page"]  = "tinyml";
  doc["score"] = score;
  doc["pred"]  = predictedAnomaly ? "ANOM" : "OK";
  doc["gt"]    = groundTruthAnomaly ? "ANOM" : "OK";
  doc["acc"]   = onlineAccuracy;

  String json;
  serializeJson(doc, json);
  Webserver_sendata(json);
}

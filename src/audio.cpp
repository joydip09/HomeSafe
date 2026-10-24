#include "audio.h"
#include "app_state.h"
#include "config.h"
#include "network.h"
#include <HomeSafe_inferencing.h>

#include <Arduino.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <driver/i2s.h>

namespace HomeSafe {
namespace Audio {

namespace {

constexpr i2s_port_t I2S_PORT = I2S_NUM_0;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t READ_SAMPLES = 256;

int32_t rawBuffer[READ_SAMPLES];
int16_t sampleBuffer[READ_SAMPLES];

static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 1,
              "Model must use mono audio");
static_assert(EI_CLASSIFIER_FREQUENCY == SAMPLE_RATE,
              "Model must use 16 kHz audio");

constexpr size_t SLICE_SAMPLES = EI_CLASSIFIER_SLICE_SIZE;
int16_t inferenceBuffer[SLICE_SAMPLES];
size_t inferenceSampleCount = 0;

bool initialized = false;
bool acousticEventLatched = false;
bool hasEmittedAcousticEvent = false;
String latchedEventClass;
uint32_t lastAcousticEventMs = 0;

int getAudioData(size_t offset, size_t length, float *outPtr) {
  if (offset + length > SLICE_SAMPLES) {
    return -1;
  }

  return numpy::int16_to_float(inferenceBuffer + offset, outPtr, length);
}

bool initializeI2S() {
  i2s_config_t cfg = {};

  cfg.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = READ_SAMPLES;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  i2s_pin_config_t pins = {};
  pins.bck_io_num = Config::Pins::I2S_SCK;
  pins.ws_io_num = Config::Pins::I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = Config::Pins::I2S_SD;

  esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, nullptr);

  if (err != ESP_OK) {
    Serial.printf("[Audio] I2S install failed: %d\n", err);
    return false;
  }

  err = i2s_set_pin(I2S_PORT, &pins);

  if (err != ESP_OK) {
    Serial.printf("[Audio] I2S pin setup failed: %d\n", err);
    i2s_driver_uninstall(I2S_PORT);
    return false;
  }

  i2s_zero_dma_buffer(I2S_PORT);
  Serial.println("[Audio] I2S initialized: 16 kHz, 32-bit slots");
  return true;
}

void runInference() {
  signal_t signal;
  signal.total_length = SLICE_SAMPLES;
  signal.get_data = getAudioData;

  ei_impulse_result_t result = {};

  EI_IMPULSE_ERROR err = run_classifier_continuous(&signal, &result, false);
  const uint32_t inferenceTimestamp = millis();

  if (err != EI_IMPULSE_OK) {
    AudioData &audio = getState().audio;
    audio.detectedClass = "";
    audio.confidence = 0.0f;
    audio.inferenceValid = false;
    audio.lastInference = inferenceTimestamp;
    acousticEventLatched = false;
    latchedEventClass = "";
    Serial.printf("[Audio] Classifier error: %d\n", static_cast<int>(err));
    return;
  }

  size_t bestIndex = 0;
  for (size_t i = 1; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
    if (result.classification[i].value >
        result.classification[bestIndex].value) {
      bestIndex = i;
    }
  }

  const char *detectedClass = result.classification[bestIndex].label;
  const float confidence = result.classification[bestIndex].value;
  AudioData &audio = getState().audio;
  audio.detectedClass = detectedClass;
  audio.confidence = confidence;
  audio.inferenceValid = true;
  audio.lastInference = inferenceTimestamp;

  const bool isNormal = strcmp(detectedClass, "normal") == 0;
  const bool isEligible =
      !isNormal && confidence >= Config::Audio::CLASSIFICATION_CONFIDENCE_THRESHOLD;

  if (!isEligible) {
    acousticEventLatched = false;
    latchedEventClass = "";
  } else if (!acousticEventLatched ||
             latchedEventClass != detectedClass) {
    const bool cooldownElapsed =
        !hasEmittedAcousticEvent ||
        inferenceTimestamp - lastAcousticEventMs >=
        Config::Audio::EVENT_COOLDOWN_MS;

    if (cooldownElapsed &&
        Network::queueAcousticEvent(detectedClass, confidence)) {
      acousticEventLatched = true;
      hasEmittedAcousticEvent = true;
      latchedEventClass = detectedClass;
      lastAcousticEventMs = inferenceTimestamp;
    }
  }

  Serial.println("\n[Audio] Classification:");

  for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
    Serial.printf("  %-12s %.3f\n", result.classification[i].label,
                  result.classification[i].value);
  }

  Serial.printf("[Audio] DSP: %lu ms | Classification: %lu ms\n",
                static_cast<unsigned long>(result.timing.dsp),
                static_cast<unsigned long>(result.timing.classification));
}

} // namespace

bool begin() {
  Serial.println("[Audio] Initializing INMP441...");

  if (!initializeI2S()) {
    Serial.println("[Audio] Initialization failed");
    return false;
  }

  inferenceSampleCount = 0;
  acousticEventLatched = false;
  hasEmittedAcousticEvent = false;
  latchedEventClass = "";
  lastAcousticEventMs = 0;
  run_classifier_init();
  initialized = true;

  Serial.printf("[Audio] TinyML ready | Slice: %u samples\n",
                static_cast<unsigned>(SLICE_SAMPLES));
  return true;
}

void update() {
  if (!initialized) {
    return;
  }

  size_t bytesRead = 0;
  esp_err_t err = i2s_read(I2S_PORT, rawBuffer, sizeof(rawBuffer), &bytesRead,
                           pdMS_TO_TICKS(20));

  if (err != ESP_OK || bytesRead < sizeof(int32_t)) {
    return;
  }

  const size_t count = bytesRead / sizeof(int32_t);

  for (size_t i = 0; i < count; ++i) {
    // INMP441 24-bit sample is carried in a 32-bit I2S slot.
    // Convert its upper 16 significant bits to signed PCM.
    const int32_t sample = rawBuffer[i] >> 16;
    sampleBuffer[i] = static_cast<int16_t>(sample);

    inferenceBuffer[inferenceSampleCount++] = sampleBuffer[i];

    if (inferenceSampleCount == SLICE_SAMPLES) {
      // Diagnostic levels for this completed slice.
      int32_t peak = 0;
      uint64_t sumSquares = 0;

      for (size_t j = 0; j < SLICE_SAMPLES; ++j) {
        const int32_t s = inferenceBuffer[j];
        const int32_t magnitude = (s < 0) ? -s : s;

        if (magnitude > peak) {
          peak = magnitude;
        }

        sumSquares += static_cast<uint64_t>(static_cast<int64_t>(s) * s);
      }

      const uint32_t rms = static_cast<uint32_t>(
          std::sqrt(static_cast<double>(sumSquares) / SLICE_SAMPLES));

      Serial.printf("[Audio] PCM peak=%ld RMS=%lu\n", static_cast<long>(peak),
                    static_cast<unsigned long>(rms));

      runInference();
      inferenceSampleCount = 0;
    }
  }
}

bool isInitialized() { return initialized; }

} // namespace Audio
} // namespace HomeSafe
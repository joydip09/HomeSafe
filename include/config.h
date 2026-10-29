#ifndef HOMESAFE_CONFIG_H
#define HOMESAFE_CONFIG_H

#include <Arduino.h>

namespace HomeSafe {
namespace Config {

namespace Pins {
constexpr uint8_t I2S_SCK = 4;
constexpr uint8_t I2S_WS = 5;
constexpr uint8_t I2S_SD = 6;

constexpr uint8_t DHT11_DATA = 45;

constexpr uint8_t MQ2_AO = 9;
constexpr uint8_t MQ135_AO = 8;

constexpr uint8_t FLAME_AO = 1;
constexpr uint8_t FLAME_DO = 2;

constexpr uint8_t PIR_OUT = 15;

constexpr uint8_t LED_RED = 40;
constexpr uint8_t LED_GREEN = 39;
constexpr uint8_t LED_BLUE = 38;

constexpr uint8_t BUZZER = 21;
// constexpr uint8_t BUTTON = 0;
} // namespace Pins

namespace Timing {
constexpr uint32_t DHT11_INTERVAL_MS = 2000;
constexpr uint32_t MQ_SENSOR_INTERVAL_MS = 250;
constexpr uint32_t FLAME_SENSOR_INTERVAL_MS = 100;
constexpr uint32_t PIR_POLL_INTERVAL_MS = 20;
constexpr uint32_t SENSOR_UPDATE_INTERVAL_MS = 1000;
constexpr uint32_t AUDIO_INFERENCE_INTERVAL_MS = 100;
} // namespace Timing

namespace Network {
constexpr uint16_t WEBSOCKET_PORT = 81;
constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 5000;
constexpr uint32_t CLIENT_HEARTBEAT_INTERVAL_MS = 10000;
} // namespace Network

namespace Safety {
constexpr uint32_t HAZARD_CONFIRMATION_MS = 1000;
constexpr uint32_t ALARM_COOLDOWN_MS = 5000;

// Temporary raw ADC thresholds.
// Calibrate these using actual sensor readings before deployment.
constexpr int MQ2_ADC_THRESHOLD = 900;
constexpr int MQ135_ADC_THRESHOLD = 400;
} // namespace Safety

namespace Audio {
constexpr float CLASSIFICATION_CONFIDENCE_THRESHOLD = 0.80f;
constexpr uint32_t EVENT_COOLDOWN_MS = 3000;
} // namespace Audio

} // namespace Config

} // namespace HomeSafe

#endif
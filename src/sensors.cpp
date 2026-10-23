#include "sensors.h"

#include "app_state.h"
#include "config.h"

#include <Arduino.h>
#include <DHT.h>
#include <math.h>

namespace HomeSafe {
namespace Sensors {

namespace {

DHT dht(Config::Pins::DHT11_DATA, DHT11);

bool initialized = false;

uint32_t lastDhtRead = 0;
uint32_t lastMQRead = 0;

void updateDHT11() {
  const uint32_t now = millis();

  if (now - lastDhtRead < Config::Timing::DHT11_INTERVAL_MS) {
    return;
  }

  lastDhtRead = now;

  const float temperature = dht.readTemperature();
  const float humidity = dht.readHumidity();

  HomeSafeState &state = getState();

  if (isnan(temperature) || isnan(humidity)) {
    state.environment.temperatureC.valid = false;
    state.environment.humidityRH.valid = false;

    // Serial.println("[Sensors] DHT11 read failed");
    return;
  }

  state.environment.temperatureC.value = temperature;
  state.environment.temperatureC.valid = true;
  state.environment.temperatureC.lastUpdated = now;

  state.environment.humidityRH.value = humidity;
  state.environment.humidityRH.valid = true;
  state.environment.humidityRH.lastUpdated = now;

  // Serial.printf("[Sensors] Temperature: %.1f C | Humidity: %.1f %%\n",
  //               temperature, humidity);
}

void updateMQSensors() {
  const uint32_t now = millis();

  if (now - lastMQRead < Config::Timing::MQ_SENSOR_INTERVAL_MS) {
    return;
  }

  lastMQRead = now;

  const int mq2Reading = analogRead(Config::Pins::MQ2_AO);
  const int mq135Reading = analogRead(Config::Pins::MQ135_AO);

  HomeSafeState &state = getState();

  state.environment.mq2ADC.value = static_cast<float>(mq2Reading);
  state.environment.mq2ADC.valid = true;
  state.environment.mq2ADC.lastUpdated = now;

  state.environment.mq135ADC.value = static_cast<float>(mq135Reading);
  state.environment.mq135ADC.valid = true;
  state.environment.mq135ADC.lastUpdated = now;

  // Serial.printf("[Sensors] MQ-2: %d | MQ-135: %d\n", mq2Reading, mq135Reading);
}

void updateFlameSensor() {
  static uint32_t lastFlameRead = 0;

  const uint32_t now = millis();

  if (now - lastFlameRead < Config::Timing::FLAME_SENSOR_INTERVAL_MS) {
    return;
  }

  lastFlameRead = now;

  const int analogValue = analogRead(Config::Pins::FLAME_AO);
  const bool digitalValue = digitalRead(Config::Pins::FLAME_DO);

  HomeSafeState &state = getState();

  state.environment.flameADC.value = static_cast<float>(analogValue);
  state.environment.flameADC.valid = true;
  state.environment.flameADC.lastUpdated = now;

  // Many flame modules pull DO LOW when a flame is detected.
  // Confirm the polarity of your particular module.
  state.environment.flameDetected = (digitalValue == LOW);
  state.environment.flameValid = true;

  // Serial.printf("[Sensors] Flame AO: %d | DO: %s | Detected: %s\n", analogValue,
  //               digitalValue == HIGH ? "HIGH" : "LOW",
  //               state.environment.flameDetected ? "YES" : "NO");
}

void updatePIR() {
  static uint32_t lastPIRRead = 0;

  const uint32_t now = millis();

  if (now - lastPIRRead < Config::Timing::PIR_POLL_INTERVAL_MS) {
    return;
  }

  lastPIRRead = now;

  const bool motionDetected = (digitalRead(Config::Pins::PIR_OUT) == HIGH);

  HomeSafeState &state = getState();

  state.environment.pirMotion = motionDetected;
  // Serial.printf("[Sensors] PIR motion: %s\n",
  //               motionDetected ? "YES" : "NO"); // for testing
  state.environment.pirValid = true;
}

} // namespace

void begin() {
  // Serial.println("[Sensors] Initializing sensors...");

  // Match the 12-bit ADC resolution used in your earlier tests.
  analogReadResolution(12);

  dht.begin();

  // Allow the first reading immediately.
  const uint32_t now = millis();
  lastDhtRead = now - Config::Timing::DHT11_INTERVAL_MS;
  lastMQRead = now - Config::Timing::MQ_SENSOR_INTERVAL_MS;

  pinMode(Config::Pins::FLAME_AO, INPUT);
  pinMode(Config::Pins::FLAME_DO, INPUT);

  pinMode(Config::Pins::PIR_OUT, INPUT);

  initialized = true;

  // Serial.println("[Sensors] DHT11 initialized");
  // Serial.println("[Sensors] MQ-2 and MQ-135 ADC acquisition initialized");
}

void update() {
  if (!initialized) {
    return;
  }

  updateDHT11();
  updateMQSensors();
  updateFlameSensor();
  updatePIR();
}

bool isInitialized() { return initialized; }

} // namespace Sensors
} // namespace HomeSafe
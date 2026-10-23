#include "outputs.h"

#include "config.h"

#include <Arduino.h>

namespace HomeSafe {
namespace Outputs {

namespace {

bool initialized = false;
bool buzzerActive = false;

// Independent alarm requests from each subsystem.
bool environmentalAlarmActive = false;
bool securityAlarmActive = false;

// Invert brightness because the RGB LED is common-anode.
// 0 brightness   -> GPIO HIGH (off)
// 255 brightness -> GPIO LOW  (full brightness)
void writeLED(uint8_t pin, uint8_t brightness) {
  analogWrite(pin, 255 - brightness);
}

// Combine alarm states and control the physical outputs.
void refreshAlarmOutputs() {
  if (!initialized) {
    return;
  }

  const bool anyAlarmActive = environmentalAlarmActive || securityAlarmActive;

  if (anyAlarmActive) {
    setRGB(255, 0, 0);
    buzzerOn();
  } else {
    buzzerOff();
    turnLEDOff();
  }
}

} // namespace

void begin() {
  // Serial.println("[Outputs] Initializing outputs...");

  // Configure RGB LED pins.
  pinMode(Config::Pins::LED_RED, OUTPUT);
  pinMode(Config::Pins::LED_GREEN, OUTPUT);
  pinMode(Config::Pins::LED_BLUE, OUTPUT);

  // Start with the common-anode LED switched off.
  writeLED(Config::Pins::LED_RED, 0);
  writeLED(Config::Pins::LED_GREEN, 0);
  writeLED(Config::Pins::LED_BLUE, 0);

  // Configure the active buzzer.
  pinMode(Config::Pins::BUZZER, OUTPUT);
  digitalWrite(Config::Pins::BUZZER, LOW);

  // Reset the output controller's state.
  buzzerActive = false;
  environmentalAlarmActive = false;
  securityAlarmActive = false;

  initialized = true;

  refreshAlarmOutputs();

  // Serial.println("[Outputs] RGB LED and buzzer initialized");
}

void setRGB(uint8_t red, uint8_t green, uint8_t blue) {
  if (!initialized) {
    return;
  }

  writeLED(Config::Pins::LED_RED, red);
  writeLED(Config::Pins::LED_GREEN, green);
  writeLED(Config::Pins::LED_BLUE, blue);
}

void turnLEDOff() { setRGB(0, 0, 0); }

void buzzerOn() {
  if (!initialized || buzzerActive) {
    return;
  }

  digitalWrite(Config::Pins::BUZZER, HIGH);
  buzzerActive = true;
}

void buzzerOff() {
  if (!initialized || !buzzerActive) {
    return;
  }

  digitalWrite(Config::Pins::BUZZER, LOW);
  buzzerActive = false;
}

bool isBuzzerActive() { return buzzerActive; }

void setEnvironmentalAlarm(bool active) {
  if (environmentalAlarmActive == active) {
    return;
  }

  environmentalAlarmActive = active;
  refreshAlarmOutputs();
}

void setSecurityAlarm(bool active) {
  if (securityAlarmActive == active) {
    return;
  }

  securityAlarmActive = active;
  refreshAlarmOutputs();
}

} // namespace Outputs
} // namespace HomeSafe
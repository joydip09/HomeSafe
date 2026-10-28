#include "safety.h"

#include "app_state.h"
#include "config.h"
#include "network.h"
#include "outputs.h"

namespace HomeSafe {
namespace Safety {

namespace {

bool initialized = false;

// Individual confirmed hazard states.
bool flameHazardActive = false;
bool mq2HazardActive = false;
bool mq135HazardActive = false;

// Overall environmental hazard state.
bool hazardActive = false;

uint32_t lastAlarmClearedAt = 0;
bool alarmCooldownActive = false;

// Confirmation tracking.
bool flamePending = false;
bool mq2Pending = false;
bool mq135Pending = false;

uint32_t flameDetectedSince = 0;
uint32_t mq2DetectedSince = 0;
uint32_t mq135DetectedSince = 0;

void updateConfirmedHazard(bool sensorValid, bool conditionDetected,
                           bool &pending, uint32_t &detectedSince,
                           bool &confirmedHazard, uint32_t now,
                           const char *hazardName) {
  // Do not make decisions using an unavailable sensor.
  if (!sensorValid) {
    pending = false;
    detectedSince = 0;
    return;
  }

  // Reset confirmation if the detection condition clears.
  if (!conditionDetected) {
    pending = false;
    detectedSince = 0;
    confirmedHazard = false;
    return;
  }

  // Begin the confirmation window.
  if (!pending) {
    pending = true;
    detectedSince = now;
    return;
  }

  // Confirm only after continuous detection.
  if (!confirmedHazard &&
      now - detectedSince >= Config::Safety::HAZARD_CONFIRMATION_MS) {
    confirmedHazard = true;

    Serial.printf("[Safety] CONFIRMED %s HAZARD\n", hazardName);
  }
}

void updateOverallHazard() {
  const uint32_t now = millis();

  const bool anyHazard =
      flameHazardActive || mq2HazardActive || mq135HazardActive;

  // No confirmed hazards remain.
  if (!anyHazard) {
    if (hazardActive) {
      hazardActive = false;

      // Release only the environmental alarm request.
      // A security alarm may still keep the outputs active.
      Outputs::setEnvironmentalAlarm(false);
      Network::queueAlarmEvent("environmental", "cleared", false,
                               getState().security.alarmActive);

      lastAlarmClearedAt = now;
      alarmCooldownActive = true;

      Serial.println("[Safety] All environmental hazards cleared");
    }

    // Expire the cooldown without blocking.
    if (alarmCooldownActive &&
        now - lastAlarmClearedAt >= Config::Safety::ALARM_COOLDOWN_MS) {
      alarmCooldownActive = false;
      Serial.println("[Safety] Alarm cooldown expired");
    }

    return;
  }

  // A hazard exists, but the alarm has not been activated.
  if (!hazardActive) {
    if (alarmCooldownActive &&
        now - lastAlarmClearedAt < Config::Safety::ALARM_COOLDOWN_MS) {
      // Keep hazard state separate from alarm output state.
      // Do not activate the environmental alarm during cooldown.
      return;
    }

    // First activation, or cooldown has expired.
    hazardActive = true;
    alarmCooldownActive = false;

    // Request the environmental alarm through the central controller.
    Outputs::setEnvironmentalAlarm(true);
    Network::queueAlarmEvent("environmental", "activated", true,
                             getState().security.alarmActive);

    Serial.println("[Safety] Environmental hazard alarm activated");
  }
}

} // namespace

void begin() {
  Serial.println("[Safety] Initializing safety logic...");

  flameHazardActive = false;
  mq2HazardActive = false;
  mq135HazardActive = false;
  hazardActive = false;

  flamePending = false;
  mq2Pending = false;
  mq135Pending = false;

  flameDetectedSince = 0;
  mq2DetectedSince = 0;
  mq135DetectedSince = 0;

  lastAlarmClearedAt = 0;
  alarmCooldownActive = false;

  initialized = true;

  Serial.println("[Safety] Safety logic initialized");
}

void update() {
  if (!initialized) {
    return;
  }

  const uint32_t now = millis();
  EnvironmentData &environment = getState().environment;
  const bool previousFlameHazard = flameHazardActive;
  const bool previousMq2Hazard = mq2HazardActive;
  const bool previousMq135Hazard = mq135HazardActive;

  // Flame sensor: digital detection is active LOW,
  // interpreted by the existing sensor driver.
  updateConfirmedHazard(environment.flameValid, environment.flameDetected,
                        flamePending, flameDetectedSince, flameHazardActive,
                        now, "FLAME");

  // MQ-2: raw ADC threshold, to be calibrated later.
  updateConfirmedHazard(
      environment.mq2ADC.valid,
      environment.mq2ADC.value >= Config::Safety::MQ2_ADC_THRESHOLD, mq2Pending,
      mq2DetectedSince, mq2HazardActive, now, "MQ-2");

  // MQ-135: raw ADC threshold, to be calibrated later.
  updateConfirmedHazard(
      environment.mq135ADC.valid,
      environment.mq135ADC.value >= Config::Safety::MQ135_ADC_THRESHOLD,
      mq135Pending, mq135DetectedSince, mq135HazardActive, now, "MQ-135");

  environment.flameHazardActive = flameHazardActive;
  environment.mq2HazardActive = mq2HazardActive;
  environment.mq135HazardActive = mq135HazardActive;
  environment.hazardActive =
      flameHazardActive || mq2HazardActive || mq135HazardActive;

  if (previousFlameHazard != flameHazardActive) {
    Network::queueHazardEvent("flame", flameHazardActive,
                              environment.hazardActive);
  }
  if (previousMq2Hazard != mq2HazardActive) {
    Network::queueHazardEvent("mq2", mq2HazardActive,
                              environment.hazardActive);
  }
  if (previousMq135Hazard != mq135HazardActive) {
    Network::queueHazardEvent("mq135", mq135HazardActive,
                              environment.hazardActive);
  }

  updateOverallHazard();
}

bool isHazardActive() { return hazardActive; }

} // namespace Safety
} // namespace HomeSafe
#include "security.h"

#include "app_state.h"
#include "config.h"
#include "outputs.h"
#include <Preferences.h>
#include <time.h>

namespace HomeSafe {
namespace Security {

namespace {

Preferences preferences;

constexpr const char *NAMESPACE = "homesafe";
constexpr const char *KEY_ARMED = "sec_armed";
constexpr const char *KEY_SCHEDULE = "sec_schedule";

constexpr const char *KEY_ARM_HOUR = "arm_hour";
constexpr const char *KEY_ARM_MINUTE = "arm_min";
constexpr const char *KEY_DISARM_HOUR = "disarm_hour";
constexpr const char *KEY_DISARM_MINUTE = "disarm_min";

uint8_t armHour = 22;
uint8_t armMinute = 0;

uint8_t disarmHour = 6;
uint8_t disarmMinute = 0;

bool scheduleTimeValid = false;

bool initialized = false;
bool securityArmed = false;
bool scheduleEnabled = false;
bool securityAlarmActive = false;

bool motionPending = false;
uint32_t motionDetectedSince = 0;

void updateMotionDetection() {
  const uint32_t now = millis();
  EnvironmentData &environment = getState().environment;

  // Ignore motion data until the sensor has produced a reading.
  if (!environment.pirValid) {
    motionPending = false;
    motionDetectedSince = 0;
    return;
  }

  // Security monitoring is only active while armed.
  if (!securityArmed) {
    motionPending = false;
    motionDetectedSince = 0;
    return;
  }

  if (!environment.pirMotion) {
    motionPending = false;
    motionDetectedSince = 0;
    return;
  }

  // Start the confirmation timer.
  if (!motionPending) {
    motionPending = true;
    motionDetectedSince = now;
    return;
  }

  // Confirm sustained motion before triggering the alarm.
  if (!securityAlarmActive &&
      now - motionDetectedSince >= Config::Safety::HAZARD_CONFIRMATION_MS) {
    securityAlarmActive = true;

    getState().security.alarmActive = true;

    Serial.println("[Security] INTRUSION DETECTED");

    Outputs::setRGB(255, 0, 0);
    Outputs::buzzerOn();
  }
}

bool validateSchedule(uint8_t startHour, uint8_t startMinute, uint8_t endHour,
                      uint8_t endMinute) {
  if (startHour > 23 || endHour > 23 || startMinute > 59 || endMinute > 59) {
    return false;
  }

  // Identical start and end times are ambiguous.
  return startHour != endHour || startMinute != endMinute;
}

void loadSchedule() {
  armHour = preferences.getUChar(KEY_ARM_HOUR, 22);
  armMinute = preferences.getUChar(KEY_ARM_MINUTE, 0);

  disarmHour = preferences.getUChar(KEY_DISARM_HOUR, 6);
  disarmMinute = preferences.getUChar(KEY_DISARM_MINUTE, 0);

  if (!validateSchedule(armHour, armMinute, disarmHour, disarmMinute)) {
    armHour = 22;
    armMinute = 0;
    disarmHour = 6;
    disarmMinute = 0;

    Serial.println("[Security] Invalid stored schedule; using defaults");
  }

  SecurityState &state = getState().security;

  state.armHour = armHour;
  state.armMinute = armMinute;
  state.disarmHour = disarmHour;
  state.disarmMinute = disarmMinute;
}

void updateSchedule() {
  if (!scheduleEnabled) {
    return;
  }

  const time_t now = time(nullptr);

  // Reject obviously invalid system time.
  // This check requires a plausible date, not just a nonzero clock.
  if (now < 1760000000) {
    if (scheduleTimeValid) {
      Serial.println("[Security] Clock became invalid");
    }

    scheduleTimeValid = false;
    return;
  }

  struct tm localTimeInfo;

  if (localtime_r(&now, &localTimeInfo) == nullptr) {
    scheduleTimeValid = false;
    return;
  }

  scheduleTimeValid = true;

  const int currentMinute = localTimeInfo.tm_hour * 60 + localTimeInfo.tm_min;

  const int startMinute = armHour * 60 + armMinute;

  const int endMinute = disarmHour * 60 + disarmMinute;

  bool shouldBeArmed;

  if (startMinute < endMinute) {
    // Same-day schedule, e.g. 09:00–17:00.
    shouldBeArmed = currentMinute >= startMinute && currentMinute < endMinute;
  } else {
    // Overnight schedule, e.g. 22:00–06:00.
    shouldBeArmed = currentMinute >= startMinute || currentMinute < endMinute;
  }

  if (securityArmed != shouldBeArmed) {
    Serial.printf("[Security] Schedule changing mode to %s\n",
                  shouldBeArmed ? "ARMED" : "DISARMED");

    setArmed(shouldBeArmed);
  }
}

} // namespace

bool setSchedule(uint8_t newArmHour, uint8_t newArmMinute,
                 uint8_t newDisarmHour, uint8_t newDisarmMinute) {
  if (!initialized || !validateSchedule(newArmHour, newArmMinute, newDisarmHour,
                                        newDisarmMinute)) {
    return false;
  }

  armHour = newArmHour;
  armMinute = newArmMinute;
  disarmHour = newDisarmHour;
  disarmMinute = newDisarmMinute;

  preferences.putUChar(KEY_ARM_HOUR, armHour);
  preferences.putUChar(KEY_ARM_MINUTE, armMinute);
  preferences.putUChar(KEY_DISARM_HOUR, disarmHour);
  preferences.putUChar(KEY_DISARM_MINUTE, disarmMinute);

  SecurityState &state = getState().security;

  state.armHour = armHour;
  state.armMinute = armMinute;
  state.disarmHour = disarmHour;
  state.disarmMinute = disarmMinute;

  Serial.println("[Security] Schedule saved");
  return true;
}

bool isScheduleValid() {
  return validateSchedule(armHour, armMinute, disarmHour, disarmMinute);
}

void begin() {
  Serial.println("[Security] Initializing security module...");

  if (!preferences.begin(NAMESPACE, false)) {
    Serial.println("[Security] WARNING: Preferences initialization failed");
    securityArmed = false;
    scheduleEnabled = false;
  } else {
    securityArmed = preferences.getBool(KEY_ARMED, false);
    scheduleEnabled = preferences.getBool(KEY_SCHEDULE, false);
  }

  securityAlarmActive = false;
  motionPending = false;
  motionDetectedSince = 0;

  // Bangladesh Standard Time: UTC+6.
  setenv("TZ", "UTC-6", 1);
  tzset();

  // Request NTP synchronization.
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  loadSchedule();

  SecurityState &state = getState().security;
  state.armed = securityArmed;
  state.scheduleEnabled = scheduleEnabled;
  state.alarmActive = false;

  initialized = true;

  Serial.printf("[Security] Initialized. Armed: %s | Schedule: %s\n",
                securityArmed ? "YES" : "NO", scheduleEnabled ? "YES" : "NO");
}

void update() {
  if (!initialized) {
    return;
  }

  updateMotionDetection();
  updateSchedule();
}

void setArmed(bool armed) {
  if (!initialized) {
    return;
  }

  securityArmed = armed;
  preferences.putBool(KEY_ARMED, securityArmed);

  SecurityState &state = getState().security;
  state.armed = securityArmed;

  // Reset pending motion when the mode changes.
  motionPending = false;
  motionDetectedSince = 0;

  if (!securityArmed) {
    securityAlarmActive = false;
    state.alarmActive = false;

    Outputs::buzzerOff();
    Outputs::turnLEDOff();

    Serial.println("[Security] System disarmed");
  } else {
    Serial.println("[Security] System armed");
  }
}

bool isArmed() { return securityArmed; }

void setScheduleEnabled(bool enabled) {
  if (!initialized) {
    return;
  }

  scheduleEnabled = enabled;
  preferences.putBool(KEY_SCHEDULE, scheduleEnabled);

  getState().security.scheduleEnabled = scheduleEnabled;

  Serial.printf("[Security] Schedule %s\n",
                scheduleEnabled ? "enabled" : "disabled");
}

bool isScheduleEnabled() { return scheduleEnabled; }

void silenceAlarm() {
  if (!securityAlarmActive) {
    return;
  }

  securityAlarmActive = false;
  getState().security.alarmActive = false;

  Outputs::buzzerOff();
  Outputs::turnLEDOff();

  Serial.println("[Security] Alarm silenced");
}

} // namespace Security
} // namespace HomeSafe
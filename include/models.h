#ifndef HOMESAFE_MODELS_H
#define HOMESAFE_MODELS_H

#include "protocol.h"

#include <Arduino.h>

namespace HomeSafe {

struct SensorValue {
  float value = 0.0f;
  bool valid = false;
  uint32_t lastUpdated = 0;
};

struct EnvironmentData {
  SensorValue temperatureC;
  SensorValue humidityRH;
  SensorValue mq2ADC;
  SensorValue mq135ADC;
  SensorValue flameADC;

  bool flameDetected = false;
  bool flameValid = false;

  bool flameHazardActive = false;
  bool mq2HazardActive = false;
  bool mq135HazardActive = false;
  bool hazardActive = false;

  bool pirMotion = false;
  bool pirValid = false;
  uint32_t pirLastUpdated = 0;
};

struct AudioData {
  String detectedClass;
  float confidence = 0.0f;
  bool inferenceValid = false;
  uint32_t lastInference = 0;
};

struct SecurityState {
  bool armed = false;
  bool scheduleEnabled = false;
  bool alarmActive = false;
  uint8_t armHour = 22;
  uint8_t armMinute = 0;

  uint8_t disarmHour = 6;
  uint8_t disarmMinute = 0;
};

struct HomeSafeEvent {
  Protocol::EventId eventId = 0;
  Protocol::SequenceNumber sequence = 0;
  Protocol::EventType type = Protocol::EventType::Security;
  String name;
  String details;
  uint32_t timestampMs = 0;
  bool active = true;
};

struct DeviceStatus {
  bool wifiConnected = false;
  String ipAddress;
  uint32_t uptimeMs = 0;
  bool sensorsInitialized = false;
  bool audioAvailable = false;
  String firmwareVersion = Protocol::FIRMWARE_VERSION;
  uint8_t protocolVersion = Protocol::VERSION;
  uint32_t websocketClientCount = 0;
  bool timeValid = false;
};

struct AlarmState {
  bool environmentalActive = false;
  bool securityActive = false;
  bool anyActive = false;
};

struct HomeSafeState {
  EnvironmentData environment;
  AudioData audio;
  SecurityState security;
  DeviceStatus device;
  AlarmState alarms;
};

} // namespace HomeSafe

#endif
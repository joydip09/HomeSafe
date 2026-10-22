#ifndef HOMESAFE_MODELS_H
#define HOMESAFE_MODELS_H

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
};

enum class EventType : uint8_t { Hazard, Acoustic, Security, System };

struct HomeSafeEvent {
  uint32_t id = 0;
  EventType type = EventType::System;
  String name;
  String details;
  uint32_t timestamp = 0;
  bool active = true;
};

struct DeviceStatus {
  bool wifiConnected = false;
  bool websocketClientConnected = false;
  bool audioAvailable = false;
  bool sensorsInitialized = false;
  uint32_t uptimeMs = 0;
};

struct HomeSafeState {
  EnvironmentData environment;
  AudioData audio;
  SecurityState security;
  DeviceStatus device;
};

} // namespace HomeSafe

#endif
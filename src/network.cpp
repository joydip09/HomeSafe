#include "network.h"
#include "app_state.h"
#include "audio.h"
#include "config.h"
#include "protocol.h"
#include "secret.h"
#include "security.h"
#include "sensors.h"

#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

namespace HomeSafe {
namespace Network {

namespace {

WebSocketsServer webSocket(Protocol::DEFAULT_PORT);

bool wifiConnected = false;

uint32_t lastReconnectAttempt = 0;
Protocol::SequenceNumber outboundSequence = 0;
Protocol::EventId nextEventId = 0;
bool stateBaselineInitialized = false;
bool acousticEventPending = false;
String pendingAcousticClass;
float pendingAcousticConfidence = 0.0f;

enum class PendingEventKind : uint8_t {
  Hazard,
  Security,
  Alarm,
};

struct PendingEvent {
  PendingEventKind kind;
  String source;
  String name;
  String action;
  bool active;
  bool armed;
  bool scheduleEnabled;
  bool alarmActive;
  bool environmentalActive;
  bool securityActive;
};

constexpr size_t PENDING_EVENT_CAPACITY = 8;
PendingEvent pendingEvents[PENDING_EVENT_CAPACITY];
size_t pendingEventHead = 0;
size_t pendingEventCount = 0;

uint8_t connectedClientCount() {
  uint8_t count = 0;

  for (uint8_t clientId = 0; clientId < WEBSOCKETS_SERVER_CLIENT_MAX;
       ++clientId) {
    if (webSocket.clientIsConnected(clientId)) {
      ++count;
    }
  }

  return count;
}

void serializeSensorValue(JsonObject object, const SensorValue &value) {
  object["valid"] = value.valid;

  if (value.valid) {
    object["value"] = value.value;
    object["last_updated_ms"] = value.lastUpdated;
  } else {
    object["value"] = nullptr;
    object["last_updated_ms"] = nullptr;
  }
}

void serializeDevice(JsonObject device) {
  const HomeSafeState &state = getState();
  const bool wifiIsConnected = WiFi.status() == WL_CONNECTED;

  device["wifi_connected"] = wifiIsConnected;
  if (wifiIsConnected) {
    device["ip_address"] = WiFi.localIP().toString();
  } else {
    device["ip_address"] = nullptr;
  }
  device["uptime_ms"] = millis();
  device["sensors_initialized"] = Sensors::isInitialized();
  device["audio_available"] = Audio::isInitialized();
  device["firmware_version"] = Protocol::FIRMWARE_VERSION;
  device["protocol_version"] = Protocol::VERSION;
  device["websocket_client_count"] = connectedClientCount();
  device["time_valid"] = state.device.timeValid;
}

void serializeEnvironment(JsonObject environment) {
  const EnvironmentData &data = getState().environment;

  serializeSensorValue(environment["temperature_c"].to<JsonObject>(),
                       data.temperatureC);
  serializeSensorValue(environment["humidity_rh"].to<JsonObject>(),
                       data.humidityRH);
  serializeSensorValue(environment["mq2_adc"].to<JsonObject>(), data.mq2ADC);
  serializeSensorValue(environment["mq135_adc"].to<JsonObject>(),
                       data.mq135ADC);

  JsonObject flame = environment["flame"].to<JsonObject>();
  serializeSensorValue(flame["adc"].to<JsonObject>(), data.flameADC);
  flame["detected"] = data.flameDetected;
  flame["valid"] = data.flameValid;

  JsonObject pir = environment["pir"].to<JsonObject>();
  pir["motion"] = data.pirMotion;
  pir["valid"] = data.pirValid;
  pir["last_updated_ms"] = data.pirValid ? data.pirLastUpdated : 0;
  if (!data.pirValid) {
    pir["last_updated_ms"] = nullptr;
  }
}

void serializeHazards(JsonObject hazards) {
  const EnvironmentData &data = getState().environment;
  hazards["flame_active"] = data.flameHazardActive;
  hazards["mq2_active"] = data.mq2HazardActive;
  hazards["mq135_active"] = data.mq135HazardActive;
  hazards["environmental_active"] = data.hazardActive;
}

void serializeAudio(JsonObject audio) {
  const AudioData &data = getState().audio;
  audio["inference_valid"] = data.inferenceValid;
  if (data.inferenceValid) {
    audio["detected_class"] = data.detectedClass;
    audio["confidence"] = data.confidence;
    audio["last_inference_ms"] = data.lastInference;
  } else {
    audio["detected_class"] = nullptr;
    audio["confidence"] = nullptr;
    audio["last_inference_ms"] = nullptr;
  }
}

void serializeSecurity(JsonObject security) {
  const SecurityState &data = getState().security;
  security["armed"] = data.armed;
  security["schedule_enabled"] = data.scheduleEnabled;
  security["arm_hour"] = data.armHour;
  security["arm_minute"] = data.armMinute;
  security["disarm_hour"] = data.disarmHour;
  security["disarm_minute"] = data.disarmMinute;
  security["alarm_active"] = data.alarmActive;
}

void serializeAlarms(JsonObject alarms) {
  const HomeSafeState &state = getState();
  const bool environmentalActive = state.environment.hazardActive;
  const bool securityActive = state.security.alarmActive;
  alarms["environmental_active"] = environmentalActive;
  alarms["security_active"] = securityActive;
  alarms["any_active"] = environmentalActive || securityActive;
}

void serializeState(JsonObject stateObject) {
  serializeDevice(stateObject["device"].to<JsonObject>());
  serializeEnvironment(stateObject["environment"].to<JsonObject>());
  serializeHazards(stateObject["hazards"].to<JsonObject>());
  serializeAudio(stateObject["audio"].to<JsonObject>());
  serializeSecurity(stateObject["security"].to<JsonObject>());
  serializeAlarms(stateObject["alarms"].to<JsonObject>());
}

struct SensorSignature {
  float value;
  bool valid;
  uint32_t lastUpdated;
};

struct StateSignature {
  bool wifiConnected;
  String ipAddress;
  bool sensorsInitialized;
  bool audioAvailable;
  bool timeValid;
  uint8_t clientCount;
  SensorSignature temperature;
  SensorSignature humidity;
  SensorSignature mq2;
  SensorSignature mq135;
  SensorSignature flameAdc;
  bool flameDetected;
  bool flameValid;
  bool pirMotion;
  bool pirValid;
  uint32_t pirLastUpdated;
  bool flameHazardActive;
  bool mq2HazardActive;
  bool mq135HazardActive;
  bool hazardActive;
  String detectedClass;
  float confidence;
  bool inferenceValid;
  uint32_t lastInference;
  bool securityArmed;
  bool scheduleEnabled;
  uint8_t armHour;
  uint8_t armMinute;
  uint8_t disarmHour;
  uint8_t disarmMinute;
  bool securityAlarmActive;
};

SensorSignature makeSensorSignature(const SensorValue &value) {
  return {value.value, value.valid, value.lastUpdated};
}

bool operator==(const SensorSignature &left, const SensorSignature &right) {
  return left.value == right.value && left.valid == right.valid &&
         left.lastUpdated == right.lastUpdated;
}

bool operator==(const StateSignature &left, const StateSignature &right) {
  return left.wifiConnected == right.wifiConnected &&
         left.ipAddress == right.ipAddress &&
         left.sensorsInitialized == right.sensorsInitialized &&
         left.audioAvailable == right.audioAvailable &&
         left.timeValid == right.timeValid &&
         left.clientCount == right.clientCount &&
         left.temperature == right.temperature &&
         left.humidity == right.humidity && left.mq2 == right.mq2 &&
         left.mq135 == right.mq135 && left.flameAdc == right.flameAdc &&
         left.flameDetected == right.flameDetected &&
         left.flameValid == right.flameValid &&
         left.pirMotion == right.pirMotion && left.pirValid == right.pirValid &&
         left.pirLastUpdated == right.pirLastUpdated &&
         left.flameHazardActive == right.flameHazardActive &&
         left.mq2HazardActive == right.mq2HazardActive &&
         left.mq135HazardActive == right.mq135HazardActive &&
         left.hazardActive == right.hazardActive &&
         left.detectedClass == right.detectedClass &&
         left.confidence == right.confidence &&
         left.inferenceValid == right.inferenceValid &&
         left.lastInference == right.lastInference &&
         left.securityArmed == right.securityArmed &&
         left.scheduleEnabled == right.scheduleEnabled &&
         left.armHour == right.armHour && left.armMinute == right.armMinute &&
         left.disarmHour == right.disarmHour &&
         left.disarmMinute == right.disarmMinute &&
         left.securityAlarmActive == right.securityAlarmActive;
}

StateSignature captureStateSignature() {
  const HomeSafeState &state = getState();
  const EnvironmentData &environment = state.environment;
  const SecurityState &security = state.security;

  return {WiFi.status() == WL_CONNECTED,
          WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String(),
          Sensors::isInitialized(),
          Audio::isInitialized(),
          state.device.timeValid,
          connectedClientCount(),
          makeSensorSignature(environment.temperatureC),
          makeSensorSignature(environment.humidityRH),
          makeSensorSignature(environment.mq2ADC),
          makeSensorSignature(environment.mq135ADC),
          makeSensorSignature(environment.flameADC),
          environment.flameDetected,
          environment.flameValid,
          environment.pirMotion,
          environment.pirValid,
          environment.pirLastUpdated,
          environment.flameHazardActive,
          environment.mq2HazardActive,
          environment.mq135HazardActive,
          environment.hazardActive,
          state.audio.detectedClass,
          state.audio.confidence,
          state.audio.inferenceValid,
          state.audio.lastInference,
          security.armed,
          security.scheduleEnabled,
          security.armHour,
          security.armMinute,
          security.disarmHour,
          security.disarmMinute,
          security.alarmActive};
}

void setError(JsonDocument &response, const char *requestId,
              const char *command, const char *code, const char *message) {
  response["request_id"] = requestId;
  response["command"] = command;
  response["success"] = false;

  JsonObject error = response["error"].to<JsonObject>();
  error["code"] = code;
  error["message"] = message;
}

void initializeCommandResponse(JsonDocument &response, const char *requestId,
                               const char *command) {
  response["version"] = Protocol::VERSION;
  response["type"] = Protocol::MessageType::COMMAND_RESULT;
  response["request_id"] = requestId;
  response["command"] = command;
  response["timestamp_ms"] = millis();
  response["sequence"] = ++outboundSequence;
}

// Send a JSON response to a specific client.
bool sendToClient(uint8_t clientId, const JsonDocument &document) {
  if (document.overflowed()) {
    return false;
  }

  String response;
  response.reserve(2048);
  const size_t bytes = serializeJson(document, response);

  if (bytes == 0 || response.isEmpty()) {
    return false;
  }

  webSocket.sendTXT(clientId, response);
  return true;
}

bool serializeAndSendToAll(const JsonDocument &document) {
  if (document.overflowed()) {
    return false;
  }

  String response;
  response.reserve(2048);
  const size_t bytes = serializeJson(document, response);

  if (bytes == 0 || response.isEmpty()) {
    return false;
  }

  webSocket.broadcastTXT(response);
  return true;
}

void publishPendingAcousticEvent() {
  if (!acousticEventPending) {
    return;
  }

  JsonDocument message;
  message["version"] = Protocol::VERSION;
  message["type"] = Protocol::MessageType::ACOUSTIC_EVENT;
  message["event_id"] = ++nextEventId;
  message["sequence"] = ++outboundSequence;
  message["timestamp_ms"] = millis();

  JsonObject data = message["data"].to<JsonObject>();
  data["detected_class"] = pendingAcousticClass;
  data["confidence"] = pendingAcousticConfidence;

  const bool sent =
      connectedClientCount() == 0 || serializeAndSendToAll(message);
  if (!sent) {
    return;
  }

  acousticEventPending = false;
  pendingAcousticClass = "";
  pendingAcousticConfidence = 0.0f;
}

bool publishPendingEvent() {
  if (pendingEventCount == 0) {
    return true;
  }

  PendingEvent &pending = pendingEvents[pendingEventHead];
  JsonDocument message;
  message["version"] = Protocol::VERSION;
  message["sequence"] = ++outboundSequence;
  message["event_id"] = ++nextEventId;
  message["timestamp_ms"] = millis();

  JsonObject data = message["data"].to<JsonObject>();
  if (pending.kind == PendingEventKind::Hazard) {
    message["type"] = Protocol::MessageType::HAZARD_EVENT;
    data["source"] = pending.source;
    data["active"] = pending.active;
    data["environmental_active"] = pending.environmentalActive;
  } else if (pending.kind == PendingEventKind::Security) {
    message["type"] = Protocol::MessageType::SECURITY_EVENT;
    data["name"] = pending.name;
    data["armed"] = pending.armed;
    data["schedule_enabled"] = pending.scheduleEnabled;
    data["alarm_active"] = pending.alarmActive;
    const SecurityState &security = getState().security;
    data["arm_hour"] = security.armHour;
    data["arm_minute"] = security.armMinute;
    data["disarm_hour"] = security.disarmHour;
    data["disarm_minute"] = security.disarmMinute;
  } else {
    message["type"] = Protocol::MessageType::ALARM_EVENT;
    data["source"] = pending.source;
    data["action"] = pending.action;
    data["environmental_active"] = pending.environmentalActive;
    data["security_active"] = pending.securityActive;
    data["any_active"] = pending.environmentalActive || pending.securityActive;
  }

  const bool sent =
      connectedClientCount() == 0 || serializeAndSendToAll(message);
  if (!sent) {
    return false;
  }

  pending.source = "";
  pending.name = "";
  pending.action = "";
  pendingEventHead = (pendingEventHead + 1) % PENDING_EVENT_CAPACITY;
  --pendingEventCount;
  return true;
}

bool sendStateSnapshot(uint8_t clientId) {
  JsonDocument message;
  message["version"] = Protocol::VERSION;
  message["type"] = Protocol::MessageType::STATE_SNAPSHOT;
  message["timestamp_ms"] = millis();
  message["sequence"] = ++outboundSequence;
  serializeState(message["data"].to<JsonObject>());

  return sendToClient(clientId, message);
}

bool sendStateUpdate(const StateSignature &previous,
                     const StateSignature &current) {
  JsonDocument message;
  message["version"] = Protocol::VERSION;
  message["type"] = Protocol::MessageType::STATE_UPDATE;
  message["timestamp_ms"] = millis();
  message["sequence"] = ++outboundSequence;
  JsonObject data = message["data"].to<JsonObject>();

  if (previous.wifiConnected != current.wifiConnected ||
      previous.ipAddress != current.ipAddress ||
      previous.sensorsInitialized != current.sensorsInitialized ||
      previous.audioAvailable != current.audioAvailable ||
      previous.timeValid != current.timeValid ||
      previous.clientCount != current.clientCount) {
    serializeDevice(data["device"].to<JsonObject>());
  }

  if (!(previous.temperature == current.temperature) ||
      !(previous.humidity == current.humidity) ||
      !(previous.mq2 == current.mq2) || !(previous.mq135 == current.mq135) ||
      !(previous.flameAdc == current.flameAdc) ||
      previous.flameDetected != current.flameDetected ||
      previous.flameValid != current.flameValid ||
      previous.pirMotion != current.pirMotion ||
      previous.pirValid != current.pirValid ||
      previous.pirLastUpdated != current.pirLastUpdated) {
    serializeEnvironment(data["environment"].to<JsonObject>());
  }

  if (previous.flameHazardActive != current.flameHazardActive ||
      previous.mq2HazardActive != current.mq2HazardActive ||
      previous.mq135HazardActive != current.mq135HazardActive ||
      previous.hazardActive != current.hazardActive) {
    serializeHazards(data["hazards"].to<JsonObject>());
  }

  if (previous.detectedClass != current.detectedClass ||
      previous.confidence != current.confidence ||
      previous.inferenceValid != current.inferenceValid ||
      previous.lastInference != current.lastInference) {
    serializeAudio(data["audio"].to<JsonObject>());
  }

  if (previous.securityArmed != current.securityArmed ||
      previous.scheduleEnabled != current.scheduleEnabled ||
      previous.armHour != current.armHour ||
      previous.armMinute != current.armMinute ||
      previous.disarmHour != current.disarmHour ||
      previous.disarmMinute != current.disarmMinute ||
      previous.securityAlarmActive != current.securityAlarmActive) {
    serializeSecurity(data["security"].to<JsonObject>());
  }

  if (previous.hazardActive != current.hazardActive ||
      previous.securityAlarmActive != current.securityAlarmActive) {
    serializeAlarms(data["alarms"].to<JsonObject>());
  }

  return serializeAndSendToAll(message);
}

void updateStateBroadcast() {
  const StateSignature current = captureStateSignature();
  static StateSignature baseline;

  if (!stateBaselineInitialized) {
    stateBaselineInitialized = true;
    baseline = current;
    return;
  }

  if (baseline == current) {
    return;
  }

  if (connectedClientCount() == 0 || sendStateUpdate(baseline, current)) {
    baseline = current;
  }
}

// Handle incoming WebSocket messages.
void handleMessage(uint8_t clientId, uint8_t *payload, size_t length) {
  JsonDocument request;

  DeserializationError error = deserializeJson(request, payload, length);

  if (error) {
    JsonDocument response;
    initializeCommandResponse(response, "", "");
    setError(response, "", "", Protocol::ErrorCode::INVALID_JSON,
             "Request could not be parsed as JSON");

    sendToClient(clientId, response);
    return;
  }

  JsonDocument response;
  if (!request.is<JsonObject>()) {
    initializeCommandResponse(response, "", "");
    setError(response, "", "", Protocol::ErrorCode::INVALID_JSON,
             "Request must be a JSON object");
    sendToClient(clientId, response);
    return;
  }

  if (!request["version"].is<uint8_t>()) {
    initializeCommandResponse(response, "", "");
    setError(response, "", "", Protocol::ErrorCode::UNSUPPORTED_VERSION,
             "version must be an integer protocol version");

    sendToClient(clientId, response);
    return;
  }

  const uint8_t receivedVersion = request["version"].as<uint8_t>();
  if (receivedVersion != Protocol::VERSION) {
    initializeCommandResponse(response, "", "");
    setError(response, "", "", Protocol::ErrorCode::UNSUPPORTED_VERSION,
             "Unsupported protocol version");

    sendToClient(clientId, response);
    return;
  }

  if (!request["type"].is<const char *>() ||
      strcmp(request["type"].as<const char *>(),
             Protocol::MessageType::COMMAND) != 0) {
    initializeCommandResponse(response, "", "");
    setError(response, "", "", Protocol::ErrorCode::INVALID_MESSAGE_TYPE,
             "Message type must be command");

    sendToClient(clientId, response);
    return;
  }

  if (!request["request_id"].is<const char *>() ||
      request["request_id"].as<const char *>()[0] == '\0') {
    initializeCommandResponse(response, "", "");
    setError(response, "", "", Protocol::ErrorCode::MISSING_REQUEST_ID,
             "request_id must be a non-empty string");

    sendToClient(clientId, response);
    return;
  }

  const char *requestId = request["request_id"].as<const char *>();
  if (strlen(requestId) > 64) {
    initializeCommandResponse(response, "", "");
    setError(response, "", "", Protocol::ErrorCode::INVALID_REQUEST_ID,
             "request_id must not exceed 64 characters");

    sendToClient(clientId, response);
    return;
  }

  if (!request["command"].is<const char *>()) {
    initializeCommandResponse(response, requestId, "");
    setError(response, requestId, "",
             Protocol::ErrorCode::MISSING_OR_INVALID_COMMAND,
             "command must be a string");

    sendToClient(clientId, response);
    return;
  }

  const char *command = request["command"].as<const char *>();
  initializeCommandResponse(response, requestId, command);

  if (command[0] == '\0') {
    setError(response, requestId, command,
             Protocol::ErrorCode::MISSING_OR_INVALID_COMMAND,
             "command must be a non-empty string");
    sendToClient(clientId, response);
    return;
  }

  if (strcmp(command, Protocol::Command::GET_STATUS) == 0) {
    JsonObject data = response["data"].to<JsonObject>();
    serializeState(data["state"].to<JsonObject>());
    response["timestamp_ms"] = millis();

    response["success"] = true;

  } else if (strcmp(command, Protocol::Command::SET_SECURITY_MODE) == 0) {
    JsonVariantConst data = request["data"];

    if (!data["armed"].is<bool>()) {
      setError(response, requestId, command,
               Protocol::ErrorCode::MISSING_OR_INVALID_ARMED,
               "data.armed must be a boolean");

    } else {
      const bool armed = data["armed"].as<bool>();

      if (!Security::isInitialized()) {
        setError(response, requestId, command,
                 Protocol::ErrorCode::SECURITY_NOT_INITIALIZED,
                 "Security module is not initialized");
        sendToClient(clientId, response);
        return;
      }

      // Manual mode takes priority over the schedule. The schedule can only
      // be enabled again through an explicit app command.
      if (Security::isScheduleEnabled()) {
        Security::setScheduleEnabled(false);
      }

      Security::setArmed(armed);
      response["success"] = (Security::isArmed() == armed);

      if (!response["success"].as<bool>()) {
        setError(response, requestId, command,
                 Protocol::ErrorCode::COMMAND_NOT_APPLIED,
                 "Security mode could not be applied");
      } else {
        serializeSecurity(response["data"]["security"].to<JsonObject>());
      }
    }

  } else if (strcmp(command, Protocol::Command::SET_SECURITY_SCHEDULE) == 0) {
    JsonVariantConst data = request["data"];

    if (!Security::isInitialized()) {
      setError(response, requestId, command,
               Protocol::ErrorCode::SECURITY_NOT_INITIALIZED,
               "Security module is not initialized");
      sendToClient(clientId, response);
      return;
    }

    if (!data["enabled"].is<bool>()) {
      setError(response, requestId, command,
               Protocol::ErrorCode::MISSING_OR_INVALID_ENABLED,
               "data.enabled must be a boolean");

    } else {
      const bool enabled = data["enabled"].as<bool>();
      bool success = true;

      if (enabled && Security::isArmed()) {
        success = false;
        setError(response, requestId, command,
                 Protocol::ErrorCode::MANUAL_SECURITY_ACTIVE,
                 "Disarm manual security before enabling the schedule");
      }

      if (enabled && success) {
        const bool hasTimes =
            data["arm_hour"].is<int>() && data["arm_minute"].is<int>() &&
            data["disarm_hour"].is<int>() && data["disarm_minute"].is<int>();

        if (!hasTimes) {
          success = false;
          setError(response, requestId, command,
                   Protocol::ErrorCode::MISSING_OR_INVALID_SCHEDULE_TIMES,
                   "Schedule times are required when enabling the schedule");

        } else {
          const int armHour = data["arm_hour"].as<int>();
          const int armMinute = data["arm_minute"].as<int>();
          const int disarmHour = data["disarm_hour"].as<int>();
          const int disarmMinute = data["disarm_minute"].as<int>();

          if (armHour < 0 || armHour > 23 || disarmHour < 0 ||
              disarmHour > 23 || armMinute < 0 || armMinute > 59 ||
              disarmMinute < 0 || disarmMinute > 59) {
            success = false;
            setError(response, requestId, command,
                     Protocol::ErrorCode::INVALID_SCHEDULE,
                     "Schedule hours or minutes are out of range");
          } else {
            success = Security::setSchedule(static_cast<uint8_t>(armHour),
                                            static_cast<uint8_t>(armMinute),
                                            static_cast<uint8_t>(disarmHour),
                                            static_cast<uint8_t>(disarmMinute));
          }

          if (!success) {
            setError(response, requestId, command,
                     Protocol::ErrorCode::INVALID_SCHEDULE,
                     "Schedule times are invalid");
          }
        }
      }

      if (success) {
        Security::setScheduleEnabled(enabled);
        response["success"] = Security::isScheduleEnabled() == enabled;
        if (response["success"].as<bool>()) {
          serializeSecurity(response["data"]["security"].to<JsonObject>());
        } else {
          setError(response, requestId, command,
                   Protocol::ErrorCode::COMMAND_NOT_APPLIED,
                   "Security schedule state could not be applied");
        }
      } else {
        response["success"] = false;
      }
    }

  } else if (strcmp(command, Protocol::Command::SILENCE_ALARM) == 0) {
    if (!Security::isInitialized()) {
      setError(response, requestId, command,
               Protocol::ErrorCode::SECURITY_NOT_INITIALIZED,
               "Security module is not initialized");
      sendToClient(clientId, response);
      return;
    }

    const bool securityAlarmWasActive = getState().security.alarmActive;
    Security::silenceAlarm();

    response["success"] = !getState().security.alarmActive;
    JsonObject data = response["data"].to<JsonObject>();
    data["security_alarm_was_active"] = securityAlarmWasActive;
    data["security_alarm_active"] = getState().security.alarmActive;
    data["environmental_alarm_active"] = getState().environment.hazardActive;
    data["any_active"] =
        getState().environment.hazardActive || getState().security.alarmActive;

  } else {
    setError(response, requestId, command, Protocol::ErrorCode::UNKNOWN_COMMAND,
             "Command is not supported");
  }

  sendToClient(clientId, response);
}

void onWebSocketEvent(uint8_t clientId, WStype_t type, uint8_t *payload,
                      size_t length) {
  switch (type) {
  case WStype_CONNECTED:
    sendStateSnapshot(clientId);
    stateBaselineInitialized = false;

    Serial.printf("[Network] WebSocket client %u connected\n", clientId);
    break;

  case WStype_DISCONNECTED:
    stateBaselineInitialized = false;

    Serial.printf("[Network] WebSocket client %u disconnected\n", clientId);
    break;

  case WStype_TEXT:
    Serial.printf("[Network] Received message (%u bytes)\n",
                  static_cast<unsigned>(length));

    handleMessage(clientId, payload, length);
    break;

  default:
    break;
  }
}

void connectWiFi() {
  Serial.printf("[Network] Connecting to Wi-Fi: %s\n", Secrets::WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  WiFi.begin(Secrets::WIFI_SSID, Secrets::WIFI_PASSWORD);

  lastReconnectAttempt = millis();
}

} // anonymous namespace

void begin() {
  Serial.println("[Network] Initializing...");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  connectWiFi();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  Serial.printf("[Network] WebSocket server initialized on port %u\n",
                Protocol::DEFAULT_PORT);
}

void update() {
  webSocket.loop();
  publishPendingAcousticEvent();
  publishPendingEvent();
  updateStateBroadcast();

  const bool currentlyConnected = WiFi.status() == WL_CONNECTED;

  static uint32_t lastDiagnostic = 0;

  if (millis() - lastDiagnostic >= 5000) {
    lastDiagnostic = millis();

    Serial.printf("[WiFi Debug] Status: %d | IP: %s | Mode: %d\n",
                  static_cast<int>(WiFi.status()),
                  WiFi.localIP().toString().c_str(),
                  static_cast<int>(WiFi.getMode()));
  }

  if (currentlyConnected != wifiConnected) {
    wifiConnected = currentlyConnected;

    if (wifiConnected) {
      Serial.printf("[Network] Wi-Fi connected. IP: %s\n",
                    WiFi.localIP().toString().c_str());
    } else {
      Serial.println("[Network] Wi-Fi disconnected.");
    }
  }

  static bool ipPrinted = false;

  if (currentlyConnected && !ipPrinted) {
    Serial.printf("[Network] ESP32 IP address: %s\n",
                  WiFi.localIP().toString().c_str());

    ipPrinted = true;
  }

  if (!currentlyConnected && millis() - lastReconnectAttempt >=
                                 Config::Network::WIFI_RECONNECT_INTERVAL_MS) {

    Serial.println("[Network] Retrying Wi-Fi connection...");

    WiFi.begin(Secrets::WIFI_SSID, Secrets::WIFI_PASSWORD);

    lastReconnectAttempt = millis();
  }
}

bool isWiFiConnected() { return WiFi.status() == WL_CONNECTED; }

bool isClientConnected() { return connectedClientCount() > 0; }

void broadcast(const String &jsonMessage) {
  if (isWiFiConnected()) {
    webSocket.broadcastTXT(jsonMessage.c_str());
  }
}

void send(const String &jsonMessage) {
  if (isWiFiConnected()) {
    webSocket.broadcastTXT(jsonMessage.c_str());
  }
}

bool queueAcousticEvent(const String &detectedClass, float confidence) {
  if (acousticEventPending) {
    return false;
  }

  pendingAcousticClass = detectedClass;
  pendingAcousticConfidence = confidence;
  acousticEventPending = true;
  return true;
}

bool queueHazardEvent(const String &source, bool active,
                      bool environmentalActive) {
  if (pendingEventCount >= PENDING_EVENT_CAPACITY) {
    return false;
  }

  const size_t index =
      (pendingEventHead + pendingEventCount) % PENDING_EVENT_CAPACITY;
  PendingEvent &pending = pendingEvents[index];
  pending.kind = PendingEventKind::Hazard;
  pending.source = source;
  pending.active = active;
  pending.environmentalActive = environmentalActive;
  ++pendingEventCount;
  return true;
}

bool queueSecurityEvent(const String &name, bool armed, bool scheduleEnabled,
                        bool alarmActive) {
  if (pendingEventCount >= PENDING_EVENT_CAPACITY) {
    return false;
  }

  const size_t index =
      (pendingEventHead + pendingEventCount) % PENDING_EVENT_CAPACITY;
  PendingEvent &pending = pendingEvents[index];
  pending.kind = PendingEventKind::Security;
  pending.name = name;
  pending.armed = armed;
  pending.scheduleEnabled = scheduleEnabled;
  pending.alarmActive = alarmActive;
  ++pendingEventCount;
  return true;
}

bool queueAlarmEvent(const String &source, const String &action,
                     bool environmentalActive, bool securityActive) {
  if (pendingEventCount >= PENDING_EVENT_CAPACITY) {
    return false;
  }

  const size_t index =
      (pendingEventHead + pendingEventCount) % PENDING_EVENT_CAPACITY;
  PendingEvent &pending = pendingEvents[index];
  pending.kind = PendingEventKind::Alarm;
  pending.source = source;
  pending.action = action;
  pending.environmentalActive = environmentalActive;
  pending.securityActive = securityActive;
  ++pendingEventCount;
  return true;
}

} // namespace Network
} // namespace HomeSafe
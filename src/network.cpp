#include "network.h"
#include "app_state.h"
#include "config.h"
#include "protocol.h"
#include "secret.h"
#include "security.h"

#include <ArduinoJson.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

namespace HomeSafe {
namespace Network {

namespace {

WebSocketsServer webSocket(Protocol::DEFAULT_PORT);

bool wifiConnected = false;
bool clientConnected = false;

uint32_t lastReconnectAttempt = 0;

// Send a JSON response to a specific client.
void sendToClient(uint8_t clientId, const JsonDocument &document) {
  String response;
  serializeJson(document, response);

  webSocket.sendTXT(clientId, response);
}

// Handle incoming WebSocket messages.
void handleMessage(uint8_t clientId, uint8_t *payload, size_t length) {
  JsonDocument request;

  DeserializationError error = deserializeJson(request, payload, length);

  if (error) {
    JsonDocument response;
    response["version"] = Protocol::VERSION;
    response["type"] = Protocol::MessageType::COMMAND_RESULT;
    response["success"] = false;
    response["error"] = "invalid_json";

    sendToClient(clientId, response);
    return;
  }

  const char *type = request["type"] | "";
  const char *command = request["command"] | "";
  const char *requestId = request["request_id"] | "";

  JsonDocument response;

  response["version"] = Protocol::VERSION;
  response["type"] = Protocol::MessageType::COMMAND_RESULT;
  response["request_id"] = requestId;

  const uint8_t receivedVersion = request["version"] | 0;

  if (receivedVersion != Protocol::VERSION) {
    response["success"] = false;
    response["error"] = "unsupported_version";

    sendToClient(clientId, response);
    return;
  }

  if (strcmp(type, Protocol::MessageType::COMMAND) != 0) {
    response["success"] = false;
    response["error"] = "invalid_message_type";

    sendToClient(clientId, response);
    return;
  }

  if (strcmp(command, Protocol::Command::GET_STATUS) == 0) {
    const HomeSafeState &state = getState();

    JsonObject data = response["data"].to<JsonObject>();

    data["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
    data["websocket_client_connected"] = true;
    data["ip_address"] = WiFi.localIP().toString();
    data["uptime_ms"] = millis();
    data["security_armed"] = state.security.armed;
    data["schedule_enabled"] = state.security.scheduleEnabled;
    data["alarm_active"] = state.security.alarmActive;

    response["success"] = true;

  } else if (strcmp(command, "set_security_mode") == 0) {
    JsonVariantConst data = request["data"];

    if (!data["armed"].is<bool>()) {
      response["success"] = false;
      response["error"] = "missing_or_invalid_armed";

    } else {
      const bool armed = data["armed"].as<bool>();

      Security::setArmed(armed);

      // Confirm the requested state was actually applied.
      response["success"] = (Security::isArmed() == armed);

      if (!response["success"].as<bool>()) {
        response["error"] = "security_state_change_failed";
      }
    }

  } else if (strcmp(command, "set_security_schedule") == 0) {
    JsonVariantConst data = request["data"];

    if (!data["enabled"].is<bool>()) {
      response["success"] = false;
      response["error"] = "missing_or_invalid_enabled";

    } else {
      const bool enabled = data["enabled"].as<bool>();
      bool success = true;

      if (enabled) {
        const bool hasTimes = data["arm_hour"].is<uint8_t>() &&
                              data["arm_minute"].is<uint8_t>() &&
                              data["disarm_hour"].is<uint8_t>() &&
                              data["disarm_minute"].is<uint8_t>();

        if (!hasTimes) {
          success = false;
          response["error"] = "missing_or_invalid_schedule_times";

        } else {
          success = Security::setSchedule(data["arm_hour"].as<uint8_t>(),
                                          data["arm_minute"].as<uint8_t>(),
                                          data["disarm_hour"].as<uint8_t>(),
                                          data["disarm_minute"].as<uint8_t>());

          if (!success) {
            response["error"] = "invalid_schedule";
          }
        }
      }

      if (success) {
        Security::setScheduleEnabled(enabled);
        response["success"] = true;
      } else {
        response["success"] = false;
      }
    }

  } else if (strcmp(command, "silence_alarm") == 0) {
    Security::silenceAlarm();

    response["success"] = true;

  } else {
    response["success"] = false;
    response["error"] = "unknown_command";
  }

  sendToClient(clientId, response);
}

void onWebSocketEvent(uint8_t clientId, WStype_t type, uint8_t *payload,
                      size_t length) {
  switch (type) {
  case WStype_CONNECTED:
    clientConnected = true;

    Serial.printf("[Network] WebSocket client %u connected\n", clientId);
    break;

  case WStype_DISCONNECTED:
    clientConnected = false;

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

bool isClientConnected() { return clientConnected; }

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

} // namespace Network
} // namespace HomeSafe
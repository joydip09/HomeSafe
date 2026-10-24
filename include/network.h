#ifndef HOMESAFE_NETWORK_H
#define HOMESAFE_NETWORK_H

#include <Arduino.h>

namespace HomeSafe {
namespace Network {

// Initialize Wi-Fi and the WebSocket server.
void begin();

// Maintain Wi-Fi and WebSocket connections.
// Call regularly from loop().
void update();

// Check whether Wi-Fi is connected.
bool isWiFiConnected();

// Check whether a WebSocket client is connected.
bool isClientConnected();

// Broadcast a JSON message to connected clients.
void broadcast(const String &jsonMessage);

// Send a JSON message to the connected client.
void send(const String &jsonMessage);

// Queue an accepted acoustic event for transmission during Network::update().
// The audio loop remains non-blocking and does not serialize JSON directly.
bool queueAcousticEvent(const String &detectedClass, float confidence);
bool queueHazardEvent(const String &source, bool active,
                      bool environmentalActive);
bool queueSecurityEvent(const String &name, bool armed, bool scheduleEnabled,
                        bool alarmActive);
bool queueAlarmEvent(const String &source, const String &action,
                     bool environmentalActive, bool securityActive);

} // namespace Network
} // namespace HomeSafe

#endif // HOMESAFE_NETWORK_H
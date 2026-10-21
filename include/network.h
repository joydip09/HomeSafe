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

} // namespace Network
} // namespace HomeSafe

#endif // HOMESAFE_NETWORK_H
#include <Arduino.h>

#include "network.h"
#include "sensors.h"

void setup() {
  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("       HomeSafe Firmware");
  Serial.println("       Network Test v1");
  Serial.println("================================");

  HomeSafe::Network::begin();
  HomeSafe::Sensors::begin();
}

void loop() {
  HomeSafe::Network::update();
  HomeSafe::Sensors::update();

  // Keep the loop responsive for future sensor tasks.
  delay(1);
}
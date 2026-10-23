#include <Arduino.h>

#include "audio.h"
#include "network.h"
#include "outputs.h"
#include "safety.h"
#include "security.h"
#include "sensors.h"

void setup() {
  Serial.begin(115200);

  delay(1000);

  // Serial.println();
  // Serial.println("================================");
  // Serial.println("       HomeSafe Firmware");
  // Serial.println("       Network Test v1");
  // Serial.println("================================");

  HomeSafe::Network::begin();
  HomeSafe::Sensors::begin();
  HomeSafe::Outputs::begin();
  HomeSafe::Safety::begin();
  HomeSafe::Security::begin();
  HomeSafe::Audio::begin();
}

void loop() {
  HomeSafe::Network::update();
  HomeSafe::Sensors::update();
  HomeSafe::Safety::update();
  HomeSafe::Security::update();
  HomeSafe::Audio::update();

  // Keep the loop responsive for future sensor tasks.
  delay(1);
}
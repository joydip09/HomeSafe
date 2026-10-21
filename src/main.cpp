#include <Arduino.h>

#include "network.h"
#include "outputs.h"
#include "sensors.h"

void testOutputs() {
  using namespace HomeSafe::Outputs;

  static uint8_t step = 0;
  static uint32_t lastStep = 0;

  const uint32_t now = millis();

  if (now - lastStep < 2000) {
    return;
  }

  lastStep = now;

  switch (step) {
  case 0:
    Serial.println("[Output Test] All channels OFF");
    setRGB(0, 0, 0);
    buzzerOff();
    break;

  case 1:
    Serial.println("[Output Test] RED");
    setRGB(255, 0, 0);
    break;

  case 2:
    Serial.println("[Output Test] GREEN");
    setRGB(0, 255, 0);
    break;

  case 3:
    Serial.println("[Output Test] BLUE");
    setRGB(0, 0, 255);
    break;

  case 4:
    Serial.println("[Output Test] YELLOW");
    setRGB(255, 255, 0);
    break;

  case 5:
    Serial.println("[Output Test] WHITE");
    setRGB(255, 255, 255);
    break;

  case 6:
    Serial.println("[Output Test] LED OFF");
    turnLEDOff();
    break;

  case 7:
    Serial.println("[Output Test] BUZZER ON");
    buzzerOn();
    break;

  case 8:
    Serial.println("[Output Test] BUZZER OFF");
    buzzerOff();
    break;

  case 9:
    Serial.println("[Output Test] Test complete");
    turnLEDOff();
    buzzerOff();
    break;
  }

  if (step < 9) {
    step++;
  }
}

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
  HomeSafe::Outputs::begin();
}

void loop() {
  HomeSafe::Network::update();
  HomeSafe::Sensors::update();

  testOutputs();
  // Keep the loop responsive for future sensor tasks.
  delay(1);
}
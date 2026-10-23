#ifndef HOMESAFE_OUTPUTS_H
#define HOMESAFE_OUTPUTS_H

#include <Arduino.h>

namespace HomeSafe {
namespace Outputs {

// Initialize the RGB LED and buzzer.
void begin();

// Set RGB LED brightness.
// Each channel ranges from 0 (off) to 255 (maximum).
void setRGB(uint8_t red, uint8_t green, uint8_t blue);

// Turn the RGB LED off.
void turnLEDOff();

// Turn the active buzzer on.
void buzzerOn();

// Turn the active buzzer off.
void buzzerOff();

// Check whether the buzzer is currently active.
bool isBuzzerActive();

// Report whether an environmental alarm should be active.
void setEnvironmentalAlarm(bool active);

// Report whether a security alarm should be active.
void setSecurityAlarm(bool active);

} // namespace Outputs
} // namespace HomeSafe

#endif // HOMESAFE_OUTPUTS_H

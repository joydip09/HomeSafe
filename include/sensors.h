#ifndef HOMESAFE_SENSORS_H
#define HOMESAFE_SENSORS_H

#include <Arduino.h>

namespace HomeSafe {
namespace Sensors {

// Initialize all configured sensors.
void begin();

// Update sensor readings when their sampling intervals expire.
// Call regularly from loop().
void update();

// Returns true when sensor initialization has completed.
bool isInitialized();

} // namespace Sensors
} // namespace HomeSafe

#endif // HOMESAFE_SENSORS_H
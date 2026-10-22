#ifndef HOMESAFE_SAFETY_H
#define HOMESAFE_SAFETY_H

#include <Arduino.h>

namespace HomeSafe {
namespace Safety {

// Initialize the safety monitoring module.
void begin();

// Evaluate sensor readings and update hazard states.
// Must be called regularly from loop().
void update();

// Indicates whether a confirmed environmental hazard is active.
bool isHazardActive();

} // namespace Safety
} // namespace HomeSafe

#endif // HOMESAFE_SAFETY_H
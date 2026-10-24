#ifndef HOMESAFE_SECURITY_H
#define HOMESAFE_SECURITY_H

#include <Arduino.h>

namespace HomeSafe {
namespace Security {

// Initialize the security module.
void begin();

// Evaluate security state and process PIR motion.
// Call regularly from loop().
void update();

// Arm or disarm the security system.
void setArmed(bool armed);

// Return whether security is currently armed.
bool isArmed();

// Return whether the security module is initialized.
bool isInitialized();

// Enable or disable scheduled arming.
void setScheduleEnabled(bool enabled);

// Return whether scheduled arming is enabled.
bool isScheduleEnabled();

// Silence the security alarm without disarming security.
void silenceAlarm();

// Configure the daily security schedule.
bool setSchedule(uint8_t armHour, uint8_t armMinute, uint8_t disarmHour,
                 uint8_t disarmMinute);

// Check whether the schedule configuration is valid.
bool isScheduleValid();

} // namespace Security
} // namespace HomeSafe

#endif // HOMESAFE_SECURITY_H
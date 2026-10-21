#ifndef HOMESAFE_PROTOCOL_H
#define HOMESAFE_PROTOCOL_H

#include <Arduino.h>

namespace HomeSafe {
namespace Protocol {

constexpr uint8_t VERSION = 1;
constexpr uint16_t DEFAULT_PORT = 81;

namespace MessageType {
constexpr const char *SENSOR_DATA = "sensor_data";
constexpr const char *DEVICE_STATUS = "device_status";
constexpr const char *HAZARD = "hazard";
constexpr const char *ACOUSTIC_EVENT = "acoustic_event";
constexpr const char *COMMAND = "command";
constexpr const char *COMMAND_RESULT = "command_result";
} // namespace MessageType

namespace Command {
constexpr const char *SET_SECURITY_MODE = "set_security_mode";
constexpr const char *SET_SECURITY_SCHEDULE = "set_security_schedule";
constexpr const char *SILENCE_ALARM = "silence_alarm";
constexpr const char *GET_STATUS = "get_status";
} // namespace Command

namespace Field {
constexpr const char *VERSION = "version";
constexpr const char *TYPE = "type";
constexpr const char *REQUEST_ID = "request_id";
constexpr const char *COMMAND = "command";
constexpr const char *TIMESTAMP = "timestamp";
constexpr const char *SUCCESS = "success";
constexpr const char *ERROR = "error";
} // namespace Field

namespace SecurityMode {
constexpr const char *ARMED = "armed";
constexpr const char *DISARMED = "disarmed";
} // namespace SecurityMode

} // namespace Protocol

} // namespace HomeSafe

#endif
#ifndef HOMESAFE_PROTOCOL_H
#define HOMESAFE_PROTOCOL_H

#include <Arduino.h>

namespace HomeSafe {
namespace Protocol {

constexpr uint8_t VERSION = 1;
constexpr uint16_t DEFAULT_PORT = 81;
constexpr const char *FIRMWARE_VERSION = "0.1.0";

using SequenceNumber = uint32_t;
using EventId = uint32_t;

namespace MessageType {
constexpr const char *STATE_SNAPSHOT = "state_snapshot";
constexpr const char *STATE_UPDATE = "state_update";
constexpr const char *ACOUSTIC_EVENT = "acoustic_event";
constexpr const char *HAZARD_EVENT = "hazard_event";
constexpr const char *SECURITY_EVENT = "security_event";
constexpr const char *ALARM_EVENT = "alarm_event";
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
constexpr const char *TIMESTAMP_MS = "timestamp_ms";
constexpr const char *SEQUENCE = "sequence";
constexpr const char *EVENT_ID = "event_id";
constexpr const char *REQUEST_ID = "request_id";
constexpr const char *COMMAND = "command";
constexpr const char *SUCCESS = "success";
constexpr const char *ERROR = "error";
} // namespace Field

namespace ErrorCode {
constexpr const char *INVALID_JSON = "invalid_json";
constexpr const char *UNSUPPORTED_VERSION = "unsupported_version";
constexpr const char *INVALID_MESSAGE_TYPE = "invalid_message_type";
constexpr const char *MISSING_REQUEST_ID = "missing_request_id";
constexpr const char *INVALID_REQUEST_ID = "invalid_request_id";
constexpr const char *MISSING_OR_INVALID_COMMAND = "missing_or_invalid_command";
constexpr const char *UNKNOWN_COMMAND = "unknown_command";
constexpr const char *MISSING_OR_INVALID_ARMED = "missing_or_invalid_armed";
constexpr const char *MISSING_OR_INVALID_ENABLED = "missing_or_invalid_enabled";
constexpr const char *MISSING_OR_INVALID_SCHEDULE_TIMES =
    "missing_or_invalid_schedule_times";
constexpr const char *INVALID_SCHEDULE = "invalid_schedule";
constexpr const char *SECURITY_NOT_INITIALIZED = "security_not_initialized";
constexpr const char *COMMAND_NOT_APPLIED = "command_not_applied";
constexpr const char *SERIALIZATION_FAILED = "serialization_failed";
} // namespace ErrorCode

enum class EventType : uint8_t {
  Acoustic,
  Hazard,
  Security,
  Alarm,
};

namespace SecurityMode {
constexpr const char *ARMED = "armed";
constexpr const char *DISARMED = "disarmed";
} // namespace SecurityMode

} // namespace Protocol

} // namespace HomeSafe

#endif
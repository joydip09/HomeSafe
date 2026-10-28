# Changelog

All notable changes to HomeSafe are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Planned

- Improve authenticated transport and secure credential provisioning.
- Expand automated hardware-in-the-loop coverage.
- Add production-grade Android notification and background-delivery support.

## [0.1.0] - 2026-10-29

### Added

- ESP32-S3 DevKitC-1 firmware using the Arduino framework and PlatformIO.
- DHT11 temperature and humidity monitoring.
- MQ-2 and MQ-135 raw ADC monitoring with configurable hazard thresholds.
- Calibrated MQ-2 and MQ-135 thresholds for the target hardware and enclosure.
- Flame sensor analog and digital detection.
- PIR motion monitoring for armed security.
- INMP441 I2S microphone capture at 16 kHz.
- Embedded Edge Impulse continuous audio classification.
- Environmental and security alarm handling with RGB LED and buzzer output.
- Persistent security mode and schedule settings using ESP32 Preferences.
- NTP-backed daily security schedules, including overnight schedules.
- Versioned local WebSocket protocol on port 81.
- State snapshots, incremental state updates, event IDs, and sequence numbers.
- Commands for status, security mode, schedules, and alarm silencing.
- Python WebSocket smoke and integration scripts.
- Comprehensive hardware, firmware, app-connection, protocol, and calibration
  documentation.

### Known limitations

- MQ thresholds are raw ADC values and require site-specific calibration.
- The ESP32 WebSocket endpoint does not currently provide application-level
  authentication or built-in TLS.
- Events are not replayed after an app disconnection.
- Scheduled security mode requires valid NTP time.
- The companion app does not currently deliver Android background notifications.

[Unreleased]: https://github.com/joydip09/HomeSafe/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/joydip09/HomeSafe/releases/tag/v0.1.0

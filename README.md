# HomeSafe

HomeSafe is a local-first smart-home safety terminal built around an
**ESP32-S3**. It combines environmental sensors, motion detection, an
INMP441 microphone with an embedded Edge Impulse classifier, a physical alarm,
and a native Android companion app.

The firmware makes the safety decisions locally. The Android app is a
dashboard and control surface that connects directly to the ESP32 over
WebSockets; there is no required cloud service, account, or remote database.

> **Safety notice:** HomeSafe is an electronics and software project, not a
> certified fire, gas, intrusion, or life-safety system. Sensor modules need
> calibration, the installation needs electrical and environmental testing, and
> the device must not be used as the sole protection for people or property.

## Features

- DHT11 temperature and relative-humidity monitoring
- MQ-2 and MQ-135 analog readings with configurable raw-ADC hazard thresholds
- Flame module with analog reading and active-low digital detection
- PIR motion monitoring for armed-security intrusion detection
- INMP441 I2S microphone at 16 kHz
- On-device continuous TinyML audio classification using the bundled
  `HomeSafe_v2_inferencing` Edge Impulse library
- Common-anode RGB status LED and active buzzer
- Environmental alarm confirmation and cooldown logic
- Armed/disarmed security mode with sustained-motion confirmation
- Persistent security mode and schedule settings using ESP32 Preferences
- Daily security schedule, including overnight schedules such as 22:00–06:00
- NTP time synchronization for schedule evaluation
- Protocol versioning, sequence numbers, event IDs, state snapshots, and
  incremental state updates
- Direct Android companion app connection over local Wi-Fi or a WebSocket-capable
  HTTPS tunnel

## System overview

```text
 DHT11 ─────┐
 MQ-2 ──────┤
 MQ-135 ────┤
 Flame ─────┤       ┌─────────────────────┐
 PIR ───────┼──────►│ ESP32-S3 firmware   │──── RGB LED
 INMP441 ───┘       │ sensors + safety    │──── Buzzer
                    │ + TinyML + WebSocket│
                    └──────────┬──────────┘
                               │ Wi-Fi / ws://
                               ▼
                    HomeSafe Android companion
```

The device owns sensor interpretation, hazard state, security decisions, and
the physical alarm. The app displays the synchronized state, records events
received while connected, and sends only firmware-supported commands.

## Hardware

### Main components

| Component | Purpose |
|---|---|
| ESP32-S3 DevKitC-1 | Main controller, Wi-Fi, ADC, I2S, and firmware runtime |
| DHT11 | Temperature and humidity |
| MQ-2 module | Smoke/flammable-gas indication |
| MQ-135 module | Air-quality/gas indication |
| Flame sensor module | Analog flame level and digital detection |
| PIR sensor | Motion detection while security is armed |
| INMP441 | I2S microphone for audio classification |
| Common-anode RGB LED | Alarm/status indication |
| Active buzzer | Audible alarm |
| Breadboard, regulated supply, resistors, and jumper wires | Assembly and power distribution |

### Pin map

The pin assignments are defined in [`include/config.h`](./include/config.h).

| Signal | ESP32-S3 GPIO | Direction / notes |
|---|---:|---|
| INMP441 SCK/BCLK | 4 | I2S clock |
| INMP441 WS/LRCL | 5 | I2S word-select |
| INMP441 SD/DOUT | 6 | I2S microphone data |
| DHT11 DATA | 45 | Digital data; use the module’s required pull-up |
| MQ-2 AO | 9 | Analog input |
| MQ-135 AO | 8 | Analog input |
| Flame AO | 1 | Analog input |
| Flame DO | 2 | Digital input; firmware treats LOW as detected |
| PIR OUT | 15 | Digital input; HIGH means motion |
| RGB red | 40 | PWM output; common-anode, active-low |
| RGB green | 39 | PWM output; common-anode, active-low |
| RGB blue | 38 | PWM output; common-anode, active-low |
| Buzzer | 21 | Digital output; HIGH activates the buzzer |

### Wiring and power notes

- Confirm the exact pin labels on each module before wiring. Module pinouts
  vary between manufacturers.
- ESP32-S3 GPIOs are **3.3 V** devices. Do not feed a 5 V analog or digital
  output directly into an ESP32-S3 input; use an appropriate level shifter or
  divider where necessary.
- MQ modules commonly use heaters and may draw substantially more current than
  the ESP32 board can provide through a small regulator. Use a suitable supply
  and connect grounds together.
- Use a current-limiting resistor for each RGB LED channel.
- The firmware assumes a common-anode RGB LED and an active buzzer. Change the
  output driver if the installed parts have different polarity or behavior.
- The flame digital-output polarity is module-dependent. Verify it with a
  controlled test before relying on the alarm.

## Firmware codebase

```text
include/
  app_state.h       Shared runtime state
  audio.h           Audio subsystem interface
  config.h          Pins, timing, thresholds, and network constants
  models.h          Sensor, security, alarm, and device data models
  network.h         Wi-Fi/WebSocket interface
  outputs.h         RGB LED and buzzer interface
  protocol.h        Protocol v1 names, commands, and error codes
  safety.h          Environmental safety interface
  security.h        Security and schedule interface
  sensors.h         Sensor acquisition interface
  secret.h          Local Wi-Fi credentials
src/
  main.cpp          Firmware initialization and cooperative main loop
  sensors.cpp       DHT11, MQ-2, MQ-135, flame, and PIR acquisition
  safety.cpp        Hazard confirmation, alarm activation, and cooldown
  security.cpp      Armed mode, PIR alarm, NTP schedule, and Preferences
  outputs.cpp       Centralized alarm output arbitration
  audio.cpp         I2S capture and continuous TinyML inference
  network.cpp       Wi-Fi, WebSocket protocol, serialization, and commands
lib/HomeSafe_v2_inferencing/
                    Generated Edge Impulse model and inference SDK
scripts/            WebSocket smoke/integration scripts
test/               PlatformIO test directory
```

The main loop is intentionally cooperative:

1. Maintain Wi-Fi and WebSocket connections.
2. Read sensors when their sampling intervals expire.
3. Confirm environmental hazards and update the environmental alarm.
4. Evaluate armed security, motion, and schedule state.
5. Capture microphone samples and run inference.

The physical outputs are arbitrated centrally. Environmental and security alarm
requests are independent, but either active request turns on the red LED and
buzzer.

## Firmware setup

### Prerequisites

- VS Code with the PlatformIO IDE extension, or PlatformIO Core
- A USB data cable
- ESP32-S3 DevKitC-1
- A 2.4 GHz Wi-Fi network accessible to both the ESP32 and phone

### Configure Wi-Fi safely

Edit [`include/secret.h`](./include/secret.h) locally and set the Wi-Fi
credentials for the network used by the device:

```cpp
namespace HomeSafe {
namespace Secrets {

constexpr char WIFI_SSID[] = "your-network-name";
constexpr char WIFI_PASSWORD[] = "your-network-password";

} // namespace Secrets
} // namespace HomeSafe
```

Do not publish real credentials or commit them to a public repository. For a
shared project, keep the local secret file out of version control and provide a
sanitized template through your team’s preferred secret-management workflow.

### Build and upload

From the firmware repository:

```bash
pio run
pio run --target upload
pio device monitor
```

The checked-in PlatformIO environment is `esp32-s3-devkitc-1`, uses the
Arduino framework, targets a 16 MB flash board with PSRAM, and uses a serial
monitor speed of `115200`.

If more than one serial device is present, set the ports in
[`platformio.ini`](./platformio.ini), or pass one explicitly:

```bash
pio run --target upload --upload-port /dev/ttyACM0
pio device monitor --port /dev/ttyACM0 --baud 115200
```

On boot, the serial log reports Wi-Fi, sensor, audio, and WebSocket
initialization. The device IP address is the value needed by the app.

## Calibration and safety behavior

### Sampling and decisions

| Subsystem | Firmware behavior |
|---|---|
| DHT11 | Read every 2 seconds |
| MQ-2 / MQ-135 | Read raw 12-bit ADC values every 250 ms |
| Flame | Read every 100 ms; digital LOW is interpreted as flame detected |
| PIR | Poll every 20 ms; HIGH is interpreted as motion |
| Environmental hazard | Condition must remain detected for 1 second |
| Environmental alarm cooldown | 5 seconds after all hazards clear |
| Audio inference | 16 kHz mono continuous inference |
| Audio event threshold | Non-`normal` class at confidence `>= 0.80` |
| Audio event cooldown | 3 seconds |

The default MQ-2 and MQ-135 thresholds are raw ADC values of `1000`. These are
placeholders, not universal gas-concentration limits. Record clean-air and
controlled-test readings for the actual modules, supply voltage, enclosure, and
environment, then adjust `MQ2_ADC_THRESHOLD` and `MQ135_ADC_THRESHOLD` in
[`include/config.h`](./include/config.h).

An environmental alarm is activated only after a continuous confirmed hazard.
When all environmental hazards clear, the environmental request is released,
but the alarm remains active during the configured cooldown before a new
environmental activation is accepted. A security alarm is tracked separately.

Security motion monitoring is active only while armed. Motion must remain
present for the same one-second confirmation period before the security alarm
is triggered. Silencing the security alarm does not disarm the system and does
not clear an environmental alarm.

Security mode, schedule-enabled state, and schedule times are stored in ESP32
non-volatile Preferences. The default schedule is 22:00–06:00. Schedule
evaluation requires a valid NTP-synchronized clock and uses UTC+6
(Bangladesh Standard Time) as configured by the firmware.

## Connecting the Android app

The companion app is maintained in the separate
[HomeSafeApp repository](https://github.com/joydip09/HomeSafeApp).

It is a React Native Android app. Its documented toolchain currently requires:

- Node.js 22.11 or newer
- JDK 21
- Android SDK Platform 36
- Android NDK 27.1.12297006
- CMake 3.22.1

### Local Wi-Fi connection

1. Flash and power the ESP32.
2. Connect the phone and ESP32 to the same Wi-Fi network, or connect the ESP32
   to the phone’s hotspot in station mode.
3. Read the ESP32 IPv4 address from the serial output or the state snapshot.
4. In the app, open the Home screen settings gear.
5. Enter the ESP32 IPv4 address and save it.
6. Tap **Connect**.

The app connects to:

```text
ws://<esp32-ip-address>:81
```

The app considers the device synchronized only after receiving a valid
protocol-v1 `state_snapshot`.

### Build and run the app

In the app repository:

```bash
npm install
npm start
```

In a second terminal, connect an Android device or start an emulator and run:

```bash
npm run android
```

For a debug APK:

```bash
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
cd ..
```

For a local release-style APK with the JavaScript bundle included:

```bash
cd android
./gradlew assembleRelease
adb install -r app/build/outputs/apk/release/app-release.apk
cd ..
```

The current release configuration is intended for local/device testing and is
not a Play Store signing setup.

### Tunnel connection

For a remote connection, enter the complete public `https://` tunnel URL in
the app. The app converts it to `wss://` while preserving its path and
explicit port. The tunnel must forward WebSocket traffic to the ESP32’s local
`ws://<esp32-ip>:81` endpoint.

Only use a tunnel with authentication and access controls appropriate for the
environment. The firmware protocol currently has no application-level
authentication, authorization, or encryption of its own.

### App behavior and limitations

- The app provides Home, Environment, Security, and History tabs.
- Settings are opened from the Home header.
- A snapshot establishes the complete state; later state updates replace only
  the sections included in each update.
- Events are de-duplicated and retained locally in SQLite, up to 500 records.
- Events are not replayed by the ESP32, so events during disconnection can be
  missed.
- The app marks readings stale when disconnected or older than 15 seconds.
- MQ values are shown as raw ADC counts and hazard flags are shown as reported
  by firmware.
- App notification preferences affect in-app alerts while the app is open.
  This build does not provide Android system notifications or background
  WebSocket delivery.
- The physical alarm is independent of the phone’s connection and notification
  state.

## WebSocket protocol v1

The source of truth is [`include/protocol.h`](./include/protocol.h), with
serialization and command handling in [`src/network.cpp`](./src/network.cpp).
Messages are JSON text frames.

### Common fields

Device-to-app messages generally include:

```json
{
  "version": 1,
  "type": "state_snapshot",
  "timestamp_ms": 123456,
  "sequence": 42,
  "data": {}
}
```

`sequence` is a device-wide outbound sequence number. Event messages also
include an `event_id`. Command requests include a caller-generated
`request_id`, which is returned in the command result for correlation.

### State messages

- `state_snapshot` is sent when a WebSocket client connects and contains
  `device`, `environment`, `hazards`, `audio`, `security`, and `alarms`.
- `state_update` contains only changed sections.
- `acoustic_event` reports a qualifying non-normal audio classification.
- `hazard_event` reports a flame, MQ-2, or MQ-135 hazard transition.
- `security_event` reports security mode, schedule, or intrusion changes.
- `alarm_event` reports environmental/security alarm activation, clearing, or
  silencing.

Example connection and status request:

```json
{
  "version": 1,
  "type": "command",
  "command": "get_status",
  "request_id": "status-001"
}
```

Example successful response shape:

```json
{
  "version": 1,
  "type": "command_result",
  "command": "get_status",
  "request_id": "status-001",
  "success": true,
  "data": {
    "device": {},
    "environment": {},
    "hazards": {},
    "audio": {},
    "security": {},
    "alarms": {}
  }
}
```

### Supported commands

| Command | Data | Effect |
|---|---|---|
| `get_status` | none | Requests a complete current status |
| `set_security_mode` | `{"armed": true/false}` | Arms or disarms security |
| `set_security_schedule` | schedule fields plus enabled state | Saves/enables the daily schedule |
| `silence_alarm` | none | Silences the security alarm only |

Schedule hours must be 0–23, minutes must be 0–59, and arm and disarm times
must not be identical. See the app repository and
[`src/network.cpp`](./src/network.cpp) for the exact command payload and error
handling.

## Testing and diagnostics

The scripts in [`scripts/`](./scripts) are live-device WebSocket checks, not
fully isolated unit tests. Install the Python dependency in a virtual
environment:

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install websockets
```

Update the device IP and run the checks:

```bash
python scripts/test_protocol.py
python scripts/test_homesafe_ws.py
python scripts/test_security.py
python scripts/test_alarm_controls.py
```

These scripts can query status, verify snapshots and updates, arm/disarm
security, silence the security alarm, and exercise reconnect behavior. They
require a powered ESP32 with the firmware running and reachable on TCP port
81.

Useful diagnostics:

- `pio device monitor --baud 115200` shows initialization, Wi-Fi, classifier,
  and alarm logs.
- If the app cannot connect, first verify the phone and ESP32 are on the same
  network, then verify the IP address and that port 81 is reachable.
- If a sensor is reported invalid, inspect its power, ground, signal polarity,
  and wiring before changing thresholds.
- If audio inference is unavailable, check the INMP441 wiring and confirm that
  the bundled model’s sample rate remains 16 kHz mono.

## Known limitations and next steps

- MQ thresholds are raw ADC thresholds and require site-specific calibration.
- The flame digital-output polarity must be confirmed for the installed module.
- There is no protocol authentication or built-in TLS on the ESP32 server.
- The device does not replay events missed while the app is disconnected.
- The app does not deliver background Android notifications.
- NTP availability is required for scheduled security mode.
- The generated Edge Impulse library increases firmware size and should be
  regenerated carefully if the model or labels change.
- A production deployment should add authenticated transport, secret
  injection, enclosure and power validation, watchdog/recovery testing, and
  independent safety certification.

## Related project

- [HomeSafeApp](https://github.com/joydip09/HomeSafeApp) — Android companion
  application and client-side protocol implementation.

## License

No project license is currently declared in this repository. Add a license
before distributing the firmware or bundled model outside the project.

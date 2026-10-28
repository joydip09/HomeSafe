# HomeSafe Audio Classifier

This directory contains the generated **Edge Impulse C++ inference library**
for HomeSafe’s on-device audio classifier. It is consumed by the ESP32-S3
firmware through [`src/audio.cpp`](../../src/audio.cpp) and is not a standalone
application.

The classifier listens to an INMP441 I2S microphone and estimates whether the
current audio window resembles one of five HomeSafe classes:

| Label | Intended meaning |
|---|---|
| `alarm` | Alarm-like sound |
| `distress` | Distress or help-related sound |
| `glass_break` | Glass-breaking sound |
| `impact` | Impact or collision sound |
| `normal` | Background or non-alarm audio |

The firmware treats every class except `normal` as an acoustic safety event
only when its confidence is at least `0.80`. Classification results are still
reported to the app below that threshold; they simply do not generate an
acoustic event.

## Model and signal configuration

The generated model metadata is in
[`src/model-parameters/model_metadata.h`](./src/model-parameters/model_metadata.h)
and the impulse definition, labels, and DSP configuration are in
[`src/model-parameters/model_variables.h`](./src/model-parameters/model-variables.h).

| Property | Value |
|---|---|
| Edge Impulse project | `HomeSafe_v2` |
| Project ID | `1148165` |
| Project owner | `joydip` |
| Sensor | Microphone |
| Input | Mono signed PCM |
| Sample rate | 16,000 Hz |
| Full model window | 32,000 samples / 2 seconds |
| Continuous slices | 4 |
| Samples per inference slice | 8,000 / 500 ms |
| DSP block | MFCC |
| MFCC coefficients | 13 |
| Frame length / stride | 25 ms / 10 ms |
| Filter bank | 32 filters |
| FFT length | 256 |
| Neural-network input features | 2,574 |
| Inference engine | Compiled quantized TensorFlow Lite |
| Input/output type | INT8 / INT8 |
| Largest declared TFLite arena | 10,873 bytes |
| Anomaly detection | Disabled |

The firmware calls `run_classifier_continuous()` after each 8,000-sample slice.
The classifier therefore receives a rolling audio window rather than a new
isolated two-second recording for every result.

## ESP32-S3 integration

The HomeSafe firmware implementation is in
[`src/audio.cpp`](../../src/audio.cpp). It performs the following pipeline:

1. Configure I2S RX on `I2S_NUM_0` at 16 kHz with 32-bit slots.
2. Read microphone samples in blocks of 256 32-bit I2S values.
3. Convert the INMP441’s 24-bit sample carried in each slot to signed 16-bit
   PCM by shifting the sample right by 16 bits.
4. Accumulate 8,000 samples.
5. Expose that slice to Edge Impulse through a `signal_t` callback.
6. Run continuous inference and select the highest-scoring label.
7. Publish the label and confidence in shared HomeSafe state.
8. Queue an `acoustic_event` for connected WebSocket clients when eligible.

The firmware enforces the model’s audio contract at compile time:

```cpp
static_assert(EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME == 1,
              "Model must use mono audio");
static_assert(EI_CLASSIFIER_FREQUENCY == SAMPLE_RATE,
              "Model must use 16 kHz audio");
```

### Microphone wiring

The pin assignments are defined in
[`include/config.h`](../../include/config.h):

| INMP441 signal | ESP32-S3 GPIO |
|---|---:|
| `SCK` / `BCLK` | 4 |
| `WS` / `LRCL` | 5 |
| `SD` / `DOUT` | 6 |
| `L/R` | Select one channel according to the microphone board |
| `VDD` | 3.3 V |
| `GND` | GND |

The driver reads the left I2S channel (`I2S_CHANNEL_FMT_ONLY_LEFT`). Confirm
the INMP441 channel-select wiring on the installed breakout board. Keep the
microphone supply clean and avoid routing its data lines alongside noisy
switching or buzzer wiring.

## Using the library directly

The generated Arduino library exposes the normal Edge Impulse inference API.
Include the generated header and provide a callback that returns the required
PCM samples:

```cpp
#include <HomeSafe_v2_inferencing.h>

int get_audio_data(size_t offset, size_t length, float *output) {
    // Convert or copy `length` mono PCM samples beginning at `offset`.
    return numpy::int16_to_float(audio_buffer + offset, output, length);
}

void classify_slice() {
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_SLICE_SIZE;
    signal.get_data = get_audio_data;

    ei_impulse_result_t result = {};
    EI_IMPULSE_ERROR error =
        run_classifier_continuous(&signal, &result, false);

    if (error == EI_IMPULSE_OK) {
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; ++i) {
            Serial.printf("%s: %.3f\n",
                          result.classification[i].label,
                          result.classification[i].value);
        }
    }
}
```

For a complete ESP32 capture implementation, use HomeSafe’s
[`audio.cpp`](../../src/audio.cpp) rather than copying only the inference call.
The generated examples are also available under
[`examples/esp32/`](./examples/esp32/), including
[`esp32_microphone_continuous`](./examples/esp32/esp32_microphone_continuous/).

Initialize the classifier once before inference:

```cpp
run_classifier_init();
```

For non-continuous inference, use a full
`EI_CLASSIFIER_RAW_SAMPLE_COUNT`-sample signal and `run_classifier()`. For the
HomeSafe firmware, continuous inference is preferred because it avoids
discarding the rolling audio context.

## HomeSafe event policy

The model itself produces probabilities; HomeSafe applies the operational
policy in [`include/config.h`](../../include/config.h):

```cpp
constexpr float CLASSIFICATION_CONFIDENCE_THRESHOLD = 0.80f;
constexpr uint32_t EVENT_COOLDOWN_MS = 3000;
```

An acoustic event is emitted when:

- the winning label is not `normal`;
- confidence is at least `0.80`;
- the same class is not already latched; and
- the three-second event cooldown has elapsed since the previous emitted
  acoustic event.

The latch clears when the result returns to `normal`, falls below the
threshold, or changes class. This reduces repeated events while a single
continuous sound persists. Audio classification does not directly activate
the physical buzzer; it reports an `acoustic_event` to the HomeSafe network
layer. Environmental and security alarms remain separate subsystems.

The device state includes:

```json
{
  "audio": {
    "inference_valid": true,
    "detected_class": "glass_break",
    "confidence": 0.91,
    "last_inference_ms": 123456
  }
}
```

An emitted event has the form:

```json
{
  "version": 1,
  "type": "acoustic_event",
  "event_id": 12,
  "sequence": 87,
  "timestamp_ms": 123456,
  "data": {
    "detected_class": "glass_break",
    "confidence": 0.91
  }
}
```

## Build through HomeSafe firmware

The library is included automatically by the project-local PlatformIO
configuration. From the repository root:

```bash
pio run -e esp32-s3-devkitc-1
pio run -e esp32-s3-devkitc-1 --target upload
pio device monitor --baud 115200
```

Successful startup logs include:

```text
[Audio] I2S initialized: 16 kHz, 32-bit slots
[Audio] TinyML ready | Slice: ...
```

During operation, the firmware logs PCM peak/RMS diagnostics, each model
classification, and DSP/classification timing. Use these diagnostics to
identify disconnected microphones, clipped input, very low signal levels, or
unexpected inference latency.

## Dataset and model updates

This repository contains the generated deployment library, not the original
training dataset or complete Edge Impulse Studio training history. To update
the classifier:

1. Collect and label representative samples for all five classes.
2. Include realistic background recordings from the intended installation.
3. Retrain and evaluate the impulse in Edge Impulse Studio.
4. Check confusion between `normal` and safety-relevant classes.
5. Export a new Arduino library deployment.
6. Replace this generated library as a coherent directory.
7. Rebuild the ESP32 firmware and verify the metadata assertions.
8. Re-test quiet rooms, overlapping sounds, microphone placement, and noisy
   alarm conditions.

When labels, sample rate, window size, or DSP settings change, review
[`src/audio.cpp`](../../src/audio.cpp), the HomeSafe event policy, app display
logic, and any documentation that lists the classes. Do not silently change
the label `normal`: the firmware uses it as the non-event class.

## Limitations

- Classification quality depends on the training data, microphone placement,
  enclosure, room acoustics, and gain.
- A high confidence score is not a guarantee that a sound is dangerous.
- Sounds can be missed while the microphone is disconnected, saturated, or
  masked by another source.
- The current firmware reports audio events but does not independently drive
  the physical alarm from them.
- The generated model is compiled for the metadata in this directory; changing
  the audio format without regenerating the model will invalidate the input
  contract.
- No benchmark accuracy, latency, or false-positive rate is asserted here;
  measure those against the target installation and hardware.

## Licensing and attribution

The surrounding HomeSafe firmware is released under the repository’s MIT
license. This generated Edge Impulse library and its model assets are
**separately licensed**. The generated metadata explicitly references the
applicable Edge Impulse Terms of Service and may require an active paid Edge
Impulse subscription. Review the notices in
[`src/model-parameters/model_metadata.h`](./src/model-parameters/model_metadata.h),
[`src/model-parameters/model_variables.h`](./src/model-parameters/model_variables.h),
and the bundled SDK before using or redistributing this directory.

Do not assume that the repository MIT license grants permission to redistribute
the generated model or Edge Impulse deployment SDK.

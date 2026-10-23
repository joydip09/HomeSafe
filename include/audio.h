#ifndef HOMESAFE_AUDIO_H
#define HOMESAFE_AUDIO_H

#include <Arduino.h>

namespace HomeSafe {
namespace Audio {

bool begin();
void update();
bool isInitialized();

} // namespace Audio
} // namespace HomeSafe

#endif // HOMESAFE_AUDIO_H
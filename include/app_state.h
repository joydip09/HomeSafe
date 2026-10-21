#ifndef HOMESAFE_APP_STATE_H
#define HOMESAFE_APP_STATE_H

#include "models.h"

namespace HomeSafe {

// Access the shared application state.
inline HomeSafeState &getState() {
  static HomeSafeState currentState;
  return currentState;
}

} // namespace HomeSafe

#endif // HOMESAFE_APP_STATE_H
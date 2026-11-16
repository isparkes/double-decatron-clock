#include "DebugManager.h"

DebugManager debugManager;

void DebugManager::setUp(bool newDebug) {
  _debug = newDebug;
}

void DebugManager::debugMsg(const String &message) {
  if (_debug) {
    Serial.println(message);
  }
}

bool DebugManager::getDebug() const {
  return _debug;
}

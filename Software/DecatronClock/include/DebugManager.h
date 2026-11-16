#ifndef DEBUG_MANAGER_H
#define DEBUG_MANAGER_H

#include <Arduino.h>

// ----------------------------------------------------------------------------------------------------
// ------------------------------------- Debug output component ---------------------------------------
// ----------------------------------------------------------------------------------------------------
//
// Unlike the EniacDecatron version this is a single shared instance (declared
// extern here, defined once in DebugManager.cpp) so that every translation unit
// honours the same enable flag.

class DebugManager
{
  public:
    void setUp(bool newDebug);
    void debugMsg(const String &message);
    bool getDebug() const;
  private:
    bool _debug = false;
};

// ----------------- Exported Variables ------------------

extern DebugManager debugManager;

#endif

#pragma once

#include <Arduino.h>

namespace Screen {
void setup();
void clear();
void drawTitle(const char* title, const char* subtitle);
void drawStatus(const char* message);
void drawInputLine(const String& inputLine);
}

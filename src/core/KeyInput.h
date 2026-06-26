#pragma once

#include <Arduino.h>

struct KeyInput {
    String characters;
    bool tab = false;
    bool esc = false;
    bool backspace = false;
    bool del = false;
    bool enter = false;
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

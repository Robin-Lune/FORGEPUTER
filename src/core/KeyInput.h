#pragma once

#include <Arduino.h>

struct KeyInput {
    String characters;
    bool backspace = false;
    bool del = false;
    bool enter = false;
};

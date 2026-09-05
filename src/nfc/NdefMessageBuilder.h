#pragma once

#include <Arduino.h>
#include <vector>

class NdefMessageBuilder {
public:
    static std::vector<uint8_t> textRecord(const String& text);
    static std::vector<uint8_t> urlRecord(const String& url);
};

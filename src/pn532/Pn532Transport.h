#pragma once

#include <Arduino.h>

class Pn532Transport {
public:
    virtual ~Pn532Transport() = default;

    virtual bool begin() = 0;
    virtual void end() = 0;
    virtual size_t write(const uint8_t* data, size_t length) = 0;
    virtual int read() = 0;
    virtual int available() = 0;
    virtual void flush() = 0;
};

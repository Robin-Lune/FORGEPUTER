#pragma once

#include "Pn532Transport.h"

#include <HardwareSerial.h>

class Pn532UartTransport : public Pn532Transport {
public:
    explicit Pn532UartTransport(HardwareSerial& serial);

    bool begin() override;
    void end() override;
    size_t write(const uint8_t* data, size_t length) override;
    int read() override;
    int available() override;
    void flush() override;

private:
    HardwareSerial& serial_;
};

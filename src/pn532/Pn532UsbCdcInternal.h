#pragma once

#include <Arduino.h>

// Tunables shared by the Pn532UsbCdc*.cpp translation units.
namespace Pn532UsbCdc {
constexpr uint16_t wchVid = 0x1A86;
constexpr uint32_t connectTimeoutMs = 4500;
constexpr uint32_t transferTimeoutMs = 300;
constexpr size_t rxLimit = 512;
constexpr size_t transferSize = 64;
constexpr uint32_t serialBaud = 115200;
constexpr uint8_t requestTypeOut = 0x40;
constexpr uint8_t cdcRequestTypeOut = 0x21;
constexpr uint8_t ch34xRequestWriteReg = 0x9A;
constexpr uint8_t ch34xRequestSerialInit = 0xA1;
constexpr uint8_t ch34xRequestModemCtrl = 0xA4;
}

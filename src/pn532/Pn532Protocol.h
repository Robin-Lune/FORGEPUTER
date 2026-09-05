#pragma once

#include <Arduino.h>

// Frame constants and helpers shared by the Pn532Client*.cpp translation units.
namespace Pn532Protocol {
constexpr uint8_t hostToPn532 = 0xD4;
constexpr uint8_t pn532ToHost = 0xD5;
constexpr uint8_t ackFrame[] = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
constexpr uint8_t wakeupFrame[] = {
    0x55, 0x55,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

inline uint8_t checksum(uint8_t value)
{
    return static_cast<uint8_t>(~value + 1);
}

inline const char* statusName(uint8_t status)
{
    switch (status) {
    case 0x00: return "OK";
    case 0x01: return "Timeout";
    case 0x02: return "CRC error";
    case 0x03: return "Parity error";
    case 0x04: return "Erroneous bit count";
    case 0x05: return "Framing error";
    case 0x06: return "Bit collision";
    case 0x07: return "Small buffer";
    case 0x09: return "RF buffer overflow";
    case 0x0A: return "RF field not activated";
    case 0x0B: return "Protocol error";
    case 0x0D: return "Temperature error";
    case 0x0E: return "Internal buffer overflow";
    case 0x10: return "Invalid parameter";
    case 0x12: return "DEP unsupported command";
    case 0x13: return "Data format mismatch";
    case 0x14: return "MIFARE authentication/protocol error";
    case 0x23: return "ISO14443-4 card activation failed";
    case 0x25: return "Invalid target number";
    case 0x26: return "DEP release";
    case 0x27: return "Card disappeared";
    case 0x29: return "NAD missing";
    case 0x2A: return "Over-current";
    case 0x2B: return "NAD mismatch";
    default: return "Unknown status";
    }
}

inline uint16_t crcA(const uint8_t* data, size_t length)
{
    // ISO14443A CRC_A is appended little-endian: READ 04 = 30 04 26 EE.
    uint16_t crc = 0x6363;

    for (size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        byte ^= static_cast<uint8_t>(crc & 0x00FF);
        byte ^= byte << 4;
        crc = (crc >> 8) ^ (static_cast<uint16_t>(byte) << 8) ^ (static_cast<uint16_t>(byte) << 3) ^ (byte >> 4);
    }

    return crc;
}
}

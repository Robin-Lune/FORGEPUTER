#pragma once

#include <Arduino.h>
#include <vector>

enum class Pn532NfcAResponseKind {
    Empty,
    StatusOnly,
    StatusPayload,
    RawPayload,
    ShortPayload,
};

struct Pn532NfcAResult {
    bool transportOk = false;
    bool accepted = false;
    Pn532NfcAResponseKind kind = Pn532NfcAResponseKind::Empty;
    uint8_t status = 0xFF;
    std::vector<uint8_t> payload;
    String error;
};

struct Pn532RawDiagnostic {
    std::vector<uint8_t> txFrame;
    std::vector<uint8_t> ackBytes;
    std::vector<uint8_t> rxFrame;
    std::vector<uint8_t> framePayload;
    std::vector<uint8_t> pn532Payload;
    bool ackOk = false;
    bool frameOk = false;
    bool responseCodeOk = false;
    bool hasStatus = false;
    uint8_t inDataExchangeStatus = 0xFF;
    std::vector<uint8_t> tagPayload;
    String parsed;
    String error;
    uint32_t elapsedMs = 0;
};

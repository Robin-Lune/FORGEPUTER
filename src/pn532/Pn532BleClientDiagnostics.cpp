// Pn532BleClient - raw PN532 command over BLE with full frame capture.

#include "Pn532BleClient.h"

namespace {
uint8_t diagChecksum(uint8_t value)
{
    return static_cast<uint8_t>(~value + 1);
}

uint8_t diagDcs(const uint8_t* data, size_t length)
{
    uint8_t sum = 0;

    for (size_t i = 0; i < length; i++) {
        sum += data[i];
    }

    return diagChecksum(sum);
}

const char* diagStatusName(uint8_t status)
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
    case 0x10: return "Invalid parameter";
    case 0x14: return "MIFARE authentication/protocol error";
    case 0x25: return "Invalid target number";
    case 0x27: return "Card disappeared";
    default: return "Unknown status";
    }
}
}

Pn532RawDiagnostic Pn532BleClient::diagnoseRawCommand(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* label, bool hasStatusByte, uint16_t timeoutMs)
{
    Pn532RawDiagnostic diag;
    const uint32_t startedAt = millis();

    if (!connected_ || !ble_ || ble_->chrWrite == nullptr) {
        diag.error = "BLE not connected";
        return diag;
    }

    if (command.empty() || command.size() > 253) {
        diag.error = "Bad command";
        return diag;
    }

    std::vector<uint8_t> payload = {ble_->DATA_TIF_SEND};
    payload.insert(payload.end(), command.begin(), command.end());
    const uint8_t length = payload.size();
    diag.txFrame = {ble_->DATA_PREAMBLE, ble_->DATA_START_CODE[0], ble_->DATA_START_CODE[1], length, diagChecksum(length)};
    diag.txFrame.insert(diag.txFrame.end(), payload.begin(), payload.end());
    diag.txFrame.push_back(diagDcs(payload.data(), payload.size()));
    diag.txFrame.push_back(ble_->DATA_POSTAMBLE);

    ble_->pn532Responses.clear();
    ble_->pn532bleBuffer.clear();
    const bool wrote = ble_->chrWrite->writeValue(diag.txFrame.data(), diag.txFrame.size(), true);

    if (!wrote) {
        diag.error = "BLE write failed";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    diag.ackOk = true;

    while (millis() - startedAt < timeoutMs) {
        for (const auto& response : ble_->pn532Responses) {
            if (response.command != command[0]) {
                continue;
            }

            diag.rxFrame.assign(response.raw, response.raw + response.length);

            if (response.length > 12) {
                const uint8_t frameLength = response.raw[9];
                const size_t start = 11;
                const size_t available = response.length > start ? response.length - start : 0;
                const size_t copyLength = min(static_cast<size_t>(frameLength), available);
                diag.framePayload.assign(response.raw + start, response.raw + start + copyLength);
            }

            diag.frameOk = !diag.framePayload.empty();
            diag.responseCodeOk = diag.framePayload.size() >= 2 && diag.framePayload[0] == ble_->DATA_TIF_RECEIVE && diag.framePayload[1] == expectedResponseCode;

            if (diag.responseCodeOk && diag.framePayload.size() > 2) {
                diag.pn532Payload.assign(diag.framePayload.begin() + 2, diag.framePayload.end());
            }

            diag.hasStatus = hasStatusByte && !diag.pn532Payload.empty();

            if (diag.hasStatus) {
                diag.inDataExchangeStatus = diag.pn532Payload[0];

                if (diag.pn532Payload.size() > 1) {
                    diag.tagPayload.assign(diag.pn532Payload.begin() + 1, diag.pn532Payload.end());
                }
            } else {
                diag.tagPayload = diag.pn532Payload;
            }

            if (!diag.responseCodeOk) {
                diag.error = "Unexpected response";
                diag.parsed = diag.error;
            } else if (!diag.hasStatus) {
                diag.parsed = String(label) + " response OK";
            } else if (diag.inDataExchangeStatus == 0x00) {
                diag.parsed = String(label) + " status 00 OK";
            } else {
                diag.parsed = String(label) + " status " + String(diag.inDataExchangeStatus, HEX) + " " + diagStatusName(diag.inDataExchangeStatus);
            }

            diag.elapsedMs = millis() - startedAt;
            ble_->pn532Responses.clear();
            return diag;
        }

        delay(5);
    }

    diag.error = "BLE RX timeout";
    diag.elapsedMs = millis() - startedAt;
    return diag;
}

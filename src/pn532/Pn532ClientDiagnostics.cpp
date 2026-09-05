// Pn532Client - raw command diagnostics with full TX/RX frame capture.

#include "Pn532Client.h"

#include <cstring>

#include "Pn532Protocol.h"

using Pn532Protocol::ackFrame;
using Pn532Protocol::checksum;
using Pn532Protocol::hostToPn532;
using Pn532Protocol::pn532ToHost;

Pn532RawDiagnostic Pn532Client::diagnoseInDataExchange(const std::vector<uint8_t>& tagCommand, uint16_t timeoutMs)
{
    std::vector<uint8_t> command = {0x40, targetNumber_};
    command.insert(command.end(), tagCommand.begin(), tagCommand.end());
    return diagnoseTagCommand(command, 0x41, "InDataExchange", true, timeoutMs);
}

Pn532RawDiagnostic Pn532Client::diagnoseInCommunicateThru(const std::vector<uint8_t>& tagCommand, uint16_t timeoutMs)
{
    std::vector<uint8_t> command = {0x42};
    command.insert(command.end(), tagCommand.begin(), tagCommand.end());
    return diagnoseTagCommand(command, 0x43, "InCommunicateThru", true, timeoutMs);
}

Pn532RawDiagnostic Pn532Client::diagnoseRawCommand(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* label, bool hasStatusByte, uint16_t timeoutMs)
{
    return diagnoseTagCommand(command, expectedResponseCode, label, hasStatusByte, timeoutMs);
}

Pn532RawDiagnostic Pn532Client::diagnoseTagCommand(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* statusLabel, bool hasStatusByte, uint16_t timeoutMs)
{
    Pn532RawDiagnostic diag;
    const uint32_t startedAt = millis();

    if (command.size() > 253) {
        diag.error = "Frame too long";
        return diag;
    }

    const uint8_t length = command.size() + 1;
    uint8_t dataSum = hostToPn532;
    diag.txFrame = {0x00, 0x00, 0xFF, length, checksum(length), hostToPn532};

    for (uint8_t byte : command) {
        diag.txFrame.push_back(byte);
        dataSum += byte;
    }

    diag.txFrame.push_back(checksum(dataSum));
    diag.txFrame.push_back(0x00);
    drainInput();
    transport_->write(diag.txFrame.data(), diag.txFrame.size());
    transport_->flush();

    for (int i = 0; i < 6; i++) {
        const int value = readByte(timeoutMs);

        if (value < 0) {
            diag.error = "ACK timeout";
            diag.elapsedMs = millis() - startedAt;
            return diag;
        }

        diag.ackBytes.push_back(static_cast<uint8_t>(value));
    }

    diag.ackOk = diag.ackBytes.size() == sizeof(ackFrame) && memcmp(diag.ackBytes.data(), ackFrame, sizeof(ackFrame)) == 0;

    if (!diag.ackOk) {
        diag.error = "Bad ACK";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    int preamble = readByte(timeoutMs);

    while (preamble >= 0 && preamble != 0x00) {
        preamble = readByte(timeoutMs);
    }

    if (preamble < 0) {
        diag.error = "RX timeout";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    diag.rxFrame.push_back(0x00);

    const int z = readByte(timeoutMs);
    const int ff = readByte(timeoutMs);

    if (z < 0 || ff < 0) {
        diag.error = "Short preamble";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    diag.rxFrame.push_back(static_cast<uint8_t>(z));
    diag.rxFrame.push_back(static_cast<uint8_t>(ff));

    if (z != 0x00 || ff != 0xFF) {
        diag.error = "Bad preamble";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    const int frameLength = readByte(timeoutMs);
    const int lcs = readByte(timeoutMs);

    if (frameLength < 0 || lcs < 0) {
        diag.error = "No length";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    diag.rxFrame.push_back(static_cast<uint8_t>(frameLength));
    diag.rxFrame.push_back(static_cast<uint8_t>(lcs));

    if (static_cast<uint8_t>(frameLength + lcs) != 0) {
        diag.error = "Bad length";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    uint8_t sum = 0;

    for (int i = 0; i < frameLength; i++) {
        const int value = readByte(timeoutMs);

        if (value < 0) {
            diag.error = "Short frame";
            diag.elapsedMs = millis() - startedAt;
            return diag;
        }

        diag.framePayload.push_back(static_cast<uint8_t>(value));
        diag.rxFrame.push_back(static_cast<uint8_t>(value));
        sum += static_cast<uint8_t>(value);
    }

    const int dcs = readByte(timeoutMs);
    const int postamble = readByte(timeoutMs);

    if (dcs >= 0) {
        diag.rxFrame.push_back(static_cast<uint8_t>(dcs));
    }

    if (postamble >= 0) {
        diag.rxFrame.push_back(static_cast<uint8_t>(postamble));
    }

    if (dcs < 0 || postamble < 0 || static_cast<uint8_t>(sum + dcs) != 0 || postamble != 0x00) {
        diag.error = "Bad checksum";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    diag.frameOk = true;

    if (diag.framePayload.size() < 2) {
        diag.error = "Short payload";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    diag.responseCodeOk = diag.framePayload[0] == pn532ToHost && diag.framePayload[1] == expectedResponseCode;
    diag.pn532Payload.assign(diag.framePayload.begin() + 2, diag.framePayload.end());

    if (!diag.responseCodeOk) {
        diag.error = "Unexpected response";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    diag.hasStatus = hasStatusByte && !diag.pn532Payload.empty();

    if (diag.hasStatus) {
        diag.inDataExchangeStatus = diag.pn532Payload[0];

        if (diag.pn532Payload.size() > 1) {
            diag.tagPayload.assign(diag.pn532Payload.begin() + 1, diag.pn532Payload.end());
        }
    } else if (!diag.pn532Payload.empty()) {
        diag.tagPayload = diag.pn532Payload;
    }

    if (!diag.hasStatus && diag.responseCodeOk) {
        diag.parsed = String(statusLabel) + " response OK";
    } else if (diag.pn532Payload.empty()) {
        diag.parsed = String("No ") + statusLabel + " status";
    } else if (diag.inDataExchangeStatus == 0x00) {
        diag.parsed = String(statusLabel) + " status 00 OK";
    } else {
        diag.parsed = String(statusLabel) + " status " + String(diag.inDataExchangeStatus, HEX) + " " + Pn532Protocol::statusName(diag.inDataExchangeStatus);
    }

    diag.elapsedMs = millis() - startedAt;
    return diag;
}

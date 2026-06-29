#include "Pn532Client.h"

#include <cstring>

namespace {
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

uint8_t checksum(uint8_t value)
{
    return static_cast<uint8_t>(~value + 1);
}

const char* pn532StatusName(uint8_t status)
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

uint16_t crcA(const uint8_t* data, size_t length)
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

Pn532Client::Pn532Client(Pn532Transport& transport)
    : transport_(&transport)
{
}

void Pn532Client::setTransport(Pn532Transport& transport)
{
    transport_ = &transport;
    lastError_ = "";
}

bool Pn532Client::begin()
{
    if (!transport_->begin()) {
        setError("Transport failed");
        return false;
    }

    for (int attempt = 0; attempt < 3; attempt++) {
        drainInput();
        transport_->write(wakeupFrame, sizeof(wakeupFrame));
        transport_->flush();
        delay(120);
        drainInput();

        if (setNormalMode()) {
            return true;
        }

        delay(120);
    }

    return false;
}

bool Pn532Client::setNormalMode()
{
    std::vector<uint8_t> response;
    return rawCommand({0x14, 0x01, 0x14, 0x01}, response, 2000);
}

bool Pn532Client::scanIso14443A(CardInfo& info)
{
    std::vector<uint8_t> response;

    if (!rawCommand({0x4A, 0x01, 0x00}, response, 1000)) {
        return false;
    }

    if (response.size() < 7 || response[0] == 0) {
        setError("No ISO14443A tag");
        return false;
    }

    const uint16_t atqa = (static_cast<uint16_t>(response[2]) << 8) | response[3];
    const uint8_t sak = response[4];
    const uint8_t uidLength = response[5];

    if (response.size() < static_cast<size_t>(6 + uidLength)) {
        setError("Short UID frame");
        return false;
    }

    targetNumber_ = response[1];
    info.type = detectIso14443AType(atqa, sak);
    info.uid = bytesToHex(&response[6], uidLength, true);
    info.atqa = bytesToHex(&response[2], 2);
    info.sak = bytesToHex(&response[4], 1);
    info.memoryReadable = info.type == CardType::MifareClassicMini || info.type == CardType::MifareClassic1K || info.type == CardType::MifareClassic4K || info.type == CardType::MifareUltralight;
    info.canWrite = info.memoryReadable;
    info.canEmulate = info.type == CardType::MifareClassic1K || info.type == CardType::MifareUltralight;
    info.isPartial = false;
    return true;
}

bool Pn532Client::mifareAuthenticate(const CardInfo& info, uint8_t block, const uint8_t key[6], bool keyA)
{
    std::vector<uint8_t> uid;

    if (!hexToBytes(info.uid, uid) || uid.size() < 4) {
        setError("Bad UID");
        return false;
    }

    std::vector<uint8_t> command = {0x40, targetNumber_, static_cast<uint8_t>(keyA ? 0x60 : 0x61), block};
    command.insert(command.end(), key, key + 6);
    command.insert(command.end(), uid.begin(), uid.begin() + 4);

    std::vector<uint8_t> response;

    if (!rawCommand(command, response, 1000)) {
        return false;
    }

    if (response.empty() || response[0] != 0x00) {
        setError("Auth failed");
        return false;
    }

    return true;
}

bool Pn532Client::mifareReadBlock(uint8_t block, uint8_t out[16])
{
    std::vector<uint8_t> response;

    if (!rawCommand({0x40, targetNumber_, 0x30, block}, response, 1000)) {
        return false;
    }

    if (response.size() < 17 || response[0] != 0x00) {
        setError("Read failed");
        return false;
    }

    memcpy(out, &response[1], 16);
    return true;
}

bool Pn532Client::mifareWriteBlock(uint8_t block, const uint8_t data[16])
{
    std::vector<uint8_t> command = {0x40, targetNumber_, 0xA0, block};
    command.insert(command.end(), data, data + 16);
    std::vector<uint8_t> response;

    if (!rawCommand(command, response, 1000)) {
        return false;
    }

    return !response.empty() && response[0] == 0x00;
}

bool Pn532Client::ntagReadPages(uint8_t page, uint8_t out[16])
{
    Pn532NfcAResult raw = type2TransceiveRaw({0x30, page}, 16, 1000);

    if (raw.accepted) {
        memcpy(out, raw.payload.data(), 16);
        return true;
    }

    std::vector<uint8_t> response;
    if (!rawCommand({0x40, targetNumber_, 0x30, page}, response, 1000)) {
        return false;
    }

    if (response.size() >= 17 && response[0] == 0x00) {
        memcpy(out, &response[1], 16);
        return true;
    }

    if (response.size() >= 16) {
        memcpy(out, response.data(), 16);
        return true;
    }

    setError("NTAG read failed");
    return false;
}

bool Pn532Client::ntagFastRead(uint8_t startPage, uint8_t endPage, std::vector<uint8_t>& out)
{
    out.clear();

    if (endPage < startPage) {
        setError("Bad fast range");
        return false;
    }

    Pn532NfcAResult raw = type2TransceiveRaw({0x3A, startPage, endPage}, 4, 1500);

    if (raw.accepted) {
        out = raw.payload;
        return true;
    }

    std::vector<uint8_t> response;
    if (!rawCommand({0x40, targetNumber_, 0x3A, startPage, endPage}, response, 1500)) {
        return false;
    }

    if (response.empty()) {
        setError("Fast read failed");
        return false;
    }

    if (response[0] == 0x00) {
        out.assign(response.begin() + 1, response.end());
    } else {
        out.assign(response.begin(), response.end());
    }

    return !out.empty();
}

Pn532NfcAResult Pn532Client::nfcATransceive(const std::vector<uint8_t>& tagCommand, size_t expectedPayloadMin, uint16_t timeoutMs)
{
    Pn532NfcAResult result;
    std::vector<uint8_t> command = {0x40, targetNumber_};
    command.insert(command.end(), tagCommand.begin(), tagCommand.end());

    std::vector<uint8_t> response;

    if (!rawCommand(command, response, timeoutMs)) {
        result.error = lastError_;
        return result;
    }

    result.transportOk = true;

    if (response.empty()) {
        result.kind = Pn532NfcAResponseKind::Empty;
        result.error = "Empty response";
        return result;
    }

    if (response[0] == 0x00) {
        result.status = 0x00;

        if (response.size() == 1) {
            result.accepted = expectedPayloadMin == 0;
            result.kind = Pn532NfcAResponseKind::StatusOnly;
            return result;
        }

        result.payload.assign(response.begin() + 1, response.end());
        result.accepted = result.payload.size() >= expectedPayloadMin;
        result.kind = result.accepted ? Pn532NfcAResponseKind::StatusPayload : Pn532NfcAResponseKind::ShortPayload;
        return result;
    }

    if (expectedPayloadMin > 0 && response.size() >= expectedPayloadMin) {
        result.payload = response;
        result.accepted = true;
        result.kind = Pn532NfcAResponseKind::RawPayload;
        return result;
    }

    result.status = response[0];
    result.kind = response.size() > 1 ? Pn532NfcAResponseKind::ShortPayload : Pn532NfcAResponseKind::StatusOnly;
    result.payload = response;
    result.error = "Status " + String(response[0], HEX);
    return result;
}

Pn532NfcAResult Pn532Client::type2TransceiveRaw(const std::vector<uint8_t>& tagCommand, size_t expectedPayloadMin, uint16_t timeoutMs)
{
    Pn532NfcAResult result;

    if (tagCommand.empty()) {
        result.error = "Empty Type2 command";
        return result;
    }

    std::vector<uint8_t> tagFrame = tagCommand;
    const uint16_t crc = crcA(tagFrame.data(), tagFrame.size());
    tagFrame.push_back(static_cast<uint8_t>(crc & 0x00FF));
    tagFrame.push_back(static_cast<uint8_t>((crc >> 8) & 0x00FF));

    std::vector<uint8_t> command = {0x42};
    command.insert(command.end(), tagFrame.begin(), tagFrame.end());
    std::vector<uint8_t> response;

    if (!rawCommand(command, response, timeoutMs)) {
        result.error = lastError_;
        return result;
    }

    result.transportOk = true;

    if (response.empty()) {
        result.kind = Pn532NfcAResponseKind::Empty;
        result.error = "Empty Type2 response";
        return result;
    }

    result.status = response[0];

    if (response[0] != 0x00) {
        result.payload = response;
        result.kind = response.size() > 1 ? Pn532NfcAResponseKind::ShortPayload : Pn532NfcAResponseKind::StatusOnly;
        result.error = "Type2 status " + String(response[0], HEX);
        return result;
    }

    if (response.size() < 3) {
        result.kind = Pn532NfcAResponseKind::ShortPayload;
        result.error = "Type2 response missing CRC";
        return result;
    }

    result.payload.assign(response.begin() + 1, response.end() - 2);
    result.accepted = result.payload.size() >= expectedPayloadMin;
    result.kind = result.accepted ? Pn532NfcAResponseKind::StatusPayload : Pn532NfcAResponseKind::ShortPayload;

    if (!result.accepted) {
        result.error = "Type2 short payload";
    }

    return result;
}

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
        diag.parsed = String(statusLabel) + " status " + String(diag.inDataExchangeStatus, HEX) + " " + pn532StatusName(diag.inDataExchangeStatus);
    }

    diag.elapsedMs = millis() - startedAt;
    return diag;
}

bool Pn532Client::ntagWritePage(uint8_t page, const uint8_t data[4])
{
    std::vector<uint8_t> command = {0x40, targetNumber_, 0xA2, page};
    command.insert(command.end(), data, data + 4);
    std::vector<uint8_t> response;

    if (!rawCommand(command, response, 1000)) {
        return false;
    }

    return !response.empty() && response[0] == 0x00;
}

bool Pn532Client::rawCommand(const std::vector<uint8_t>& command, std::vector<uint8_t>& response, uint16_t timeoutMs)
{
    response.clear();

    if (command.empty()) {
        setError("Empty command");
        return false;
    }

    drainInput();

    if (!sendFrame(command)) {
        return false;
    }

    if (!readAck(timeoutMs)) {
        return false;
    }

    std::vector<uint8_t> frame;

    if (!readFrame(frame, timeoutMs)) {
        return false;
    }

    if (frame.size() < 2 || frame[0] != pn532ToHost || frame[1] != static_cast<uint8_t>(command[0] + 1)) {
        setError("Unexpected response");
        return false;
    }

    response.assign(frame.begin() + 2, frame.end());
    return true;
}

uint8_t Pn532Client::selectedTargetNumber() const
{
    return targetNumber_;
}

String Pn532Client::lastError() const
{
    return lastError_;
}

bool Pn532Client::sendFrame(const std::vector<uint8_t>& command)
{
    if (command.size() > 253) {
        setError("Frame too long");
        return false;
    }

    std::vector<uint8_t> frame;
    const uint8_t length = command.size() + 1;
    uint8_t dataSum = hostToPn532;

    frame.reserve(command.size() + 8);
    frame.push_back(0x00);
    frame.push_back(0x00);
    frame.push_back(0xFF);
    frame.push_back(length);
    frame.push_back(checksum(length));
    frame.push_back(hostToPn532);

    for (uint8_t byte : command) {
        frame.push_back(byte);
        dataSum += byte;
    }

    frame.push_back(checksum(dataSum));
    frame.push_back(0x00);
    transport_->write(frame.data(), frame.size());
    transport_->flush();
    return true;
}

bool Pn532Client::readAck(uint16_t timeoutMs)
{
    for (uint8_t expected : ackFrame) {
        const int value = readByte(timeoutMs);

        if (value < 0 || static_cast<uint8_t>(value) != expected) {
            if (value < 0) {
                setError("PN532 no ACK timeout");
            } else {
                setError("PN532 bad ACK " + String(value, HEX));
            }

            return false;
        }
    }

    return true;
}

bool Pn532Client::readFrame(std::vector<uint8_t>& frame, uint16_t timeoutMs)
{
    frame.clear();

    int preamble = readByte(timeoutMs);

    while (preamble >= 0 && preamble != 0x00) {
        preamble = readByte(timeoutMs);
    }

    if (preamble < 0) {
        setError("Response timeout");
        return false;
    }

    if (readByte(timeoutMs) != 0x00 || readByte(timeoutMs) != 0xFF) {
        setError("Bad preamble");
        return false;
    }

    const int length = readByte(timeoutMs);
    const int lcs = readByte(timeoutMs);

    if (length < 0 || lcs < 0 || static_cast<uint8_t>(length + lcs) != 0) {
        setError("Bad length");
        return false;
    }

    uint8_t sum = 0;

    for (int i = 0; i < length; i++) {
        const int value = readByte(timeoutMs);

        if (value < 0) {
            setError("Short frame");
            return false;
        }

        frame.push_back(value);
        sum += value;
    }

    const int dcs = readByte(timeoutMs);
    const int postamble = readByte(timeoutMs);

    if (dcs < 0 || postamble < 0 || static_cast<uint8_t>(sum + dcs) != 0 || postamble != 0x00) {
        setError("Bad checksum");
        return false;
    }

    return true;
}

int Pn532Client::readByte(uint16_t timeoutMs)
{
    const unsigned long start = millis();

    while (millis() - start < timeoutMs) {
        if (transport_->available() > 0) {
            return transport_->read();
        }

        delay(1);
    }

    return -1;
}

void Pn532Client::drainInput()
{
    const uint32_t start = millis();

    while (millis() - start < 20) {
        while (transport_->available() > 0) {
            transport_->read();
        }

        delay(1);
    }
}

void Pn532Client::setError(const String& message)
{
    lastError_ = message;
}

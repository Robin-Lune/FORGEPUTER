// Pn532Client - transport lifecycle and PN532 commands.
// Frame IO lives in Pn532ClientFraming.cpp, raw captures in
// Pn532ClientDiagnostics.cpp, shared frame constants in Pn532Protocol.h.

#include "Pn532Client.h"

#include <cstring>

#include "Pn532Protocol.h"

using Pn532Protocol::crcA;
using Pn532Protocol::pn532ToHost;
using Pn532Protocol::wakeupFrame;

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
    info.family = cardFamilyForType(info.type);
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

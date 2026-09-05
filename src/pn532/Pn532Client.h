#pragma once

#include "../nfc/CardInfo.h"
#include "Pn532NfcAResult.h"
#include "Pn532Transport.h"

#include <vector>

// Implementation split across Pn532Client*.cpp:
//   Pn532Client.cpp             lifecycle and PN532 commands
//   Pn532ClientFraming.cpp      frame IO on the transport
//   Pn532ClientDiagnostics.cpp  raw command captures
// Shared frame constants live in Pn532Protocol.h.
class Pn532Client {
public:
    explicit Pn532Client(Pn532Transport& transport);

    void setTransport(Pn532Transport& transport);
    bool begin();
    bool setNormalMode();
    bool scanIso14443A(CardInfo& info);
    bool mifareAuthenticate(const CardInfo& info, uint8_t block, const uint8_t key[6], bool keyA);
    bool mifareReadBlock(uint8_t block, uint8_t out[16]);
    bool mifareWriteBlock(uint8_t block, const uint8_t data[16]);
    bool ntagReadPages(uint8_t page, uint8_t out[16]);
    bool ntagFastRead(uint8_t startPage, uint8_t endPage, std::vector<uint8_t>& out);
    Pn532NfcAResult nfcATransceive(const std::vector<uint8_t>& tagCommand, size_t expectedPayloadMin = 0, uint16_t timeoutMs = 1000);
    Pn532NfcAResult type2TransceiveRaw(const std::vector<uint8_t>& tagCommand, size_t expectedPayloadMin = 0, uint16_t timeoutMs = 1000);
    Pn532RawDiagnostic diagnoseRawCommand(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* label, bool hasStatusByte, uint16_t timeoutMs = 500);
    Pn532RawDiagnostic diagnoseInDataExchange(const std::vector<uint8_t>& tagCommand, uint16_t timeoutMs = 150);
    Pn532RawDiagnostic diagnoseInCommunicateThru(const std::vector<uint8_t>& tagCommand, uint16_t timeoutMs = 500);
    bool ntagWritePage(uint8_t page, const uint8_t data[4]);
    bool rawCommand(const std::vector<uint8_t>& command, std::vector<uint8_t>& response, uint16_t timeoutMs = 1000);
    uint8_t selectedTargetNumber() const;
    String lastError() const;

private:
    Pn532Transport* transport_;
    String lastError_;
    uint8_t targetNumber_ = 0x01;

    bool sendFrame(const std::vector<uint8_t>& command);
    bool readAck(uint16_t timeoutMs);
    bool readFrame(std::vector<uint8_t>& frame, uint16_t timeoutMs);
    Pn532RawDiagnostic diagnoseTagCommand(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* statusLabel, bool hasStatusByte, uint16_t timeoutMs);
    int readByte(uint16_t timeoutMs);
    void drainInput();
    void setError(const String& message);
};

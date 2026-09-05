#pragma once

#include "../nfc/CardInfo.h"
#include "Pn532NfcAResult.h"
#include "../storage/KeyStore.h"

#define private public
#include <pn532_ble.h>
#undef private
#include <memory>

// Implementation split across Pn532BleClient*.cpp:
//   Pn532BleClient.cpp             link lifecycle, scan, read dispatch, retry
//   Pn532BleClientReaders.cpp      Type 2 raw transceive, MFC and ISO15693
//   Pn532BleClientNtag.cpp         MFU/NTAG read and report
//   Pn532BleClientEmulation.cpp    NDEF emulation and APDU responder
//   Pn532BleClientDiagnostics.cpp  raw PN532 command capture
class Pn532BleClient {
public:
    explicit Pn532BleClient(KeyStore& keys);

    bool begin();
    void disconnect();
    bool isConnected() const;
    bool isKiller() const;
    bool scan(CardInfo& info);
    bool readAuto(NfcDump& dump);
    bool retryMissing(NfcDump& dump);
    bool startNdefEmulation(const NfcDump& dump);
    bool startNdefEmulation(const std::vector<uint8_t>& ndefRecord, const CardInfo& card);
    void stopNdefEmulation();
    bool tickNdefEmulation();
    bool isEmulating() const;
    int emulationExchangeCount() const;
    Pn532RawDiagnostic diagnoseRawCommand(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* label, bool hasStatusByte, uint16_t timeoutMs = 500);
    void setProgressCallback(void* context, NfcReadProgressCallback callback);
    String deviceName() const;
    String lastError() const;

private:
    std::unique_ptr<PN532_BLE> ble_;
    KeyStore& keys_;
    bool connected_ = false;
    bool killer_ = false;
    bool emulating_ = false;
    int emulationExchangeCount_ = 0;
    uint16_t selectedEmuFile_ = 0;
    String deviceName_;
    String lastError_;
    std::vector<uint8_t> emulatedNdefFile_;
    std::vector<uint8_t> emulatedCcFile_;
    void* progressContext_ = nullptr;
    NfcReadProgressCallback progressCallback_ = nullptr;

    bool readMifareClassic(const PN532_BLE::Iso14aTagInfo& tag, const CardInfo& info, NfcDump& dump);
    bool readNtag(const PN532_BLE::Iso14aTagInfo& tag, const CardInfo& info, NfcDump& dump);
    bool readIso15693(const PN532_BLE::Iso15TagInfo& tag, NfcDump& dump);
    Pn532NfcAResult type2TransceiveRaw(const std::vector<uint8_t>& tagCommand, size_t expectedPayloadMin, uint16_t timeoutMs);
    bool extractType2Ndef(const NfcDump& dump, std::vector<uint8_t>& ndef) const;
    std::vector<uint8_t> buildTargetParameters(const NfcDump& dump) const;
    bool respondToApdu(const std::vector<uint8_t>& apdu);
    bool sendStatus(uint8_t sw1, uint8_t sw2);
    uint8_t firstBlockForSector(int sector) const;
    uint8_t blockCountForSector(int sector) const;
    bool isMifareClassic(CardType type) const;
    bool isNtag(CardType type) const;
    void emitProgress(const String& title, const String& detail, int current, int total, bool done, bool ok);
    void setError(const String& error);
};

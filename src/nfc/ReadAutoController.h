#pragma once

#include "../pn532/Pn532Client.h"
#include "../storage/KeyStore.h"
#include "CardInfo.h"
#include "MifareUltralightReader.h"

class ReadAutoController {
public:
    ReadAutoController(Pn532Client& pn532, KeyStore& keys);

    bool scan(CardInfo& info);
    bool readAuto(NfcDump& dump);
    bool retryMissing(NfcDump& dump);
    void setProgressCallback(void* context, NfcReadProgressCallback callback);
    String lastError() const;

private:
    Pn532Client& pn532_;
    KeyStore& keys_;
    MifareUltralightReader ultralightReader_;
    String lastError_;
    void* progressContext_ = nullptr;
    NfcReadProgressCallback progressCallback_ = nullptr;

    bool readMifareClassic(const CardInfo& info, NfcDump& dump);
    bool readNtag(const CardInfo& info, NfcDump& dump);
    uint8_t firstBlockForSector(int sector) const;
    uint8_t blockCountForSector(int sector) const;
    bool isMifareClassic(CardType type) const;
    void emitProgress(const String& title, const String& detail, int current, int total, bool done, bool ok);
    void setError(const String& error);
};

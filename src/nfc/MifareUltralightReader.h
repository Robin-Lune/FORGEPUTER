#pragma once

#include "../pn532/Pn532Client.h"
#include "CardReader.h"

// Implementation split across MifareUltralight*.cpp:
//   MifareUltralightReader.cpp  page reads and fallbacks
//   MifareUltralightReport.cpp  version classification and report
class MifareUltralightReader : public CardReader {
public:
    explicit MifareUltralightReader(Pn532Client& pn532);

    void setProgressCallback(void* context, NfcReadProgressCallback callback);
    bool supports(const CardInfo& card) const override;
    bool read(const CardInfo& card, NfcDump& dump) override;
    bool retry(NfcDump& dump) override;
    String lastError() const override;

private:
    Pn532Client& pn532_;
    String lastError_;
    void* progressContext_ = nullptr;
    NfcReadProgressCallback progressCallback_ = nullptr;

    bool tryReadWindow(uint8_t startPage, uint8_t endPage, std::vector<uint8_t>& out, String& source);
    void classifyVersion(const std::vector<uint8_t>& version, CardInfo& card, int& maxPages) const;
    void enrichType2Model(NfcDump& dump) const;
    void enrichReport(NfcDump& dump, const std::vector<uint8_t>& version, bool rawReadOk, bool rawFastReadWorked, bool fallbackUsed, int fallbackRecovered) const;
    void emitProgress(const String& title, const String& detail, int current, int total, bool done, bool ok);
    void setError(const String& error);
};

#pragma once

#include "CardInfo.h"

enum class NfcFamily {
    Unknown,
    NfcA,
    MifareClassic,
    MifareUltralight,
    Desfire,
    NfcB,
    Iso15693,
    Felica,
};

class CardReader {
public:
    virtual ~CardReader() = default;
    virtual bool supports(const CardInfo& card) const = 0;
    virtual bool read(const CardInfo& card, NfcDump& dump) = 0;
    virtual bool retry(NfcDump& dump) = 0;
    virtual String lastError() const = 0;
};


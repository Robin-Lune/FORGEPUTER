#pragma once

#include <Arduino.h>
#include <vector>

enum class CardType {
    Unknown,
    Iso14443A,
    MifareClassicMini,
    MifareClassic1K,
    MifareClassic4K,
    MifareUltralight,
    Ntag213,
    Ntag215,
    Ntag216,
    Iso15693,
    EM4100,
    Desfire,
    BankCardUnsupported,
};

struct CardInfo {
    CardType type = CardType::Unknown;
    String uid;
    String atqa;
    String sak;
    bool memoryReadable = false;
    bool canWrite = false;
    bool canEmulate = false;
    bool isPartial = false;
};

enum class DumpStatus {
    Empty,
    InfoOnly,
    Partial,
    Full,
};

struct NfcDump {
    String name;
    CardInfo card;
    DumpStatus status = DumpStatus::Empty;
    std::vector<uint8_t> data;
    int unitsTotal = 0;
    int unitsRead = 0;
    int protectedUnits = 0;
    int unknownUnits = 0;
    String product;
    String vendor;
    String storage;
    String protocol;
    String getVersion;
    String auth0;
    String access;
    std::vector<uint8_t> missingUnits;
    std::vector<String> reportLines;
};

struct NfcReadProgress {
    String title;
    String detail;
    int current = 0;
    int total = 0;
    bool done = false;
    bool ok = false;
};

using NfcReadProgressCallback = void (*)(void* context, const NfcReadProgress& progress);

const char* cardTypeName(CardType type);
const char* cardTypeSlug(CardType type);
const char* dumpStatusName(DumpStatus status);
String bytesToHex(const uint8_t* data, size_t length, bool separator = false);
bool hexToBytes(const String& hex, std::vector<uint8_t>& out);
CardType detectIso14443AType(uint16_t atqa, uint8_t sak);
int mifareClassicSectorCount(CardType type);
int mifareClassicBlockCount(CardType type);

#include "CardInfo.h"

namespace {
char hexNibble(uint8_t value)
{
    value &= 0x0F;
    return value < 10 ? '0' + value : 'A' + (value - 10);
}

int hexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}
}

const char* cardTypeName(CardType type)
{
    switch (type) {
    case CardType::Iso14443A: return "ISO14443A";
    case CardType::MifareClassicMini: return "MFC Mini";
    case CardType::MifareClassic1K: return "MFC 1K";
    case CardType::MifareClassic4K: return "MFC 4K";
    case CardType::MifareUltralight: return "MFU";
    case CardType::Ntag213: return "NTAG213";
    case CardType::Ntag215: return "NTAG215";
    case CardType::Ntag216: return "NTAG216";
    case CardType::Iso15693: return "ISO15693";
    case CardType::EM4100: return "EM4100";
    case CardType::Desfire: return "DESFire";
    case CardType::BankCardUnsupported: return "Bank card";
    default: return "Unknown";
    }
}

const char* cardTypeSlug(CardType type)
{
    switch (type) {
    case CardType::MifareClassicMini: return "mifare_classic_mini";
    case CardType::MifareClassic1K: return "mifare_classic_1k";
    case CardType::MifareClassic4K: return "mifare_classic_4k";
    case CardType::MifareUltralight: return "mifare_ultralight";
    case CardType::Ntag213: return "ntag213";
    case CardType::Ntag215: return "ntag215";
    case CardType::Ntag216: return "ntag216";
    case CardType::Iso15693: return "iso15693";
    case CardType::EM4100: return "em4100";
    case CardType::Desfire: return "desfire";
    case CardType::BankCardUnsupported: return "bank_card_unsupported";
    case CardType::Iso14443A: return "iso14443a";
    default: return "unknown";
    }
}

const char* cardFamilyName(CardFamily family)
{
    switch (family) {
    case CardFamily::Type2: return "Type 2";
    case CardFamily::MifareClassic: return "MIFARE Classic";
    case CardFamily::Desfire: return "DESFire";
    case CardFamily::Iso15693: return "ISO15693";
    case CardFamily::Em4100: return "EM4100";
    default: return "Unknown";
    }
}

const char* readMethodName(ReadMethod method)
{
    switch (method) {
    case ReadMethod::Type2RawCrc: return "type2_raw_crc";
    case ReadMethod::InDataExchange: return "indataexchange";
    case ReadMethod::Fallback: return "fallback";
    default: return "unknown";
    }
}

const char* dumpStatusName(DumpStatus status)
{
    switch (status) {
    case DumpStatus::InfoOnly: return "info_only";
    case DumpStatus::Partial: return "partial";
    case DumpStatus::Full: return "full";
    default: return "empty";
    }
}

CardFamily cardFamilyForType(CardType type)
{
    switch (type) {
    case CardType::MifareUltralight:
    case CardType::Ntag213:
    case CardType::Ntag215:
    case CardType::Ntag216:
        return CardFamily::Type2;
    case CardType::MifareClassicMini:
    case CardType::MifareClassic1K:
    case CardType::MifareClassic4K:
        return CardFamily::MifareClassic;
    case CardType::Desfire:
    case CardType::BankCardUnsupported:
        return CardFamily::Desfire;
    case CardType::Iso15693:
        return CardFamily::Iso15693;
    case CardType::EM4100:
        return CardFamily::Em4100;
    default:
        return CardFamily::Unknown;
    }
}

String bytesToHex(const uint8_t* data, size_t length, bool separator)
{
    String out;
    out.reserve(length * (separator ? 3 : 2));

    for (size_t i = 0; i < length; i++) {
        if (separator && i > 0) {
            out += ':';
        }

        out += hexNibble(data[i] >> 4);
        out += hexNibble(data[i]);
    }

    return out;
}

bool hexToBytes(const String& hex, std::vector<uint8_t>& out)
{
    String cleaned;

    for (size_t i = 0; i < hex.length(); i++) {
        const char c = hex[i];

        if (c == ':' || c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }

        if (hexValue(c) < 0) {
            return false;
        }

        cleaned += c;
    }

    if (cleaned.length() % 2 != 0) {
        return false;
    }

    out.clear();
    out.reserve(cleaned.length() / 2);

    for (size_t i = 0; i < cleaned.length(); i += 2) {
        const int high = hexValue(cleaned[i]);
        const int low = hexValue(cleaned[i + 1]);
        out.push_back((high << 4) | low);
    }

    return true;
}

CardType detectIso14443AType(uint16_t atqa, uint8_t sak)
{
    if ((sak & 0x20) != 0) {
        return CardType::Desfire;
    }

    if (sak == 0x09) {
        return CardType::MifareClassicMini;
    }

    if (sak == 0x08) {
        return CardType::MifareClassic1K;
    }

    if (sak == 0x18) {
        return CardType::MifareClassic4K;
    }

    if (sak == 0x00 && (atqa == 0x0044 || atqa == 0x0004)) {
        return CardType::MifareUltralight;
    }

    return CardType::Iso14443A;
}

int mifareClassicSectorCount(CardType type)
{
    if (type == CardType::MifareClassicMini) {
        return 5;
    }

    if (type == CardType::MifareClassic4K) {
        return 40;
    }

    if (type == CardType::MifareClassic1K) {
        return 16;
    }

    return 0;
}

int mifareClassicBlockCount(CardType type)
{
    if (type == CardType::MifareClassicMini) {
        return 20;
    }

    if (type == CardType::MifareClassic4K) {
        return 256;
    }

    if (type == CardType::MifareClassic1K) {
        return 64;
    }

    return 0;
}

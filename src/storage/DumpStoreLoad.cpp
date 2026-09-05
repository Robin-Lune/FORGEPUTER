// DumpStore - dump loading and metadata JSON parsing.

#include "DumpStore.h"

#include <cctype>
#include <FS.h>
#include <SD.h>

bool DumpStore::load(const String& name, NfcDump& dump)
{
    if (!available_) {
        setError("SD not ready");
        return false;
    }

    File json = SD.open(metadataPath(name), FILE_READ);

    if (!json) {
        setError("Meta missing");
        return false;
    }

    const String body = json.readString();
    json.close();
    File bin = SD.open(dataPath(name), FILE_READ);

    if (!bin) {
        setError("Data missing");
        return false;
    }

    dump = NfcDump();
    dump.name = name;
    dump.card.type = parseType(jsonValue(body, "type"));
    dump.card.family = cardFamilyForType(dump.card.type);
    dump.familyName = jsonValue(body, "family");
    dump.exactType = jsonValue(body, "exact_type");
    if (dump.exactType.length() == 0) {
        dump.exactType = jsonValue(body, "exactType");
    }
    dump.card.exactType = dump.exactType;
    dump.card.uid = jsonValue(body, "uid");
    dump.card.atqa = jsonValue(body, "atqa");
    dump.card.sak = jsonValue(body, "sak");
    dump.status = parseStatus(jsonValue(body, "read_status"));
    dump.unitsTotal = jsonValue(body, "units_total").toInt();
    dump.unitsRead = jsonValue(body, "units_read").toInt();
    dump.storageSize = jsonValue(body, "storage_size").toInt();
    dump.readMethod = jsonValue(body, "read_method");
    if (dump.readMethod.length() == 0) {
        dump.readMethod = jsonValue(body, "readMethod");
    }
    dump.statusSummary = jsonValue(body, "status_summary");
    if (dump.statusSummary.length() == 0) {
        dump.statusSummary = jsonValue(body, "statusSummary");
    }
    dump.cloneSummary = jsonValue(body, "clone_summary");
    if (dump.cloneSummary.length() == 0) {
        dump.cloneSummary = jsonValue(body, "cloneSummary");
    }
    dump.actionCapabilities = jsonValue(body, "actionCapabilities");
    dump.auth0 = jsonValue(body, "auth0");
    dump.access = jsonValue(body, "access");
    dump.lockBytes = jsonValue(body, "lockBytes");
    dump.otpBytes = jsonValue(body, "otpBytes");
    parseMissingUnits(body, dump.missingUnits);
    dump.data.resize(bin.size());

    if (!dump.data.empty()) {
        bin.read(dump.data.data(), dump.data.size());
    }

    bin.close();

    if (dump.unitsTotal == 0) {
        if (dump.card.type == CardType::MifareClassicMini || dump.card.type == CardType::MifareClassic1K || dump.card.type == CardType::MifareClassic4K) {
            dump.unitsTotal = mifareClassicSectorCount(dump.card.type);
        } else if (dump.card.type == CardType::MifareUltralight || dump.card.type == CardType::Ntag213 || dump.card.type == CardType::Ntag215 || dump.card.type == CardType::Ntag216) {
            dump.unitsTotal = dump.data.size() / 4;
        }
    }

    if (dump.familyName.length() == 0) {
        dump.familyName = cardFamilyName(dump.card.family);
    }

    if (dump.exactType.length() == 0) {
        dump.exactType = cardTypeName(dump.card.type);
        dump.card.exactType = dump.exactType;
    }

    if (dump.storageSize == 0 && dump.unitsTotal > 0) {
        dump.storageSize = dump.card.family == CardFamily::Type2 ? dump.unitsTotal * 4 : dump.data.size();
    }

    if (dump.statusSummary.length() == 0) {
        dump.statusSummary = dump.status == DumpStatus::Full ? "Readable" : (dump.status == DumpStatus::Partial ? "Partial read" : "Info only");
    }

    if (dump.cloneSummary.length() == 0) {
        dump.cloneSummary = dump.status == DumpStatus::Full ? "Full dump" : "Partial only";
    }

    if (dump.unitsRead == 0 && dump.unitsTotal > 0 && dump.status == DumpStatus::Full) {
        dump.unitsRead = dump.unitsTotal;
    } else if (dump.unitsRead == 0 && dump.unitsTotal > 0 && !dump.missingUnits.empty()) {
        dump.unitsRead = dump.unitsTotal - dump.missingUnits.size();
    }

    dump.card.canEmulate = dump.card.type == CardType::MifareClassic1K || dump.card.type == CardType::MifareUltralight || dump.card.type == CardType::Ntag213 || dump.card.type == CardType::Ntag215 || dump.card.type == CardType::Ntag216;
    return true;
}

CardType DumpStore::parseType(const String& value) const
{
    if (value == "mifare_classic_mini") return CardType::MifareClassicMini;
    if (value == "mifare_classic_1k") return CardType::MifareClassic1K;
    if (value == "mifare_classic_4k") return CardType::MifareClassic4K;
    if (value == "mifare_ultralight") return CardType::MifareUltralight;
    if (value == "ntag213") return CardType::Ntag213;
    if (value == "ntag215") return CardType::Ntag215;
    if (value == "ntag216") return CardType::Ntag216;
    if (value == "iso15693") return CardType::Iso15693;
    if (value == "em4100") return CardType::EM4100;
    if (value == "desfire") return CardType::Desfire;
    if (value == "bank_card_unsupported") return CardType::BankCardUnsupported;
    if (value == "iso14443a") return CardType::Iso14443A;
    return CardType::Unknown;
}

DumpStatus DumpStore::parseStatus(const String& value) const
{
    if (value == "info_only") return DumpStatus::InfoOnly;
    if (value == "partial") return DumpStatus::Partial;
    if (value == "full") return DumpStatus::Full;
    return DumpStatus::Empty;
}

String DumpStore::jsonValue(const String& json, const String& key) const
{
    const String marker = "\"" + key + "\"";
    int start = json.indexOf(marker);

    if (start < 0) {
        return "";
    }

    start = json.indexOf(':', start);

    if (start < 0) {
        return "";
    }

    start++;

    while (start < static_cast<int>(json.length()) && isspace(static_cast<unsigned char>(json[start]))) {
        start++;
    }

    if (start >= static_cast<int>(json.length())) {
        return "";
    }

    if (json[start] == '"') {
        const int end = json.indexOf('"', start + 1);
        return end > start ? json.substring(start + 1, end) : "";
    }

    int end = start;

    while (end < static_cast<int>(json.length()) && json[end] != ',' && json[end] != '\n' && json[end] != '}') {
        end++;
    }

    String value = json.substring(start, end);
    value.trim();
    return value;
}

void DumpStore::parseMissingUnits(const String& json, std::vector<uint8_t>& out) const
{
    out.clear();
    const String marker = "\"missing_units\"";
    int start = json.indexOf(marker);

    if (start < 0) {
        return;
    }

    start = json.indexOf('[', start);
    const int end = json.indexOf(']', start);

    if (start < 0 || end < 0 || end <= start) {
        return;
    }

    String token;

    for (int i = start + 1; i < end; i++) {
        const char c = json[i];

        if (c >= '0' && c <= '9') {
            token += c;
            continue;
        }

        if (token.length() > 0) {
            out.push_back(static_cast<uint8_t>(token.toInt()));
            token = "";
        }
    }

    if (token.length() > 0) {
        out.push_back(static_cast<uint8_t>(token.toInt()));
    }
}

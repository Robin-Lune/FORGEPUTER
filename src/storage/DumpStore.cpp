#include "DumpStore.h"

#include <cctype>
#include <FS.h>
#include <SD.h>

namespace {
constexpr const char* nfcDir = "/forgeputer/nfc";
constexpr const char* dumpsDir = "/forgeputer/nfc/dumps";
}

bool DumpStore::begin()
{
    available_ = SD.cardType() != CARD_NONE;

    if (!available_) {
        status_ = "No SD";
        return false;
    }

    available_ = ensureDirs();
    status_ = available_ ? "SD ready" : "SD error";
    return available_;
}

bool DumpStore::available() const
{
    return available_;
}

bool DumpStore::save(const NfcDump& dump, String& savedName)
{
    return saveAs(dump, makeBaseName(dump), savedName);
}

bool DumpStore::saveAs(const NfcDump& dump, const String& requestedName, String& savedName)
{
    if (!available_) {
        setError("SD not ready");
        return false;
    }

    String base = sanitizeName(requestedName);

    if (base.length() == 0) {
        base = makeBaseName(dump);
    }

    savedName = uniqueName(base);
    const String binPath = dataPath(savedName);
    const String jsonPath = metadataPath(savedName);
    File bin = SD.open(binPath, FILE_WRITE);

    if (!bin) {
        setError("Dump write failed");
        return false;
    }

    if (!dump.data.empty()) {
        bin.write(dump.data.data(), dump.data.size());
    }

    bin.close();

    File json = SD.open(jsonPath, FILE_WRITE);

    if (!json) {
        setError("Meta write failed");
        return false;
    }

    json.println("{");
    json.println("  \"format\": \"forgeputer_nfc_dump_v1\",");
    json.print("  \"name\": \""); json.print(savedName); json.println("\",");
    json.print("  \"type\": \""); json.print(cardTypeSlug(dump.card.type)); json.println("\",");
    json.print("  \"uid\": \""); json.print(dump.card.uid); json.println("\",");
    json.print("  \"atqa\": \""); json.print(dump.card.atqa); json.println("\",");
    json.print("  \"sak\": \""); json.print(dump.card.sak); json.println("\",");
    json.print("  \"read_status\": \""); json.print(dumpStatusName(dump.status)); json.println("\",");
    json.print("  \"units_total\": "); json.print(dump.unitsTotal); json.println(",");
    json.print("  \"units_read\": "); json.print(dump.unitsRead); json.println(",");
    json.print("  \"missing_units\": [");

    for (size_t i = 0; i < dump.missingUnits.size(); i++) {
        if (i > 0) {
            json.print(", ");
        }

        json.print(dump.missingUnits[i]);
    }

    json.println("],");
    json.print("  \"can_emulate\": "); json.print(dump.card.canEmulate && dump.status == DumpStatus::Full ? "true" : "false"); json.println(",");
    json.println("  \"created_by\": \"Forgeputer\",");
    json.println("  \"transport\": \"uart\"");
    json.println("}");
    json.close();
    return true;
}

bool DumpStore::list(std::vector<DumpEntry>& entries)
{
    entries.clear();

    if (!available_) {
        setError("SD not ready");
        return false;
    }

    File dir = SD.open(dumpsDir);

    if (!dir) {
        setError("Dump dir failed");
        return false;
    }

    File file = dir.openNextFile();

    while (file) {
        const String path = file.path();

        if (!file.isDirectory() && path.endsWith(".json")) {
            String json = file.readString();
            DumpEntry entry;
            entry.name = jsonValue(json, "name");
            entry.path = path;
            entry.uid = jsonValue(json, "uid");
            entry.type = parseType(jsonValue(json, "type"));
            entry.status = parseStatus(jsonValue(json, "read_status"));

            if (entry.name.length() > 0) {
                entries.push_back(entry);
            }
        }

        file.close();
        file = dir.openNextFile();
    }

    dir.close();
    return true;
}

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
    dump.card.uid = jsonValue(body, "uid");
    dump.card.atqa = jsonValue(body, "atqa");
    dump.card.sak = jsonValue(body, "sak");
    dump.status = parseStatus(jsonValue(body, "read_status"));
    dump.unitsTotal = jsonValue(body, "units_total").toInt();
    dump.unitsRead = jsonValue(body, "units_read").toInt();
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

    if (dump.unitsRead == 0 && dump.unitsTotal > 0 && dump.status == DumpStatus::Full) {
        dump.unitsRead = dump.unitsTotal;
    } else if (dump.unitsRead == 0 && dump.unitsTotal > 0 && !dump.missingUnits.empty()) {
        dump.unitsRead = dump.unitsTotal - dump.missingUnits.size();
    }

    dump.card.canEmulate = dump.card.type == CardType::MifareClassic1K || dump.card.type == CardType::MifareUltralight || dump.card.type == CardType::Ntag213 || dump.card.type == CardType::Ntag215 || dump.card.type == CardType::Ntag216;
    return true;
}

bool DumpStore::remove(const String& name)
{
    if (!available_) {
        setError("SD not ready");
        return false;
    }

    SD.remove(metadataPath(name));
    SD.remove(dataPath(name));
    return true;
}

String DumpStore::status() const
{
    return status_;
}

String DumpStore::lastError() const
{
    return lastError_;
}

String DumpStore::makeBaseName(const NfcDump& dump) const
{
    String uid = dump.card.uid;
    uid.replace(":", "");
    uid.toLowerCase();
    return String(cardTypeSlug(dump.card.type)) + "_" + (uid.length() > 0 ? uid : "unknown");
}

String DumpStore::sanitizeName(const String& requestedName) const
{
    String name = requestedName;
    name.trim();
    name.toLowerCase();
    String out;

    for (size_t i = 0; i < name.length(); i++) {
        const char c = name[i];

        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            out += c;
        } else if (c == ' ' || c == '.' || c == ':') {
            out += '_';
        }
    }

    while (out.indexOf("__") >= 0) {
        out.replace("__", "_");
    }

    return out;
}

String DumpStore::uniqueName(const String& base) const
{
    String candidate = base;
    int suffix = 1;

    while (SD.exists(metadataPath(candidate)) || SD.exists(dataPath(candidate))) {
        candidate = base + "_" + String(suffix);
        suffix++;
    }

    return candidate;
}

String DumpStore::metadataPath(const String& name) const
{
    return String(dumpsDir) + "/" + name + ".json";
}

String DumpStore::dataPath(const String& name) const
{
    return String(dumpsDir) + "/" + name + ".bin";
}

bool DumpStore::ensureDirs()
{
    SD.mkdir("/forgeputer");
    SD.mkdir(nfcDir);
    SD.mkdir(dumpsDir);
    return SD.exists(dumpsDir);
}

void DumpStore::setError(const String& error)
{
    lastError_ = error;
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

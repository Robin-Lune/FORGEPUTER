// DumpStore - SD layout, dump saving, listing and naming.
// Loading and metadata parsing live in DumpStoreLoad.cpp.

#include "DumpStore.h"

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
    json.print("  \"family\": \""); json.print(dump.familyName); json.println("\",");
    json.print("  \"exact_type\": \""); json.print(dump.exactType); json.println("\",");
    json.print("  \"exactType\": \""); json.print(dump.exactType); json.println("\",");
    json.print("  \"uid\": \""); json.print(dump.card.uid); json.println("\",");
    json.print("  \"atqa\": \""); json.print(dump.card.atqa); json.println("\",");
    json.print("  \"sak\": \""); json.print(dump.card.sak); json.println("\",");
    json.print("  \"read_status\": \""); json.print(dumpStatusName(dump.status)); json.println("\",");
    json.print("  \"units_total\": "); json.print(dump.unitsTotal); json.println(",");
    json.print("  \"units_read\": "); json.print(dump.unitsRead); json.println(",");
    json.print("  \"pagesTotal\": "); json.print(dump.pagesTotal); json.println(",");
    json.print("  \"pagesRead\": "); json.print(dump.pagesRead); json.println(",");
    json.print("  \"pagesUnknown\": "); json.print(dump.pagesUnknown); json.println(",");
    json.print("  \"pagesProtected\": "); json.print(dump.pagesProtected); json.println(",");
    json.print("  \"storage_size\": "); json.print(dump.storageSize); json.println(",");
    json.print("  \"read_method\": \""); json.print(dump.readMethod); json.println("\",");
    json.print("  \"readMethod\": \""); json.print(dump.readMethod); json.println("\",");
    json.print("  \"status_summary\": \""); json.print(dump.statusSummary); json.println("\",");
    json.print("  \"statusSummary\": \""); json.print(dump.statusSummary); json.println("\",");
    json.print("  \"clone_summary\": \""); json.print(dump.cloneSummary); json.println("\",");
    json.print("  \"cloneSummary\": \""); json.print(dump.cloneSummary); json.println("\",");
    json.print("  \"actionCapabilities\": \""); json.print(dump.actionCapabilities); json.println("\",");
    json.print("  \"auth0\": \""); json.print(dump.auth0); json.println("\",");
    json.print("  \"access\": \""); json.print(dump.access); json.println("\",");
    json.print("  \"lockBytes\": \""); json.print(dump.lockBytes); json.println("\",");
    json.print("  \"otpBytes\": \""); json.print(dump.otpBytes); json.println("\",");
    json.print("  \"readableRanges\": [");

    for (size_t i = 0; i < dump.readableRanges.size(); i++) {
        if (i > 0) json.print(", ");
        json.print("\""); json.print(dump.readableRanges[i]); json.print("\"");
    }

    json.println("],");
    json.print("  \"protectedRanges\": [");

    for (size_t i = 0; i < dump.protectedRanges.size(); i++) {
        if (i > 0) json.print(", ");
        json.print("\""); json.print(dump.protectedRanges[i]); json.print("\"");
    }

    json.println("],");
    json.print("  \"unknownRanges\": [");

    for (size_t i = 0; i < dump.unknownRanges.size(); i++) {
        if (i > 0) json.print(", ");
        json.print("\""); json.print(dump.unknownRanges[i]); json.print("\"");
    }

    json.println("],");
    json.print("  \"pages\": [");

    if (dump.card.family == CardFamily::Type2 || dump.familyName == "Type 2") {
        const int pageCount = dump.data.size() / 4;

        for (int page = 0; page < pageCount; page++) {
            if (page > 0) json.print(", ");
            json.print("{\"index\": "); json.print(page); json.print(", \"data\": \"");
            json.print(bytesToHex(&dump.data[page * 4], 4, true));
            json.print("\"}");
        }
    }

    json.println("],");
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

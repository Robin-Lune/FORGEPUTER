#pragma once

#include "../nfc/CardInfo.h"

#include <Arduino.h>
#include <vector>

struct DumpEntry {
    String name;
    String path;
    CardType type = CardType::Unknown;
    DumpStatus status = DumpStatus::Empty;
    String uid;
};

// Implementation split across DumpStore*.cpp:
//   DumpStore.cpp      SD layout, save, list, naming
//   DumpStoreLoad.cpp  load and metadata JSON parsing
class DumpStore {
public:
    bool begin();
    bool available() const;
    bool save(const NfcDump& dump, String& savedName);
    bool saveAs(const NfcDump& dump, const String& requestedName, String& savedName);
    bool list(std::vector<DumpEntry>& entries);
    bool load(const String& name, NfcDump& dump);
    bool remove(const String& name);
    String status() const;
    String lastError() const;

private:
    bool available_ = false;
    String status_ = "SD not ready";
    String lastError_;

    String makeBaseName(const NfcDump& dump) const;
    String sanitizeName(const String& requestedName) const;
    String uniqueName(const String& base) const;
    String metadataPath(const String& name) const;
    String dataPath(const String& name) const;
    bool ensureDirs();
    void setError(const String& error);
    CardType parseType(const String& value) const;
    DumpStatus parseStatus(const String& value) const;
    String jsonValue(const String& json, const String& key) const;
    void parseMissingUnits(const String& json, std::vector<uint8_t>& out) const;
};

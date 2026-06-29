#include "KeyStore.h"

#include "../nfc/CardInfo.h"
#include "SystemMfcKeys.h"

#include <FS.h>
#include <SD.h>
#include <array>
#include <cstring>

namespace {
constexpr const char* keyDir = "/forgeputer/nfc/keys";
constexpr const char* userKeys = "/forgeputer/nfc/keys/user_mfc.keys";
constexpr const char* recoveredKeys = "/forgeputer/nfc/keys/recovered_mfc.keys";
constexpr const char* systemKeys = "/forgeputer/nfc/keys/system_mfc.keys";
}

bool KeyStore::begin()
{
    keys_.clear();
    addDefaultKeys();

    if (SD.cardType() == CARD_NONE) {
        status_ = "Defaults only";
        return true;
    }

    ensureKeyFiles();
    loadFile(systemKeys);
    loadFile(userKeys);
    loadFile(recoveredKeys);
    status_ = String(keys_.size()) + " keys";
    return true;
}

const std::vector<std::array<uint8_t, 6>>& KeyStore::keys() const
{
    return keys_;
}

String KeyStore::status() const
{
    return status_;
}

void KeyStore::addDefaultKeys()
{
    for (int i = 0; i < systemMfcKeyCount; i++) {
        addKeyLine(systemMfcKeys[i]);
    }
}

void KeyStore::loadFile(const char* path)
{
    File file = SD.open(path, FILE_READ);

    if (!file) {
        return;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
        addKeyLine(line);
    }

    file.close();
}

bool KeyStore::addKeyLine(const String& rawLine)
{
    String line = rawLine;
    line.trim();

    if (line.length() == 0 || line[0] == '#') {
        return false;
    }

    std::vector<uint8_t> bytes;

    if (!hexToBytes(line, bytes) || bytes.size() != 6) {
        return false;
    }

    std::array<uint8_t, 6> key;
    memcpy(key.data(), bytes.data(), 6);

    for (const auto& existing : keys_) {
        if (memcmp(existing.data(), key.data(), 6) == 0) {
            return false;
        }
    }

    keys_.push_back(key);
    return true;
}

void KeyStore::ensureKeyFiles()
{
    SD.mkdir("/forgeputer");
    SD.mkdir("/forgeputer/nfc");
    SD.mkdir(keyDir);

    if (!SD.exists(systemKeys) || shouldRefreshSystemKeys()) {
        writeSystemKeys();
    }

    if (!SD.exists(userKeys)) {
        File file = SD.open(userKeys, FILE_WRITE);

        if (file) {
            file.println("# Add one 6-byte hex key per line");
            file.close();
        }
    }

    if (!SD.exists(recoveredKeys)) {
        File file = SD.open(recoveredKeys, FILE_WRITE);

        if (file) {
            file.println("# Lab recovered keys");
            file.close();
        }
    }
}

bool KeyStore::shouldRefreshSystemKeys()
{
    File file = SD.open(systemKeys, FILE_READ);

    if (!file) {
        return true;
    }

    int count = 0;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();

        if (line.length() == 12 && line[0] != '#') {
            count++;
        }
    }

    file.close();
    return count < 20;
}

void KeyStore::writeSystemKeys()
{
    SD.remove(systemKeys);
    File file = SD.open(systemKeys, FILE_WRITE);

    if (!file) {
        return;
    }

    file.println("# Forgeputer MIFARE Classic system keys");
    file.println("# Curated from common MIFARE Classic defaults and Flipper-style dictionaries.");

    for (int i = 0; i < systemMfcKeyCount; i++) {
        file.println(systemMfcKeys[i]);
    }

    file.close();
}

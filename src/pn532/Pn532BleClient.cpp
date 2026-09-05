// Pn532BleClient - BLE link lifecycle, scan, read dispatch and retry.
// Readers, NTAG read, emulation and raw diagnostics live in the
// Pn532BleClient*.cpp siblings listed in Pn532BleClient.h.

#include "Pn532BleClient.h"

#include <cstring>

Pn532BleClient::Pn532BleClient(KeyStore& keys)
    : keys_(keys)
{
}

bool Pn532BleClient::begin()
{
    disconnect();
    ble_.reset(new PN532_BLE(false));
    connected_ = false;
    killer_ = false;
    deviceName_ = "";

    if (!ble_->searchForDevice()) {
        setError("BLE PN532 not found");
        return false;
    }

    deviceName_ = String(ble_->getName().c_str());

    if (!ble_->connectToDevice()) {
        setError("BLE connect failed");
        return false;
    }

    connected_ = ble_->isConnected();
    killer_ = ble_->isPN532Killer();

    if (!connected_) {
        setError("BLE services failed");
        return false;
    }

    if (!ble_->setNormalMode()) {
        setError("BLE wake failed");
        connected_ = false;
        return false;
    }

    return true;
}

void Pn532BleClient::disconnect()
{
    emulating_ = false;
    connected_ = false;
    killer_ = false;
    deviceName_ = "";

    if (ble_) {
        ble_.reset();
    }
}

bool Pn532BleClient::isConnected() const
{
    return connected_;
}

bool Pn532BleClient::isKiller() const
{
    return killer_;
}

bool Pn532BleClient::scan(CardInfo& info)
{
    if (!connected_) {
        setError("BLE not connected");
        return false;
    }

    ble_->setNormalMode();
    PN532_BLE::Iso14aTagInfo tag = ble_->hf14aScan();

    if (tag.uid.empty() || tag.atqa.size() < 2) {
        PN532_BLE::Iso15TagInfo iso15 = ble_->hf15Scan();

        if (iso15.uid.empty()) {
            setError("No tag found");
            return false;
        }

        info.type = CardType::Iso15693;
        info.family = cardFamilyForType(info.type);
        info.uid = bytesToHex(iso15.uid.data(), iso15.uid.size(), true);
        info.atqa = "";
        info.sak = "";
        info.memoryReadable = true;
        info.canWrite = true;
        info.canEmulate = killer_;
        info.isPartial = false;
        return true;
    }

    info.type = detectIso14443AType((static_cast<uint16_t>(tag.atqa[0]) << 8) | tag.atqa[1], tag.sak);
    info.family = cardFamilyForType(info.type);
    info.uid = bytesToHex(tag.uid.data(), tag.uid.size(), true);
    info.atqa = tag.atqa_hex;
    info.sak = tag.sak_hex;
    info.memoryReadable = isMifareClassic(info.type) || info.type == CardType::MifareUltralight;
    info.canWrite = info.memoryReadable;
    info.canEmulate = info.type == CardType::MifareClassic1K || info.type == CardType::MifareUltralight;
    info.isPartial = false;
    return true;
}

bool Pn532BleClient::readAuto(NfcDump& dump)
{
    if (!connected_) {
        setError("BLE not connected");
        return false;
    }

    ble_->setNormalMode();
    PN532_BLE::Iso14aTagInfo tag = ble_->hf14aScan();

    if (tag.uid.empty() || tag.atqa.size() < 2) {
        PN532_BLE::Iso15TagInfo iso15 = ble_->hf15Scan();

        if (iso15.uid.empty()) {
            setError("No tag found");
            return false;
        }

        return readIso15693(iso15, dump);
    }

    CardInfo info;
    info.type = detectIso14443AType((static_cast<uint16_t>(tag.atqa[0]) << 8) | tag.atqa[1], tag.sak);
    info.family = cardFamilyForType(info.type);
    info.uid = bytesToHex(tag.uid.data(), tag.uid.size(), true);
    info.atqa = tag.atqa_hex;
    info.sak = tag.sak_hex;
    info.memoryReadable = isMifareClassic(info.type) || info.type == CardType::MifareUltralight;
    info.canWrite = info.memoryReadable;
    info.canEmulate = info.type == CardType::MifareClassic1K || info.type == CardType::MifareUltralight;

    if (isMifareClassic(info.type)) {
        return readMifareClassic(tag, info, dump);
    }

    if (info.type == CardType::MifareUltralight) {
        return readNtag(tag, info, dump);
    }

    dump = NfcDump();
    dump.card = info;
    dump.status = DumpStatus::InfoOnly;
    return true;
}

bool Pn532BleClient::retryMissing(NfcDump& dump)
{
    if (!connected_) {
        setError("BLE not connected");
        return false;
    }

    if (!isMifareClassic(dump.card.type) || dump.missingUnits.empty()) {
        setError("No MFC missing sectors");
        return false;
    }

    ble_->setNormalMode();
    PN532_BLE::Iso14aTagInfo tag = ble_->hf14aScan();

    if (tag.uid.empty() || bytesToHex(tag.uid.data(), tag.uid.size(), true) != dump.card.uid) {
        setError("Different card");
        return false;
    }

    if (dump.data.empty()) {
        dump.data.assign(mifareClassicBlockCount(dump.card.type) * 16, 0);
    }

    std::vector<uint8_t> stillMissing;
    const int totalMissing = dump.missingUnits.size();
    emitProgress("Retry MFC", "Missing sectors", 0, totalMissing, false, true);

    for (int i = 0; i < totalMissing; i++) {
        const uint8_t sector = dump.missingUnits[i];
        const uint8_t firstBlock = firstBlockForSector(sector);
        bool authenticated = false;
        emitProgress("Retry MFC", "Sector " + String(sector) + " auth", i, totalMissing, false, true);

        for (const auto& key : keys_.keys()) {
            uint8_t keyBytes[6];
            memcpy(keyBytes, key.data(), 6);

            if (ble_->mfAuth(tag.uid, firstBlock, keyBytes, true) || ble_->mfAuth(tag.uid, firstBlock, keyBytes, false)) {
                authenticated = true;
                break;
            }
        }

        if (!authenticated) {
            stillMissing.push_back(sector);
            emitProgress("Retry MFC", "Sector " + String(sector) + " LOCKED", i + 1, totalMissing, true, false);
            continue;
        }

        bool sectorRead = true;
        const uint8_t blocks = blockCountForSector(sector);

        for (uint8_t offset = 0; offset < blocks; offset++) {
            const uint8_t block = firstBlock + offset;
            std::vector<uint8_t> blockData = ble_->mfRdbl(block);

            if (blockData.size() < 17 || blockData[0] != 0x00) {
                sectorRead = false;
                break;
            }

            memcpy(&dump.data[block * 16], &blockData[1], 16);
        }

        if (sectorRead) {
            dump.unitsRead++;
            emitProgress("Retry MFC", "Sector " + String(sector) + " OK", i + 1, totalMissing, true, true);
        } else {
            stillMissing.push_back(sector);
            emitProgress("Retry MFC", "Sector " + String(sector) + " READ ERR", i + 1, totalMissing, true, false);
        }
    }

    dump.missingUnits = stillMissing;
    dump.card.isPartial = !dump.missingUnits.empty();
    dump.status = dump.missingUnits.empty() ? DumpStatus::Full : DumpStatus::Partial;
    return true;
}

String Pn532BleClient::deviceName() const
{
    return deviceName_;
}

String Pn532BleClient::lastError() const
{
    return lastError_;
}

void Pn532BleClient::setProgressCallback(void* context, NfcReadProgressCallback callback)
{
    progressContext_ = context;
    progressCallback_ = callback;
}

uint8_t Pn532BleClient::firstBlockForSector(int sector) const
{
    if (sector < 32) {
        return sector * 4;
    }

    return 128 + ((sector - 32) * 16);
}

uint8_t Pn532BleClient::blockCountForSector(int sector) const
{
    return sector < 32 ? 4 : 16;
}

bool Pn532BleClient::isMifareClassic(CardType type) const
{
    return type == CardType::MifareClassicMini || type == CardType::MifareClassic1K || type == CardType::MifareClassic4K;
}

bool Pn532BleClient::isNtag(CardType type) const
{
    return type == CardType::MifareUltralight || type == CardType::Ntag213 || type == CardType::Ntag215 || type == CardType::Ntag216;
}

void Pn532BleClient::emitProgress(const String& title, const String& detail, int current, int total, bool done, bool ok)
{
    if (progressCallback_ == nullptr) {
        return;
    }

    NfcReadProgress progress;
    progress.title = title;
    progress.detail = detail;
    progress.current = current;
    progress.total = total;
    progress.done = done;
    progress.ok = ok;
    progressCallback_(progressContext_, progress);
}

void Pn532BleClient::setError(const String& error)
{
    lastError_ = error;
}

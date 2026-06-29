#include "Pn532BleClient.h"

#include <cstring>

namespace {
uint8_t diagChecksum(uint8_t value)
{
    return static_cast<uint8_t>(~value + 1);
}

uint8_t diagDcs(const uint8_t* data, size_t length)
{
    uint8_t sum = 0;

    for (size_t i = 0; i < length; i++) {
        sum += data[i];
    }

    return diagChecksum(sum);
}

const char* diagStatusName(uint8_t status)
{
    switch (status) {
    case 0x00: return "OK";
    case 0x01: return "Timeout";
    case 0x02: return "CRC error";
    case 0x03: return "Parity error";
    case 0x04: return "Erroneous bit count";
    case 0x05: return "Framing error";
    case 0x06: return "Bit collision";
    case 0x07: return "Small buffer";
    case 0x09: return "RF buffer overflow";
    case 0x0A: return "RF field not activated";
    case 0x0B: return "Protocol error";
    case 0x10: return "Invalid parameter";
    case 0x14: return "MIFARE authentication/protocol error";
    case 0x25: return "Invalid target number";
    case 0x27: return "Card disappeared";
    default: return "Unknown status";
    }
}

uint16_t diagCrcA(const uint8_t* data, size_t length)
{
    // ISO14443A CRC_A is appended little-endian: READ 04 = 30 04 26 EE.
    uint16_t crc = 0x6363;

    for (size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        byte ^= static_cast<uint8_t>(crc & 0x00FF);
        byte ^= byte << 4;
        crc = (crc >> 8) ^ (static_cast<uint16_t>(byte) << 8) ^ (static_cast<uint16_t>(byte) << 3) ^ (byte >> 4);
    }

    return crc;
}
}

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

bool Pn532BleClient::startNdefEmulation(const NfcDump& dump)
{
    std::vector<uint8_t> ndef;

    if (!extractType2Ndef(dump, ndef)) {
        setError("No NDEF in dump");
        return false;
    }

    return startNdefEmulation(ndef, dump.card);
}

bool Pn532BleClient::startNdefEmulation(const std::vector<uint8_t>& ndefRecord, const CardInfo& card)
{
    if (!connected_) {
        setError("BLE not connected");
        return false;
    }

    if (ndefRecord.empty()) {
        setError("Empty NDEF");
        return false;
    }

    if (ndefRecord.size() > 0xFFFE) {
        setError("NDEF too large");
        return false;
    }

    emulatedNdefFile_.clear();
    emulatedNdefFile_.push_back((ndefRecord.size() >> 8) & 0xFF);
    emulatedNdefFile_.push_back(ndefRecord.size() & 0xFF);
    emulatedNdefFile_.insert(emulatedNdefFile_.end(), ndefRecord.begin(), ndefRecord.end());

    const uint16_t ndefFileSize = static_cast<uint16_t>(emulatedNdefFile_.size());
    const uint16_t maxNdefSize = ndefFileSize > 0x00FF ? ndefFileSize : 0x00FF;
    emulatedCcFile_ = {
        0x00, 0x0F,
        0x20,
        0x00, 0xFF,
        0x00, 0xFF,
        0x04, 0x06,
        0xE1, 0x04,
        static_cast<uint8_t>((maxNdefSize >> 8) & 0xFF),
        static_cast<uint8_t>(maxNdefSize & 0xFF),
        0x00,
        0xFF,
    };

    selectedEmuFile_ = 0;
    emulationExchangeCount_ = 0;
    ble_->setNormalMode();

    NfcDump targetDump;
    targetDump.card = card;
    std::vector<uint8_t> initResponse = ble_->tgInitAsTarget(buildTargetParameters(targetDump));

    if (initResponse.empty()) {
        setError("Emu init timeout");
        return false;
    }

    emulating_ = true;
    return true;
}

void Pn532BleClient::stopNdefEmulation()
{
    emulating_ = false;
    selectedEmuFile_ = 0;

    if (connected_) {
        ble_->inRelease();
        ble_->setNormalMode();
    }
}

bool Pn532BleClient::tickNdefEmulation()
{
    if (!connected_ || !emulating_) {
        return false;
    }

    std::vector<uint8_t> apdu = ble_->getData();

    if (apdu.empty()) {
        ble_->inRelease();
        emulating_ = false;
        return false;
    }

    if (apdu[0] == 0x29 || apdu[0] == 0x25) {
        emulating_ = false;
        return false;
    }

    if (apdu.size() > 2 && apdu[0] == 0x00 && apdu[1] == 0x00) {
        apdu.erase(apdu.begin());
    }

    emulationExchangeCount_++;
    return respondToApdu(apdu);
}

bool Pn532BleClient::isEmulating() const
{
    return emulating_;
}

int Pn532BleClient::emulationExchangeCount() const
{
    return emulationExchangeCount_;
}

String Pn532BleClient::deviceName() const
{
    return deviceName_;
}

String Pn532BleClient::lastError() const
{
    return lastError_;
}

Pn532RawDiagnostic Pn532BleClient::diagnoseRawCommand(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* label, bool hasStatusByte, uint16_t timeoutMs)
{
    Pn532RawDiagnostic diag;
    const uint32_t startedAt = millis();

    if (!connected_ || !ble_ || ble_->chrWrite == nullptr) {
        diag.error = "BLE not connected";
        return diag;
    }

    if (command.empty() || command.size() > 253) {
        diag.error = "Bad command";
        return diag;
    }

    std::vector<uint8_t> payload = {ble_->DATA_TIF_SEND};
    payload.insert(payload.end(), command.begin(), command.end());
    const uint8_t length = payload.size();
    diag.txFrame = {ble_->DATA_PREAMBLE, ble_->DATA_START_CODE[0], ble_->DATA_START_CODE[1], length, diagChecksum(length)};
    diag.txFrame.insert(diag.txFrame.end(), payload.begin(), payload.end());
    diag.txFrame.push_back(diagDcs(payload.data(), payload.size()));
    diag.txFrame.push_back(ble_->DATA_POSTAMBLE);

    ble_->pn532Responses.clear();
    ble_->pn532bleBuffer.clear();
    const bool wrote = ble_->chrWrite->writeValue(diag.txFrame.data(), diag.txFrame.size(), true);

    if (!wrote) {
        diag.error = "BLE write failed";
        diag.elapsedMs = millis() - startedAt;
        return diag;
    }

    diag.ackOk = true;

    while (millis() - startedAt < timeoutMs) {
        for (const auto& response : ble_->pn532Responses) {
            if (response.command != command[0]) {
                continue;
            }

            diag.rxFrame.assign(response.raw, response.raw + response.length);

            if (response.length > 12) {
                const uint8_t frameLength = response.raw[9];
                const size_t start = 11;
                const size_t available = response.length > start ? response.length - start : 0;
                const size_t copyLength = min(static_cast<size_t>(frameLength), available);
                diag.framePayload.assign(response.raw + start, response.raw + start + copyLength);
            }

            diag.frameOk = !diag.framePayload.empty();
            diag.responseCodeOk = diag.framePayload.size() >= 2 && diag.framePayload[0] == ble_->DATA_TIF_RECEIVE && diag.framePayload[1] == expectedResponseCode;

            if (diag.responseCodeOk && diag.framePayload.size() > 2) {
                diag.pn532Payload.assign(diag.framePayload.begin() + 2, diag.framePayload.end());
            }

            diag.hasStatus = hasStatusByte && !diag.pn532Payload.empty();

            if (diag.hasStatus) {
                diag.inDataExchangeStatus = diag.pn532Payload[0];

                if (diag.pn532Payload.size() > 1) {
                    diag.tagPayload.assign(diag.pn532Payload.begin() + 1, diag.pn532Payload.end());
                }
            } else {
                diag.tagPayload = diag.pn532Payload;
            }

            if (!diag.responseCodeOk) {
                diag.error = "Unexpected response";
                diag.parsed = diag.error;
            } else if (!diag.hasStatus) {
                diag.parsed = String(label) + " response OK";
            } else if (diag.inDataExchangeStatus == 0x00) {
                diag.parsed = String(label) + " status 00 OK";
            } else {
                diag.parsed = String(label) + " status " + String(diag.inDataExchangeStatus, HEX) + " " + diagStatusName(diag.inDataExchangeStatus);
            }

            diag.elapsedMs = millis() - startedAt;
            ble_->pn532Responses.clear();
            return diag;
        }

        delay(5);
    }

    diag.error = "BLE RX timeout";
    diag.elapsedMs = millis() - startedAt;
    return diag;
}

void Pn532BleClient::setProgressCallback(void* context, NfcReadProgressCallback callback)
{
    progressContext_ = context;
    progressCallback_ = callback;
}

Pn532NfcAResult Pn532BleClient::type2TransceiveRaw(const std::vector<uint8_t>& tagCommand, size_t expectedPayloadMin, uint16_t timeoutMs)
{
    Pn532NfcAResult result;

    if (tagCommand.empty()) {
        result.error = "Empty Type2 command";
        return result;
    }

    std::vector<uint8_t> tagFrame = tagCommand;
    const uint16_t crc = diagCrcA(tagFrame.data(), tagFrame.size());
    tagFrame.push_back(static_cast<uint8_t>(crc & 0x00FF));
    tagFrame.push_back(static_cast<uint8_t>((crc >> 8) & 0x00FF));

    std::vector<uint8_t> command = {0x42};
    command.insert(command.end(), tagFrame.begin(), tagFrame.end());
    Pn532RawDiagnostic diag = diagnoseRawCommand(command, 0x43, "InCommunicateThru", true, timeoutMs);

    if (!diag.ackOk || !diag.frameOk || !diag.responseCodeOk) {
        result.error = diag.error.length() > 0 ? diag.error : "Type2 raw failed";
        return result;
    }

    result.transportOk = true;
    result.status = diag.inDataExchangeStatus;

    if (!diag.hasStatus || diag.inDataExchangeStatus != 0x00) {
        result.payload = diag.pn532Payload;
        result.kind = Pn532NfcAResponseKind::StatusOnly;
        result.error = diag.parsed.length() > 0 ? diag.parsed : "Type2 status failed";
        return result;
    }

    if (diag.tagPayload.size() < 2) {
        result.kind = Pn532NfcAResponseKind::ShortPayload;
        result.error = "Type2 response missing CRC";
        return result;
    }

    result.payload.assign(diag.tagPayload.begin(), diag.tagPayload.end() - 2);
    result.accepted = result.payload.size() >= expectedPayloadMin;
    result.kind = result.accepted ? Pn532NfcAResponseKind::StatusPayload : Pn532NfcAResponseKind::ShortPayload;

    if (!result.accepted) {
        result.error = "Type2 short payload";
    }

    return result;
}

bool Pn532BleClient::readMifareClassic(const PN532_BLE::Iso14aTagInfo& tag, const CardInfo& info, NfcDump& dump)
{
    dump = NfcDump();
    dump.card = info;
    dump.unitsTotal = mifareClassicSectorCount(info.type);
    dump.data.assign(mifareClassicBlockCount(info.type) * 16, 0);
    emitProgress("Reading MFC", info.uid, 0, dump.unitsTotal, false, true);

    for (int sector = 0; sector < dump.unitsTotal; sector++) {
        const uint8_t firstBlock = firstBlockForSector(sector);
        bool authenticated = false;
        emitProgress("Reading MFC", "Sector " + String(sector) + " auth", sector, dump.unitsTotal, false, true);

        for (const auto& key : keys_.keys()) {
            uint8_t keyBytes[6];
            memcpy(keyBytes, key.data(), 6);

            if (ble_->mfAuth(tag.uid, firstBlock, keyBytes, true) || ble_->mfAuth(tag.uid, firstBlock, keyBytes, false)) {
                authenticated = true;
                break;
            }
        }

        if (!authenticated) {
            dump.missingUnits.push_back(sector);
            emitProgress("Reading MFC", "Sector " + String(sector) + " LOCKED", sector + 1, dump.unitsTotal, true, false);
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
            emitProgress("Reading MFC", "Sector " + String(sector) + " OK", sector + 1, dump.unitsTotal, true, true);
        } else {
            dump.missingUnits.push_back(sector);
            emitProgress("Reading MFC", "Sector " + String(sector) + " READ ERR", sector + 1, dump.unitsTotal, true, false);
        }
    }

    dump.card.isPartial = dump.unitsRead < dump.unitsTotal;
    dump.status = dump.unitsRead == dump.unitsTotal ? DumpStatus::Full : DumpStatus::Partial;
    return dump.unitsRead > 0;
}

bool Pn532BleClient::readNtag(const PN532_BLE::Iso14aTagInfo& tag, const CardInfo& info, NfcDump& dump)
{
    dump = NfcDump();
    dump.card = info;
    constexpr int maxPages = 45;
    dump.unitsTotal = maxPages;
    bool rawFastReadWorked = false;
    bool fallbackUsed = false;

    Pn532NfcAResult read04 = type2TransceiveRaw({0x30, 0x04}, 16, 1000);

    if (!read04.accepted) {
        dump.unitsRead = 0;
        dump.unknownUnits = maxPages;
        dump.card.isPartial = true;
        dump.status = DumpStatus::Partial;
        dump.reportLines.push_back("Type2 raw READ: ERR");
        dump.reportLines.push_back("Type2 raw FAST_READ: not run");
        dump.reportLines.push_back("Fallback IDX used: no");
        dump.reportLines.push_back("Pages read: 0/" + String(maxPages));
        dump.reportLines.push_back("Protected pages: 0");
        dump.reportLines.push_back("Unknown pages: " + String(maxPages));
        dump.reportLines.push_back("Type 2 raw read failed");
        setError("Type 2 raw read failed");
        emitProgress("Reading NTAG", "Type 2 raw read failed", 0, maxPages, true, false);
        return false;
    }

    std::vector<uint8_t> pagesData(maxPages * 4, 0);
    std::vector<bool> pageRead(maxPages, false);
    emitProgress("Reading NTAG", info.uid, 0, dump.unitsTotal, false, true);

    for (uint8_t page = 0; page < maxPages; page++) {
        emitProgress("Reading NTAG", "Window " + String(page) + "-" + String(min(static_cast<int>(page + 3), maxPages - 1)), page, maxPages, false, true);
        std::vector<uint8_t> pages;
        const Pn532NfcAResult rawRead = type2TransceiveRaw({0x30, page}, 16, 1000);

        if (rawRead.accepted) {
            pages = rawRead.payload;
        } else {
            const Pn532NfcAResult rawFast = type2TransceiveRaw({0x3A, page, static_cast<uint8_t>(min(static_cast<int>(page + 3), maxPages - 1))}, 16, 1500);

            if (rawFast.accepted) {
                pages = rawFast.payload;
                rawFastReadWorked = true;
            }
        }

        if (pages.empty()) {
            fallbackUsed = true;
            pages = ble_->mfRdbl(page);

            if (pages.size() >= 17 && pages[0] == 0x00) {
                pages.erase(pages.begin());
            }
        }

        if (pages.size() < 16) {
            emitProgress("Reading NTAG", "Window " + String(page) + " ERR", page, maxPages, true, false);
            continue;
        }

        int newPages = 0;

        for (int offset = 0; offset < 4 && page + offset < maxPages; offset++) {
            const int pageIndex = page + offset;

            if (!pageRead[pageIndex]) {
                memcpy(&pagesData[pageIndex * 4], &pages[offset * 4], 4);
                pageRead[pageIndex] = true;
                newPages++;
            }
        }

        dump.unitsRead += newPages;
        emitProgress("Reading NTAG", "Window " + String(page) + " +" + String(newPages), dump.unitsRead, maxPages, true, true);
    }

    int highestReadPage = -1;

    for (int page = 0; page < maxPages; page++) {
        if (pageRead[page]) {
            highestReadPage = page;
        }
    }

    if (highestReadPage >= 0) {
        dump.unitsTotal = highestReadPage + 1;
        dump.data.assign(pagesData.begin(), pagesData.begin() + (dump.unitsTotal * 4));
    } else {
        dump.unitsTotal = maxPages;
    }

    dump.unitsRead = 0;
    dump.missingUnits.clear();

    for (int page = 0; page < dump.unitsTotal; page++) {
        if (pageRead[page]) {
            dump.unitsRead++;
        } else {
            dump.missingUnits.push_back(page);
        }
    }

    dump.card.isPartial = !dump.missingUnits.empty();
    dump.status = dump.unitsRead > 0 && dump.missingUnits.empty() ? DumpStatus::Full : DumpStatus::Partial;
    dump.unknownUnits = dump.missingUnits.size();
    dump.reportLines.push_back("Type2 raw READ: OK");
    dump.reportLines.push_back(String("Type2 raw FAST_READ: ") + (rawFastReadWorked ? "OK" : "ERR/unused"));
    dump.reportLines.push_back(String("Fallback IDX used: ") + (fallbackUsed ? "yes" : "no"));
    dump.reportLines.push_back("Pages read: " + String(dump.unitsRead) + "/" + String(dump.unitsTotal));
    dump.reportLines.push_back("Protected pages: " + String(dump.protectedUnits));
    dump.reportLines.push_back("Unknown pages: " + String(dump.unknownUnits));
    return dump.unitsRead > 0;
}

bool Pn532BleClient::readIso15693(const PN532_BLE::Iso15TagInfo& tag, NfcDump& dump)
{
    dump = NfcDump();
    dump.card.type = CardType::Iso15693;
    dump.card.uid = bytesToHex(tag.uid.data(), tag.uid.size(), true);
    dump.card.memoryReadable = true;
    dump.card.canWrite = true;
    dump.card.canEmulate = killer_;
    dump.unitsTotal = 64;
    dump.data.reserve(256);
    emitProgress("Reading ISO15", dump.card.uid, 0, dump.unitsTotal, false, true);

    for (uint8_t block = 0; block < dump.unitsTotal; block++) {
        emitProgress("Reading ISO15", "Block " + String(block), block, dump.unitsTotal, false, true);
        std::vector<uint8_t> data = ble_->hf15Rdbl(block);

        if (data.size() < 2 || data[0] != 0x00) {
            dump.missingUnits.push_back(block);
            emitProgress("Reading ISO15", "Block " + String(block) + " END/ERR", block + 1, dump.unitsTotal, true, false);

            if (dump.unitsRead > 0) {
                break;
            }

            continue;
        }

        dump.data.insert(dump.data.end(), data.begin() + 1, data.end());
        dump.unitsRead++;
        emitProgress("Reading ISO15", "Block " + String(block) + " OK", block + 1, dump.unitsTotal, true, true);
    }

    if (dump.unitsRead > 0 && dump.unitsRead < dump.unitsTotal) {
        dump.unitsTotal = dump.unitsRead + dump.missingUnits.size();
    }

    dump.card.isPartial = !dump.missingUnits.empty();
    dump.status = dump.unitsRead == 0 ? DumpStatus::InfoOnly : (dump.card.isPartial ? DumpStatus::Partial : DumpStatus::Full);
    return true;
}

bool Pn532BleClient::extractType2Ndef(const NfcDump& dump, std::vector<uint8_t>& ndef) const
{
    ndef.clear();

    if (!isNtag(dump.card.type) || dump.data.size() <= 16) {
        return false;
    }

    size_t offset = 16;

    while (offset < dump.data.size()) {
        const uint8_t type = dump.data[offset++];

        if (type == 0x00) {
            continue;
        }

        if (type == 0xFE || offset >= dump.data.size()) {
            return false;
        }

        size_t length = dump.data[offset++];

        if (length == 0xFF) {
            if (offset + 1 >= dump.data.size()) {
                return false;
            }

            length = (static_cast<size_t>(dump.data[offset]) << 8) | dump.data[offset + 1];
            offset += 2;
        }

        if (offset + length > dump.data.size()) {
            return false;
        }

        if (type == 0x03) {
            ndef.assign(dump.data.begin() + offset, dump.data.begin() + offset + length);
            return !ndef.empty();
        }

        offset += length;
    }

    return false;
}

std::vector<uint8_t> Pn532BleClient::buildTargetParameters(const NfcDump& dump) const
{
    std::vector<uint8_t> uid;
    hexToBytes(dump.card.uid, uid);

    while (uid.size() < 3) {
        uid.push_back(0x00);
    }

    return {
        0x04,
        0x08, 0x00,
        uid[0], uid[1], uid[2],
        0x60,
        0x01, 0xFE, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
        0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
        0xFF, 0xFF,
        0xAA, 0x99, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11,
        0x00,
        0x00,
    };
}

bool Pn532BleClient::respondToApdu(const std::vector<uint8_t>& apdu)
{
    if (apdu.size() >= 13 && apdu[0] == 0x00 && apdu[1] == 0xA4 && apdu[2] == 0x04) {
        const uint8_t app[] = {0xD2, 0x76, 0x00, 0x00, 0x85, 0x01, 0x01};

        if (memcmp(&apdu[5], app, sizeof(app)) == 0) {
            return sendStatus(0x90, 0x00);
        }
    }

    if (apdu.size() >= 7 && apdu[0] == 0x00 && apdu[1] == 0xA4 && apdu[2] == 0x00) {
        selectedEmuFile_ = (static_cast<uint16_t>(apdu[5]) << 8) | apdu[6];
        return sendStatus(0x90, 0x00);
    }

    if (apdu.size() >= 5 && apdu[0] == 0x00 && apdu[1] == 0xB0) {
        const uint16_t offset = (static_cast<uint16_t>(apdu[2]) << 8) | apdu[3];
        const uint8_t requested = apdu[4];
        const std::vector<uint8_t>* file = nullptr;

        if (selectedEmuFile_ == 0xE103) {
            file = &emulatedCcFile_;
        } else if (selectedEmuFile_ == 0xE104) {
            file = &emulatedNdefFile_;
        }

        if (file == nullptr || offset > file->size()) {
            return sendStatus(0x6A, 0x82);
        }

        const size_t available = file->size() - offset;
        const size_t length = requested < available ? requested : available;
        std::vector<uint8_t> response(file->begin() + offset, file->begin() + offset + length);
        response.push_back(0x90);
        response.push_back(0x00);
        ble_->setData(response);
        return true;
    }

    return sendStatus(0x6D, 0x00);
}

bool Pn532BleClient::sendStatus(uint8_t sw1, uint8_t sw2)
{
    ble_->setData({sw1, sw2});
    return true;
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

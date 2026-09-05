// Pn532BleClient - raw Type 2 transceive, MIFARE Classic and ISO15693 reads.

#include "Pn532BleClient.h"

#include <cstring>

namespace {
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

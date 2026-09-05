// Pn532KillerClient - emulator memory upload strategies and WR/RD traces.

#include "Pn532KillerClient.h"

#include <cstring>

#include "Pn532KillerProtocol.h"

using namespace Pn532Killer;

bool Pn532KillerClient::uploadMifareClassic1K(uint8_t slot, const NfcDump& dump)
{
    const uint8_t typeCode = killerType(KillerSlotType::Mfc1K);
    const int total = mfc1kBlockCount + 1;

    if (!setWorkMode(workModePn532, 0x00, 0x00)) {
        return false;
    }

    emitProgress("Upload slot", "MFC 1K slot " + String(slot + 1), 0, total, false, true);

    for (int sector = 0; sector < mfc1kSectorCount; sector++) {
        for (int block = 0; block < 4; block++) {
            const int globalBlock = sector * 4 + block;
            const uint8_t* blockData = &dump.data[globalBlock * mfcBlockSize];
            const uint16_t address = (static_cast<uint16_t>(sector) << 8) | static_cast<uint8_t>(block);
            const String detail = "S" + String(sector) + " B" + String(block);

            emitProgress("Upload slot", detail, globalBlock, total, false, true);

            if (!writeEmulatorData(typeCode, slot, address, blockData, mfcBlockSize)) {
                emitProgress("Upload slot", detail + " failed", globalBlock, total, true, false);
                return false;
            }
        }
    }

    emitProgress("Upload slot", "Finalizing", mfc1kBlockCount, total, false, true);

    if (!completeEmulatorWrite(typeCode, slot)) {
        emitProgress("Upload slot", "Finalize failed", mfc1kBlockCount, total, true, false);
        return false;
    }

    activeSlot_ = slot;
    activeType_ = KillerSlotType::Mfc1K;
    traceReadback(typeCode, slot, 0x0000);
    traceReadback(typeCode, slot, 0x0100);
    traceReadback(typeCode, slot, 0x0F03);
    emitProgress("Upload slot", "Slot ready", total, total, true, true);
    return true;
}

bool Pn532KillerClient::uploadNtagPages(uint8_t slot, const NfcDump& dump, int pageCount)
{
    const uint8_t typeCode = killerType(KillerSlotType::Ntag);
    const int total = pageCount + 1;

    if (!setWorkMode(workModePn532, 0x00, 0x00)) {
        return false;
    }

    emitProgress("Upload slot", "NTAG slot " + String(slot + 1), 0, total, false, true);

    for (int page = 0; page < pageCount; page++) {
        const uint8_t* pageData = &dump.data[page * type2PageSize];
        const uint8_t group = page / 4;
        const uint8_t localPage = page % 4;
        const uint16_t address = (static_cast<uint16_t>(group) << 8) | localPage;
        const String detail = "G" + String(group) + " P" + String(localPage);

        emitProgress("Upload slot", detail, page, total, false, true);

        if (!writeEmulatorData(typeCode, slot, address, pageData, type2PageSize)) {
            emitProgress("Upload slot", detail + " failed", page, total, true, false);
            return false;
        }
    }

    emitProgress("Upload slot", "Finalizing", pageCount, total, false, true);

    if (!completeEmulatorWrite(typeCode, slot)) {
        emitProgress("Upload slot", "Finalize failed", pageCount, total, true, false);
        return false;
    }

    activeSlot_ = slot;
    activeType_ = KillerSlotType::Ntag;
    traceReadback(typeCode, slot, 0x0000);
    traceReadback(typeCode, slot, 0x0100);
    traceReadback(typeCode, slot, 0x0200);
    traceReadback(typeCode, slot, 0x0B00);
    emitProgress("Upload slot", "Slot ready", total, total, true, true);
    return true;
}

bool Pn532KillerClient::uploadNtagWindows(uint8_t slot, const NfcDump& dump, int pageCount)
{
    const uint8_t typeCode = killerType(KillerSlotType::Ntag);
    const int windowCount = (pageCount + type2WindowPages - 1) / type2WindowPages;
    const int total = windowCount + 1;

    if (!setWorkMode(workModePn532, 0x00, 0x00)) {
        return false;
    }

    emitProgress("Upload slot", "NTAG slot " + String(slot + 1), 0, total, false, true);

    for (int window = 0; window < windowCount; window++) {
        uint8_t data[type2WindowSize] = {0};
        const int startPage = window * type2WindowPages;
        const int pagesInWindow = min(type2WindowPages, pageCount - startPage);

        if (pagesInWindow > 0) {
            memcpy(data, &dump.data[startPage * type2PageSize], pagesInWindow * type2PageSize);
        }

        const uint16_t address = static_cast<uint16_t>(window) << 8;
        const String detail = "Win " + String(window) + " p" + String(startPage);

        emitProgress("Upload slot", detail, window, total, false, true);

        if (!writeEmulatorData(typeCode, slot, address, data, sizeof(data))) {
            emitProgress("Upload slot", detail + " failed", window, total, true, false);
            return false;
        }
    }

    emitProgress("Upload slot", "Finalizing", windowCount, total, false, true);

    if (!completeEmulatorWrite(typeCode, slot)) {
        emitProgress("Upload slot", "Finalize failed", windowCount, total, true, false);
        return false;
    }

    activeSlot_ = slot;
    activeType_ = KillerSlotType::Ntag;
    traceReadback(typeCode, slot, 0x0000);
    traceReadback(typeCode, slot, 0x0100);
    traceReadback(typeCode, slot, 0x0200);
    traceReadback(typeCode, slot, 0x0B00);
    emitProgress("Upload slot", "Slot ready", total, total, true, true);
    return true;
}

bool Pn532KillerClient::uploadLinearUnits(uint8_t slot, KillerSlotType type, const NfcDump& dump, size_t unitSize, int unitCount, const String& label)
{
    const uint8_t typeCode = killerType(type);
    const int total = unitCount + 1;

    if (!setWorkMode(workModePn532, 0x00, 0x00)) {
        return false;
    }

    emitProgress("Upload slot", label + " slot " + String(slot + 1), 0, total, false, true);

    for (int index = 0; index < unitCount; index++) {
        const uint8_t* unitData = &dump.data[index * unitSize];
        const String detail = "Unit " + String(index + 1) + "/" + String(unitCount);
        emitProgress("Upload slot", detail, index, total, false, true);

        if (!writeEmulatorData(typeCode, slot, index, unitData, unitSize)) {
            emitProgress("Upload slot", detail + " failed", index, total, true, false);
            return false;
        }
    }

    emitProgress("Upload slot", "Finalizing", unitCount, total, false, true);

    if (!completeEmulatorWrite(typeCode, slot)) {
        emitProgress("Upload slot", "Finalize failed", unitCount, total, true, false);
        return false;
    }

    activeSlot_ = slot;
    activeType_ = type;
    traceReadback(typeCode, slot, 0x0000);
    traceReadback(typeCode, slot, 0x0100);
    traceReadback(typeCode, slot, 0x0200);
    traceReadback(typeCode, slot, 0x0300);
    emitProgress("Upload slot", "Slot ready", total, total, true, true);
    return true;
}

bool Pn532KillerClient::uploadAddressedUnits(uint8_t slot, KillerSlotType type, const NfcDump& dump, size_t unitSize, int unitCount, const String& label)
{
    const uint8_t typeCode = killerType(type);
    const int total = unitCount + 1;

    if (!setWorkMode(workModePn532, 0x00, 0x00)) {
        return false;
    }

    emitProgress("Upload slot", label + " slot " + String(slot + 1), 0, total, false, true);

    for (int index = 0; index < unitCount; index++) {
        const uint8_t* unitData = &dump.data[index * unitSize];
        const uint16_t address = static_cast<uint16_t>(index) << 8;
        const String detail = "Addr " + String(index) + " B0";

        emitProgress("Upload slot", detail, index, total, false, true);

        if (!writeEmulatorData(typeCode, slot, address, unitData, unitSize)) {
            emitProgress("Upload slot", detail + " failed", index, total, true, false);
            return false;
        }
    }

    emitProgress("Upload slot", "Finalizing", unitCount, total, false, true);

    if (!completeEmulatorWrite(typeCode, slot)) {
        emitProgress("Upload slot", "Finalize failed", unitCount, total, true, false);
        return false;
    }

    activeSlot_ = slot;
    activeType_ = type;
    traceReadback(typeCode, slot, 0x0000);
    traceReadback(typeCode, slot, 0x0100);
    traceReadback(typeCode, slot, 0x0200);
    traceReadback(typeCode, slot, 0x0300);
    emitProgress("Upload slot", "Slot ready", total, total, true, true);
    return true;
}

bool Pn532KillerClient::writeEmulatorData(uint8_t type, uint8_t slot, uint16_t index, const uint8_t* data, size_t length)
{
    std::vector<uint8_t> command = {
        commandWriteEmulator,
        type,
        slot,
        static_cast<uint8_t>((index >> 8) & 0xFF),
        static_cast<uint8_t>(index & 0xFF),
    };
    command.insert(command.end(), data, data + length);

    std::vector<uint8_t> response;

    if (!pn532_.rawCommand(command, response, 1500)) {
        setError(pn532_.lastError());
        return false;
    }

    if (response.empty()) {
        setError("No slot response @" + String(index));
        return false;
    }

    if (shouldTraceIndex(index)) {
        traceCommand("WR", type, slot, index, data, length, response);
    }

    return emulatorResponseOk(response, "Slot @" + String(index));
}

bool Pn532KillerClient::readEmulatorData(uint8_t type, uint8_t slot, uint16_t index, std::vector<uint8_t>& response)
{
    std::vector<uint8_t> command = {
        commandReadEmulator,
        type,
        slot,
        static_cast<uint8_t>((index >> 8) & 0xFF),
        static_cast<uint8_t>(index & 0xFF),
    };

    response.clear();

    if (!pn532_.rawCommand(command, response, 1500)) {
        return false;
    }

    return !response.empty();
}

bool Pn532KillerClient::completeEmulatorWrite(uint8_t type, uint8_t slot)
{
    uint8_t zeroBlock[mfcBlockSize] = {0};
    return writeEmulatorData(type, slot, 0xFFFF, zeroBlock, sizeof(zeroBlock));
}

bool Pn532KillerClient::emulatorResponseOk(const std::vector<uint8_t>& response, const String& context)
{
    if (response.empty()) {
        setError(context + " empty response");
        return false;
    }

    const uint8_t status = response.back();

    if (status != 0x00) {
        setError(context + " refused " + bytesToHex(response.data(), response.size(), true));
        return false;
    }

    return true;
}

bool Pn532KillerClient::shouldTraceIndex(uint16_t index) const
{
    return index == 0xFFFF || traceLines_.size() < 8;
}

void Pn532KillerClient::traceCommand(const String& prefix, uint8_t type, uint8_t slot, uint16_t index, const uint8_t* data, size_t length, const std::vector<uint8_t>& response)
{
    String line = prefix + " ";
    line += bytesToHex(&type, 1, false);
    line += " s";
    line += String(slot + 1);
    line += " @";
    line += String((index >> 8) & 0xFF, HEX);
    line += ":";
    line += String(index & 0xFF, HEX);
    line += " tx ";
    line += bytesToHex(data, min(length, static_cast<size_t>(8)), true);
    line += " rx ";
    line += bytesToHex(response.data(), min(response.size(), static_cast<size_t>(12)), true);
    traceLines_.push_back(line);
}

void Pn532KillerClient::traceReadback(uint8_t type, uint8_t slot, uint16_t index)
{
    std::vector<uint8_t> response;
    const bool ok = readEmulatorData(type, slot, index, response);
    String line = "RD ";
    line += bytesToHex(&type, 1, false);
    line += " s";
    line += String(slot + 1);
    line += " @";
    line += String((index >> 8) & 0xFF, HEX);
    line += ":";
    line += String(index & 0xFF, HEX);
    line += ok ? " rx " : " err ";
    line += ok ? bytesToHex(response.data(), min(response.size(), static_cast<size_t>(20)), true) : pn532_.lastError();
    traceLines_.push_back(line);
}

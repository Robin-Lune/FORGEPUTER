#include "Pn532KillerClient.h"

namespace {
constexpr uint8_t commandSetWorkMode = 0xAC;
constexpr uint8_t commandWriteEmulator = 0x1E;
constexpr uint8_t workModePn532 = 0x01;
constexpr uint8_t workModeEmulator = 0x02;
constexpr uint8_t killerTypeMfc1K = 0x01;
constexpr uint8_t killerTypeNtag = 0x02;
constexpr uint8_t killerTypeIso15693 = 0x03;
constexpr uint8_t killerTypeEm4100 = 0x04;
constexpr int mfc1kBlockCount = 64;
constexpr int mfcBlockSize = 16;
constexpr int type2PageSize = 4;
constexpr int iso15693BlockSize = 4;
}

Pn532KillerClient::Pn532KillerClient(Pn532Client& pn532)
    : pn532_(pn532)
{
}

void Pn532KillerClient::setProgressCallback(void* context, NfcReadProgressCallback callback)
{
    progressContext_ = context;
    progressCallback_ = callback;
}

bool Pn532KillerClient::switchMifareSlot(uint8_t slot)
{
    activeSlot_ = slot;
    activeType_ = KillerSlotType::Mfc1K;
    return setWorkMode(workModePn532, 0x00, 0x00);
}

bool Pn532KillerClient::switchNtagSlot(uint8_t slot)
{
    activeSlot_ = slot;
    activeType_ = KillerSlotType::Ntag;
    return setWorkMode(workModePn532, 0x00, 0x00);
}

bool Pn532KillerClient::switchIso15693Slot(uint8_t slot)
{
    activeSlot_ = slot;
    activeType_ = KillerSlotType::Iso15693;
    return setWorkMode(workModePn532, 0x00, 0x00);
}

bool Pn532KillerClient::switchEm4100Slot(uint8_t slot)
{
    activeSlot_ = slot;
    activeType_ = KillerSlotType::Em4100;
    return setWorkMode(workModePn532, 0x00, 0x00);
}

bool Pn532KillerClient::startEmulation()
{
    return startEmulation(activeSlot_, activeType_);
}

bool Pn532KillerClient::startEmulation(uint8_t slot, KillerSlotType type)
{
    const uint8_t typeCode = killerType(type);

    if (typeCode == 0) {
        setError("Slot type unsupported");
        return false;
    }

    if (slot > 7) {
        setError("Bad slot");
        return false;
    }

    if (!setWorkMode(workModePn532, 0x00, 0x00)) {
        return false;
    }

    if (!setWorkMode(workModeEmulator, typeCode, slot)) {
        return false;
    }

    activeSlot_ = slot;
    activeType_ = type;
    return true;
}

bool Pn532KillerClient::stopEmulation()
{
    return setWorkMode(workModePn532, 0x00, 0x00);
}

bool Pn532KillerClient::readSlotInfo(uint8_t slot, KillerSlotInfo& info)
{
    info.index = slot;
    info.type = slot == activeSlot_ ? activeType_ : KillerSlotType::Unknown;
    info.uid = "";
    info.loaded = slot == activeSlot_;
    return true;
}

bool Pn532KillerClient::uploadDump(uint8_t slot, const NfcDump& dump)
{
    if (slot > 7) {
        setError("Bad slot");
        return false;
    }

    if (dump.status != DumpStatus::Full) {
        setError("Full dump required");
        return false;
    }

    const KillerSlotType dumpType = slotTypeForDump(dump);
    const uint8_t typeCode = killerType(dumpType);

    if (typeCode == 0) {
        setError("Slot type unsupported");
        return false;
    }

    if (dumpType == KillerSlotType::Mfc1K) {
        if (dump.data.size() < mfc1kBlockCount * mfcBlockSize) {
            setError("Short MFC dump");
            return false;
        }

        return uploadLinearUnits(slot, dumpType, dump, mfcBlockSize, mfc1kBlockCount, "MFC 1K");
    }

    if (dumpType == KillerSlotType::Ntag) {
        const int availablePages = dump.data.size() / type2PageSize;
        const int pageCount = dump.unitsTotal > 0 ? min(dump.unitsTotal, availablePages) : availablePages;

        if (pageCount <= 0) {
            setError("Short NTAG dump");
            return false;
        }

        return uploadLinearUnits(slot, dumpType, dump, type2PageSize, pageCount, "NTAG");
    }

    if (dumpType == KillerSlotType::Iso15693) {
        const int availableBlocks = dump.data.size() / iso15693BlockSize;
        const int blockCount = dump.unitsTotal > 0 ? min(dump.unitsTotal, availableBlocks) : availableBlocks;

        if (blockCount <= 0) {
            setError("Short ISO15 dump");
            return false;
        }

        return uploadLinearUnits(slot, dumpType, dump, iso15693BlockSize, blockCount, "ISO15693");
    }

    setError("Slot type unsupported");
    return false;
}

String Pn532KillerClient::lastError() const
{
    return lastError_;
}

bool Pn532KillerClient::setWorkMode(uint8_t mode, uint8_t type, uint8_t slot)
{
    std::vector<uint8_t> response;

    if (!pn532_.rawCommand({commandSetWorkMode, mode, type, slot}, response, 1500)) {
        setError(pn532_.lastError());
        return false;
    }

    if (response.empty()) {
        setError("No work mode response");
        return false;
    }

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

    return true;
}

bool Pn532KillerClient::completeEmulatorWrite(uint8_t type, uint8_t slot)
{
    uint8_t zeroBlock[mfcBlockSize] = {0};
    return writeEmulatorData(type, slot, 0xFFFF, zeroBlock, sizeof(zeroBlock));
}

uint8_t Pn532KillerClient::killerType(KillerSlotType type) const
{
    if (type == KillerSlotType::Mfc1K) return killerTypeMfc1K;
    if (type == KillerSlotType::Ntag) return killerTypeNtag;
    if (type == KillerSlotType::Iso15693) return killerTypeIso15693;
    if (type == KillerSlotType::Em4100) return killerTypeEm4100;
    return 0;
}

KillerSlotType Pn532KillerClient::slotTypeForDump(const NfcDump& dump) const
{
    if (dump.card.type == CardType::MifareClassic1K) {
        return KillerSlotType::Mfc1K;
    }

    if (dump.card.type == CardType::MifareUltralight || dump.card.type == CardType::Ntag213 || dump.card.type == CardType::Ntag215 || dump.card.type == CardType::Ntag216) {
        return KillerSlotType::Ntag;
    }

    if (dump.card.type == CardType::Iso15693) {
        return KillerSlotType::Iso15693;
    }

    return KillerSlotType::Unknown;
}

void Pn532KillerClient::emitProgress(const String& title, const String& detail, int current, int total, bool done, bool ok)
{
    if (!progressCallback_) {
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

void Pn532KillerClient::setError(const String& error)
{
    lastError_ = error;
}

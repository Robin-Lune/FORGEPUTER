// Pn532KillerClient - slot selection, emulation control and upload dispatch.
// Upload strategies live in Pn532KillerClientUpload.cpp, vendor constants in
// Pn532KillerProtocol.h.

#include "Pn532KillerClient.h"

#include "Pn532KillerProtocol.h"

using namespace Pn532Killer;

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
    traceLines_.clear();

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

        return uploadMifareClassic1K(slot, dump);
    }

    if (dumpType == KillerSlotType::Ntag) {
        const int availablePages = dump.data.size() / type2PageSize;
        const int pageCount = dump.unitsTotal > 0 ? min(dump.unitsTotal, availablePages) : availablePages;

        if (pageCount <= 0) {
            setError("Short NTAG dump");
            return false;
        }

        return uploadNtagWindows(slot, dump, pageCount);
    }

    if (dumpType == KillerSlotType::Iso15693) {
        const int availableBlocks = dump.data.size() / iso15693BlockSize;
        const int blockCount = dump.unitsTotal > 0 ? min(dump.unitsTotal, availableBlocks) : availableBlocks;

        if (blockCount <= 0) {
            setError("Short ISO15 dump");
            return false;
        }

        return uploadAddressedUnits(slot, dumpType, dump, iso15693BlockSize, blockCount, "ISO15693");
    }

    setError("Slot type unsupported");
    return false;
}

String Pn532KillerClient::lastError() const
{
    return lastError_;
}

const std::vector<String>& Pn532KillerClient::traceLines() const
{
    return traceLines_;
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

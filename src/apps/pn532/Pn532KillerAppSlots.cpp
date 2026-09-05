// Pn532KillerApp - PN532Killer slot banks: pick, upload, start, stop.

#include "Pn532KillerApp.h"

void Pn532KillerApp::openSlotDumpPicker()
{
    if (transportMode_ == TransportMode::Ble) {
        status_ = "Slots need wired";
        draw();
        return;
    }

    if (!ensureDirectReady()) {
        return;
    }

    selectingDumpForSlot_ = true;
    pendingSlotUpload_ = false;
    refreshDumpList();
    view_ = View::DumpList;
    selectedIndex_ = 0;
    listOffset_ = 0;
    draw();
}

void Pn532KillerApp::loadSelectedDumpForSlot()
{
    if (dumpEntries_.empty()) {
        return;
    }

    selectedDump_ = selectedIndex_;

    if (!dumpStore_.load(dumpEntries_[selectedDump_].name, currentDump_)) {
        status_ = dumpStore_.lastError();
        draw();
        return;
    }

    const KillerSlotType dumpType = slotTypeForDump(currentDump_);

    if (slotTypeIndex(dumpType) < 0) {
        selectingDumpForSlot_ = false;
        pendingSlotUpload_ = false;
        status_ = "Slot type unsupported";
        view_ = View::Slots;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    selectingDumpForSlot_ = false;
    pendingSlotUpload_ = true;
    selectedSlotType_ = dumpType;
    status_ = "Choose target slot";
    view_ = View::Slots;
    selectedIndex_ = 0;
    listOffset_ = 0;
    draw();
}

void Pn532KillerApp::switchSelectedSlot()
{
    showStatus("Switching slot...");

    if (transportMode_ == TransportMode::Ble) {
        status_ = "Slots need wired";
        draw();
        return;
    }

    if (!ensureDirectReady()) {
        return;
    }

    bool ok = false;

    if (selectedSlotType_ == KillerSlotType::Mfc1K) {
        ok = killer_.switchMifareSlot(selectedSlot_);
    } else if (selectedSlotType_ == KillerSlotType::Ntag) {
        ok = killer_.switchNtagSlot(selectedSlot_);
    } else if (selectedSlotType_ == KillerSlotType::Iso15693) {
        ok = killer_.switchIso15693Slot(selectedSlot_);
    } else if (selectedSlotType_ == KillerSlotType::Em4100) {
        ok = killer_.switchEm4100Slot(selectedSlot_);
    } else {
        status_ = "Slot type unsupported";
        draw();
        return;
    }

    status_ = ok ? "Slot switched" : killer_.lastError();
    draw();
}

void Pn532KillerApp::startSlot()
{
    showStatus("Starting...");

    if (transportMode_ == TransportMode::Ble) {
        status_ = "Slots need wired";
        draw();
        return;
    }

    if (!ensureDirectReady()) {
        return;
    }

    const bool ok = killer_.startEmulation(selectedSlot_, selectedSlotType_);
    status_ = ok ? String("Emulating ") + killerSlotTypeName(selectedSlotType_) + " " + String(selectedSlot_ + 1) : killer_.lastError();
    draw();
}

void Pn532KillerApp::stopSlot()
{
    showStatus("Stopping...");

    if (transportMode_ == TransportMode::Ble) {
        status_ = "Slots need wired";
        draw();
        return;
    }

    if (!ensureDirectReady()) {
        return;
    }

    status_ = killer_.stopEmulation() ? "Emulation stopped" : killer_.lastError();
    draw();
}

bool Pn532KillerApp::uploadCurrentDumpToSlot()
{
    if (!hasCurrentDump()) {
        status_ = "Load/read a dump first";
        draw();
        return false;
    }

    showStatus("Uploading dump...");

    if (transportMode_ == TransportMode::Ble) {
        status_ = "Slots need wired";
        draw();
        return false;
    }

    if (!ensureDirectReady()) {
        return false;
    }

    const KillerSlotType dumpType = slotTypeForDump(currentDump_);
    const int bankIndex = slotTypeIndex(dumpType);

    if (bankIndex < 0) {
        status_ = "Slot type unsupported";
        draw();
        return false;
    }

    selectedSlotType_ = dumpType;
    slotUploadResultVisible_ = false;
    beginReadProgress("Upload slot");
    readLog_.push_back("Dump " + (currentDump_.name.length() > 0 ? currentDump_.name : suggestedDumpName()));
    readLog_.push_back(String(killerSlotTypeName(dumpType)) + " slot " + String(selectedSlot_ + 1));
    readLog_.push_back(String(currentDump_.data.size()) + " bytes");

    const bool uploaded = killer_.uploadDump(selectedSlot_, currentDump_);
    const std::vector<String> traceLines = killer_.traceLines();
    const bool ok = uploaded && killer_.startEmulation(selectedSlot_, dumpType);

    readLog_.clear();
    readLogOffset_ = 0;
    readLog_.push_back("Dump " + (currentDump_.name.length() > 0 ? currentDump_.name : suggestedDumpName()));
    readLog_.push_back(String(killerSlotTypeName(dumpType)) + " slot " + String(selectedSlot_ + 1));
    readLog_.push_back(String(currentDump_.data.size()) + " bytes");

    if (traceLines.empty()) {
        readLog_.push_back("No WR/RD trace captured");
    } else {
        for (const String& line : traceLines) {
            readLog_.push_back(line);
        }
    }

    readLog_.push_back(uploaded ? "Upload accepted" : "Upload refused");
    if (uploaded) {
        readLog_.push_back(ok ? "Emulation started" : "Emulation start failed");
    }

    String slotLogPath;
    if (saveSlotUploadLog(dumpType, selectedSlot_, uploaded, ok, slotLogPath)) {
        readLog_.push_back("Log saved");
        readLog_.push_back(slotLogPath);
    } else {
        readLog_.push_back("Log save failed");
    }

    if (ok) {
        slotLoaded_[bankIndex][selectedSlot_] = true;
        slotNames_[bankIndex][selectedSlot_] = currentDump_.name.length() > 0 ? currentDump_.name : suggestedDumpName();
        slotUids_[bankIndex][selectedSlot_] = currentDump_.card.uid;
        pendingSlotUpload_ = false;
        status_ = String(killerSlotTypeName(dumpType)) + " slot " + String(selectedSlot_ + 1) + " emulating";
    } else {
        status_ = uploaded ? "Start failed: " + killer_.lastError() : killer_.lastError();
    }

    readProgress_.title = "Upload slot";
    readProgress_.detail = "Scroll WR/RD logs";
    readProgress_.done = true;
    readProgress_.ok = ok;
    if (readProgress_.total == 0) {
        readProgress_.total = 1;
    }
    if (ok) {
        readProgress_.current = readProgress_.total;
    }
    readLog_.push_back((ok ? "OK " : "!! ") + status_);
    slotUploadResultVisible_ = true;
    drawReadProgress();
    return ok;
}

int Pn532KillerApp::slotTypeIndex(KillerSlotType type) const
{
    if (type == KillerSlotType::Mfc1K) return 0;
    if (type == KillerSlotType::Ntag) return 1;
    if (type == KillerSlotType::Iso15693) return 2;
    if (type == KillerSlotType::Em4100) return 3;
    return -1;
}

KillerSlotType Pn532KillerApp::slotTypeForIndex(int index) const
{
    if (index == 0) return KillerSlotType::Mfc1K;
    if (index == 1) return KillerSlotType::Ntag;
    if (index == 2) return KillerSlotType::Iso15693;
    if (index == 3) return KillerSlotType::Em4100;
    return KillerSlotType::Unknown;
}

void Pn532KillerApp::cycleSlotType()
{
    const int currentIndex = max(0, slotTypeIndex(selectedSlotType_));
    selectedSlotType_ = slotTypeForIndex((currentIndex + 1) % slotTypeCount_);
    draw();
}

KillerSlotType Pn532KillerApp::slotTypeForDump(const NfcDump& dump) const
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

const char* Pn532KillerApp::killerSlotTypeName(KillerSlotType type) const
{
    if (type == KillerSlotType::Mfc1K) return "MFC 1K";
    if (type == KillerSlotType::Ntag) return "NTAG";
    if (type == KillerSlotType::Iso15693) return "ISO15693";
    if (type == KillerSlotType::Em4100) return "EM4100";
    return "unknown";
}

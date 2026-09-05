// Pn532KillerApp - scan, read, retry, save and saved-dump actions.

#include "Pn532KillerApp.h"

void Pn532KillerApp::scanInfo()
{
    showStatus("Scanning...");

    bool ok = false;
    lastCard_ = CardInfo();

    if (transportMode_ == TransportMode::Ble) {
        ok = ensureBleReady() && bleClient_.scan(lastCard_);
        status_ = ok ? "Card detected" : bleClient_.lastError();
    } else {
        ok = ensureDirectReady() && reader_.scan(lastCard_);
        status_ = ok ? "Card detected" : reader_.lastError();
    }

    if (ok) {
        view_ = View::ScanInfo;
    } else {
        view_ = View::ScanInfo;
    }

    draw();
}

void Pn532KillerApp::readAuto()
{
    bool ok = false;
    currentDump_ = NfcDump();
    resultOffset_ = 0;
    beginReadProgress("Reading...");

    if (transportMode_ == TransportMode::Ble) {
        ok = ensureBleReady() && bleClient_.readAuto(currentDump_);
        status_ = ok ? "Read complete" : bleClient_.lastError();
    } else {
        ok = ensureDirectReady() && reader_.readAuto(currentDump_);
        status_ = ok ? "Read complete" : reader_.lastError();
    }

    if (ok) {
        currentDump_.name = "";
    } else {
        readLog_.push_back(status_);
    }

    selectedIndex_ = 0;
    listOffset_ = 0;
    view_ = View::CardInfo;
    draw();
}

void Pn532KillerApp::beginReadProgress(const String& title)
{
    readProgress_ = NfcReadProgress();
    readProgress_.title = title;
    readProgress_.detail = "Preparing";
    readLog_.clear();
    readLogOffset_ = 0;
    view_ = View::ReadProgress;
    draw();
}

void Pn532KillerApp::handleReadProgress(const NfcReadProgress& progress)
{
    readProgress_ = progress;

    if (progress.done || readLog_.empty()) {
        String prefix = progress.ok ? "OK " : "!! ";
        readLog_.push_back(prefix + progress.detail);
    }

    drawReadProgress();
    yield();
}

void Pn532KillerApp::handleReadProgressThunk(void* context, const NfcReadProgress& progress)
{
    if (context == nullptr) {
        return;
    }

    static_cast<Pn532KillerApp*>(context)->handleReadProgress(progress);
}

void Pn532KillerApp::saveCurrentDump()
{
    String savedName;

    if (dumpStore_.saveAs(currentDump_, saveName_, savedName)) {
        currentDump_.name = savedName;
        status_ = "Saved " + savedName;
        view_ = View::CardInfo;
    } else {
        status_ = dumpStore_.lastError();
    }

    draw();
}

void Pn532KillerApp::openSaveName()
{
    if (currentDump_.status == DumpStatus::Empty && lastCard_.uid.length() > 0) {
        prepareInfoDumpFromScan();
    }

    saveName_ = currentDump_.name.length() > 0 ? currentDump_.name : suggestedDumpName();
    view_ = View::SaveName;
    draw();
}

String Pn532KillerApp::suggestedDumpName() const
{
    String uid = currentDump_.card.uid;
    uid.replace(":", "");
    uid.toLowerCase();

    if (uid.length() == 0) {
        uid = "unknown";
    }

    String prefix = "nfc";

    if (currentDump_.card.type == CardType::MifareClassicMini || currentDump_.card.type == CardType::MifareClassic1K || currentDump_.card.type == CardType::MifareClassic4K) {
        prefix = "mfc";
    } else if (currentDump_.card.type == CardType::MifareUltralight || currentDump_.card.type == CardType::Ntag213 || currentDump_.card.type == CardType::Ntag215 || currentDump_.card.type == CardType::Ntag216) {
        prefix = "ntag";
    } else if (currentDump_.card.type == CardType::Iso15693) {
        prefix = "iso15";
    }

    return prefix + "_" + uid;
}

void Pn532KillerApp::prepareInfoDumpFromScan()
{
    currentDump_ = NfcDump();
    currentDump_.card = lastCard_;
    currentDump_.card.family = cardFamilyForType(lastCard_.type);
    currentDump_.familyName = cardFamilyName(currentDump_.card.family);
    currentDump_.exactType = lastCard_.exactType.length() > 0 ? lastCard_.exactType : cardTypeName(lastCard_.type);
    currentDump_.statusSummary = "Card detected";
    currentDump_.cloneSummary = "Info only";
    currentDump_.actionCapabilities = "Save info, Read detected";
    currentDump_.status = DumpStatus::InfoOnly;
    currentDump_.unitsTotal = 0;
    currentDump_.unitsRead = 0;
}

void Pn532KillerApp::performCardAction()
{
    const String action = cardActionLabel(selectedIndex_);

    if (action == "Read detected") {
        readAuto();
        return;
    }

    if (action == "Save dump" || action == "Save partial" || action == "Save info") {
        openSaveName();
        return;
    }

    if (action == "Retry missing") {
        retryMissing();
        return;
    }

    if (action == "Retry read") {
        readAuto();
        return;
    }

    if (action == "Diag read" || action == "Diagnostics") {
        runNtagDiagnostic(View::CardActions);
        return;
    }

    if (action == "Keys") {
        status_ = keyStore_.status();
        draw();
        return;
    }

    if (action == "Emulate dump" || action == "Emulate full") {
        startBleNdefEmulation();
        return;
    }

    if (action == "Custom NDEF") {
        openBleNdefEditor(false);
        return;
    }

    if (action == "Write NDEF") {
        view_ = View::WriteTag;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (action == "Write card" || action == "Raw info") {
        status_ = action + ": next";
        draw();
        return;
    }

    goBack();
}

void Pn532KillerApp::performDumpAction()
{
    const String action = dumpActionLabel(selectedIndex_);

    if (action == "View missing") {
        resultOffset_ = 0;
        view_ = View::MissingUnits;
        draw();
        return;
    }

    if (action == "Retry read") {
        if (isMifareClassic(currentDump_.card.type)) {
            retryMissing();
        } else {
            readAuto();
        }
        return;
    }

    if (action == "Diag read") {
        runNtagDiagnostic(View::DumpDetail);
        return;
    }

    if (action == "Emulate dump") {
        startBleNdefEmulation();
        return;
    }

    if (action == "Custom NDEF") {
        openBleNdefEditor(true);
        return;
    }

    if (action == "Write card" || action == "Raw info") {
        status_ = action + ": next";
        draw();
        return;
    }

    if (action == "Delete") {
        deleteSelectedDump();
        draw();
        return;
    }

    goBack();
}

void Pn532KillerApp::retryMissing()
{
    if (!hasCurrentDump() || !isMifareClassic(currentDump_.card.type) || currentDump_.missingUnits.empty()) {
        status_ = "No missing sectors";
        resultOffset_ = 0;
        view_ = View::MissingUnits;
        draw();
        return;
    }

    retryBeforeRead_ = currentDump_.unitsRead;
    retryBeforeMissing_ = currentDump_.missingUnits.size();
    beginReadProgress("Retry MFC");
    bool ok = false;

    if (transportMode_ == TransportMode::Ble) {
        ok = ensureBleReady() && bleClient_.retryMissing(currentDump_);
        status_ = ok ? "Retry complete" : bleClient_.lastError();
    } else {
        ok = ensureDirectReady() && reader_.retryMissing(currentDump_);
        status_ = ok ? "Retry complete" : reader_.lastError();
    }

    retryAfterRead_ = currentDump_.unitsRead;
    retryAfterMissing_ = currentDump_.missingUnits.size();

    if (!ok) {
        view_ = View::CardInfo;
        draw();
        return;
    }

    resultOffset_ = 0;
    selectedIndex_ = 0;
    listOffset_ = 0;
    view_ = View::RetryResult;
    draw();
}

void Pn532KillerApp::refreshDumpList()
{
    dumpStore_.list(dumpEntries_);
}

void Pn532KillerApp::loadSelectedDump()
{
    if (dumpEntries_.empty()) {
        return;
    }

    selectedDump_ = selectedIndex_;

    if (dumpStore_.load(dumpEntries_[selectedDump_].name, currentDump_)) {
        view_ = View::DumpDetail;
        selectedIndex_ = 0;
        listOffset_ = 0;
    } else {
        status_ = dumpStore_.lastError();
    }

    draw();
}

void Pn532KillerApp::deleteSelectedDump()
{
    if (!hasCurrentDump()) {
        return;
    }

    dumpStore_.remove(currentDump_.name);
    refreshDumpList();
    view_ = View::DumpList;
    selectedIndex_ = 0;
    listOffset_ = 0;
    currentDump_ = NfcDump();
}

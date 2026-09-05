// Pn532KillerApp - BLE NDEF emulation window and tag NDEF writing.

#include "Pn532KillerApp.h"

void Pn532KillerApp::startBleNdefEmulation()
{
    if (transportMode_ != TransportMode::Ble) {
        status_ = "BLE mode required";
        draw();
        return;
    }

    if (!hasCurrentDump()) {
        status_ = "Load/read a dump first";
        draw();
        return;
    }

    emulateFromDump_ = currentDump_.name.length() > 0;
    prepareBleEmulationWindow(true, std::vector<uint8_t>(), currentDump_.card);
}

void Pn532KillerApp::openBleNdefEditor(bool fromDump)
{
    if (transportMode_ != TransportMode::Ble) {
        status_ = "BLE mode required";
        draw();
        return;
    }

    emulateFromDump_ = fromDump;
    view_ = View::EmulateNdefEdit;
    selectedIndex_ = 0;
    listOffset_ = 0;
    draw();
}

void Pn532KillerApp::startBleEditedNdefEmulation()
{
    if (transportMode_ != TransportMode::Ble) {
        status_ = "BLE mode required";
        draw();
        return;
    }

    if (emulateBuffer_.length() == 0) {
        status_ = "NDEF text empty";
        draw();
        return;
    }

    const bool returnToDump = emulateFromDump_;

    CardInfo card;

    if (hasCurrentDump()) {
        card = currentDump_.card;
    } else if (lastCard_.uid.length() > 0) {
        card = lastCard_;
    } else {
        card.type = CardType::Iso14443A;
        card.uid = "04:46:4F";
    }

    std::vector<uint8_t> record = emulateUrlMode_ ? NdefMessageBuilder::urlRecord(emulateBuffer_) : NdefMessageBuilder::textRecord(emulateBuffer_);

    currentDump_.card = card;
    emulateFromDump_ = returnToDump;
    prepareBleEmulationWindow(false, record, card);
}

void Pn532KillerApp::stopBleNdefEmulation()
{
    bleEmulatePending_ = false;
    bleEmulatePendingUseDump_ = false;
    bleEmulatePendingRecord_.clear();
    bleEmulatePendingCard_ = CardInfo();
    bleEmulateStartedAt_ = 0;
    bleEmulateLastDrawAt_ = 0;
    bleEmulateNextAttemptAt_ = 0;
    bleClient_.stopNdefEmulation();
    status_ = "Emulation stopped";

    if (emulateFromDump_ && currentDump_.name.length() > 0) {
        view_ = View::DumpDetail;
    } else if (currentDump_.status != DumpStatus::Empty) {
        view_ = View::CardActions;
    } else {
        view_ = View::Menu;
    }

    selectedIndex_ = 0;
    listOffset_ = 0;
    draw();
}

void Pn532KillerApp::prepareBleEmulationWindow(bool useDump, const std::vector<uint8_t>& record, const CardInfo& card)
{
    bleEmulatePending_ = true;
    bleEmulatePendingUseDump_ = useDump;
    bleEmulatePendingRecord_ = record;
    bleEmulatePendingCard_ = card;
    bleEmulateStartedAt_ = millis();
    bleEmulateLastDrawAt_ = bleEmulateStartedAt_;
    bleEmulateNextAttemptAt_ = bleEmulateStartedAt_;
    status_ = "Phone near antenna";
    view_ = View::BleEmulate;
    selectedIndex_ = 0;
    listOffset_ = 0;
    draw();
}

int Pn532KillerApp::bleEmulateSecondsLeft() const
{
    if (bleEmulateStartedAt_ == 0) {
        return bleEmulateDurationMs_ / 1000;
    }

    const uint32_t elapsed = millis() - bleEmulateStartedAt_;

    if (elapsed >= bleEmulateDurationMs_) {
        return 0;
    }

    return static_cast<int>((bleEmulateDurationMs_ - elapsed + 999) / 1000);
}

void Pn532KillerApp::writeCurrentNdef()
{
    showStatus("Writing...");

    if (transportMode_ == TransportMode::Ble) {
        status_ = "BLE write not ready";
        draw();
        return;
    }

    if (!ensureDirectReady()) {
        return;
    }

    const bool ok = writeUrlMode_ ? ndefWriter_.writeUrl(writeBuffer_) : ndefWriter_.writeText(writeBuffer_);
    status_ = ok ? "NDEF written" : ndefWriter_.lastError();
    draw();
}

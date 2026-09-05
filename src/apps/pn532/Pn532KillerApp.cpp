// Pn532KillerApp - lifecycle, transport selection and shared helpers.
// Screens, input, menus, read, diagnostics, slots and emulation live in the
// Pn532KillerApp*.cpp siblings listed in Pn532KillerApp.h.

#include "Pn532KillerApp.h"

#include <SD.h>
#include <SPI.h>

#include "../../ui/Screen.h"

#ifndef FORGE_SD_SCK_PIN
#define FORGE_SD_SCK_PIN 40
#endif

#ifndef FORGE_SD_MISO_PIN
#define FORGE_SD_MISO_PIN 39
#endif

#ifndef FORGE_SD_MOSI_PIN
#define FORGE_SD_MOSI_PIN 14
#endif

#ifndef FORGE_SD_CS_PIN
#define FORGE_SD_CS_PIN 12
#endif

Pn532KillerApp::Pn532KillerApp(PowerManager& powerManager)
    : powerManager_(powerManager),
      transport_(Serial1),
      pn532_(transport_),
      killer_(pn532_),
      reader_(pn532_, keyStore_),
      ndefWriter_(pn532_),
      bleClient_(keyStore_)
{
    reader_.setProgressCallback(this, handleReadProgressThunk);
    bleClient_.setProgressCallback(this, handleReadProgressThunk);
    killer_.setProgressCallback(this, handleReadProgressThunk);
}

void Pn532KillerApp::init()
{
    releaseBleConnection();
    view_ = View::Transport;
    transportMode_ = TransportMode::None;
    selectedIndex_ = 0;
    listOffset_ = 0;
    status_ = "Choose link";
    pn532Ready_ = false;
    lastCard_ = CardInfo();
    currentDump_ = NfcDump();
    selectingDumpForSlot_ = false;
    pendingSlotUpload_ = false;
    slotUploadResultVisible_ = false;
    ntagDiagnosticVisible_ = false;
    rawDiagLegacyIdxFailed_ = false;
    rawDiagType2CrcOk_ = false;
    bleEmulatePending_ = false;
    bleEmulatePendingUseDump_ = false;
    bleEmulatePendingRecord_.clear();
    bleEmulatePendingCard_ = CardInfo();
    bleEmulateStartedAt_ = 0;
    bleEmulateLastDrawAt_ = 0;
    bleEmulateNextAttemptAt_ = 0;
    initStorage();
}

void Pn532KillerApp::update()
{
    if (view_ != View::BleEmulate) {
        return;
    }

    const uint32_t now = millis();

    if (now - bleEmulateStartedAt_ >= bleEmulateDurationMs_) {
        stopBleNdefEmulation();
        return;
    }

    bool changed = false;

    if (bleEmulatePending_ && !bleClient_.isEmulating() && now >= bleEmulateNextAttemptAt_) {
        bleEmulateNextAttemptAt_ = now + 4500;

        if (!ensureBleReady()) {
            status_ = bleClient_.lastError();
        } else {
            const bool ok = bleEmulatePendingUseDump_
                ? bleClient_.startNdefEmulation(currentDump_)
                : bleClient_.startNdefEmulation(bleEmulatePendingRecord_, bleEmulatePendingCard_);

            if (ok) {
                bleEmulatePending_ = false;
                status_ = "NDEF active";
            } else {
                status_ = bleClient_.lastError();
            }
        }

        changed = true;
    }

    if (now - bleEmulateLastDrawAt_ >= 1000) {
        bleEmulateLastDrawAt_ = now;
        changed = true;
    }

    if (bleClient_.isEmulating()) {
        const bool hadExchange = bleClient_.tickNdefEmulation();

        if (hadExchange) {
            changed = true;
        } else if (!bleClient_.isEmulating()) {
            bleEmulatePending_ = true;
            bleEmulateNextAttemptAt_ = millis() + 200;
            status_ = "Reader released";
            changed = true;
        }
    }

    if (changed) {
        draw();
    }
}

void Pn532KillerApp::draw()
{
    Screen::setup();
    Screen::clear();

    if (view_ == View::Transport) {
        drawTransport();
    } else if (view_ == View::Menu) {
        drawMenu();
    } else if (view_ == View::ScanInfo) {
        drawScanInfo();
    } else if (view_ == View::ReadProgress) {
        drawReadProgress();
    } else if (view_ == View::CardInfo) {
        drawCardInfo();
    } else if (view_ == View::CardActions) {
        drawCardActions();
    } else if (view_ == View::RetryResult) {
        drawRetryResult();
    } else if (view_ == View::SaveName) {
        drawSaveName();
    } else if (view_ == View::DumpList) {
        drawDumpList();
    } else if (view_ == View::DumpDetail) {
        drawDumpDetail();
    } else if (view_ == View::MissingUnits) {
        drawMissingUnits();
    } else if (view_ == View::BleEmulate) {
        drawBleEmulate();
    } else if (view_ == View::EmulateNdefEdit) {
        drawEmulateNdefEdit();
    } else if (view_ == View::Slots) {
        drawSlots();
    } else if (view_ == View::WriteTag) {
        drawWriteTag();
    } else {
        drawLab();
    }
}

void Pn532KillerApp::close()
{
    releaseBleConnection();
    transport_.end();
    usbTransport_.end();
}

void Pn532KillerApp::initStorage()
{
    SPI.begin(FORGE_SD_SCK_PIN, FORGE_SD_MISO_PIN, FORGE_SD_MOSI_PIN, FORGE_SD_CS_PIN);
    SD.begin(FORGE_SD_CS_PIN, SPI);
    dumpStore_.begin();
    keyStore_.begin();
}

void Pn532KillerApp::initPn532()
{
    pn532Ready_ = pn532_.begin();
    if (pn532Ready_) {
        status_ = transportMode_ == TransportMode::UsbCdc ? "PN532 USB-C ready" : "PN532 UART ready";
    } else {
        status_ = transportMode_ == TransportMode::UsbCdc && usbTransport_.lastError().length() > 0 ? usbTransport_.lastError() : pn532_.lastError();
    }
}

void Pn532KillerApp::releaseBleConnection()
{
    if (transportMode_ == TransportMode::Ble || bleClient_.isConnected()) {
        if (bleClient_.isEmulating()) {
            bleClient_.stopNdefEmulation();
        }

        bleClient_.disconnect();
    }
}

void Pn532KillerApp::selectTransport()
{
    if (selectedIndex_ == 0) {
        releaseBleConnection();
        usbTransport_.end();
        transportMode_ = TransportMode::Uart;
        pn532_.setTransport(transport_);
        initPn532();

        if (!pn532Ready_) {
            transport_.end();
            transportMode_ = TransportMode::None;
            status_ = pn532_.lastError();
            draw();
            return;
        }

        selectedIndex_ = 0;
        listOffset_ = 0;
        view_ = View::Menu;
        draw();
        return;
    }

    if (selectedIndex_ == 1) {
        releaseBleConnection();
        transport_.end();
        transportMode_ = TransportMode::UsbCdc;
        pn532_.setTransport(usbTransport_);
        showStatus("USB-C serial...");
        initPn532();

        if (!pn532Ready_) {
            transportMode_ = TransportMode::None;
            status_ = "USB err: " + status_;
            selectedIndex_ = 1;
            listOffset_ = 0;
            draw();
            return;
        }

        selectedIndex_ = 0;
        listOffset_ = 0;
        view_ = View::Menu;
        draw();
        return;
    }

    transportMode_ = TransportMode::Ble;
    showStatus("Scanning BLE...");

    if (!bleClient_.begin()) {
        transportMode_ = TransportMode::None;
        pn532Ready_ = false;
        status_ = bleClient_.lastError();
        draw();
        return;
    }

    pn532Ready_ = true;
    selectedIndex_ = 0;
    listOffset_ = 0;
    status_ = bleClient_.deviceName();
    view_ = View::Menu;
    draw();
}

bool Pn532KillerApp::ensureDirectReady()
{
    if (transportMode_ != TransportMode::Uart && transportMode_ != TransportMode::UsbCdc) {
        status_ = "Select UART/USB first";
        view_ = View::Transport;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return false;
    }

    if (!pn532Ready_) {
        initPn532();
    }

    return pn532Ready_;
}

bool Pn532KillerApp::ensureBleReady()
{
    if (transportMode_ != TransportMode::Ble) {
        status_ = "Select BLE first";
        view_ = View::Transport;
        selectedIndex_ = 1;
        listOffset_ = 0;
        draw();
        return false;
    }

    if (!bleClient_.isConnected()) {
        showStatus("Scanning BLE...");
        pn532Ready_ = bleClient_.begin();
    }

    return bleClient_.isConnected();
}

bool Pn532KillerApp::isMifareClassic(CardType type) const
{
    return type == CardType::MifareClassicMini || type == CardType::MifareClassic1K || type == CardType::MifareClassic4K;
}

bool Pn532KillerApp::isNtag(CardType type) const
{
    return type == CardType::MifareUltralight || type == CardType::Ntag213 || type == CardType::Ntag215 || type == CardType::Ntag216;
}

void Pn532KillerApp::showStatus(const String& status)
{
    status_ = status;
    Screen::clear();
    Screen::drawTitle("ForgeNFC", status_.c_str());
    Screen::drawInputLine("Working...");
}

bool Pn532KillerApp::hasCurrentDump() const
{
    return currentDump_.status != DumpStatus::Empty && (currentDump_.name.length() > 0 || !currentDump_.data.empty());
}

#include "Pn532KillerApp.h"

#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

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

void Pn532KillerApp::onKey(const KeyInput& input)
{
    if (view_ == View::BleEmulate) {
        if (input.del || input.backspace) {
            stopBleNdefEmulation();
        }

        return;
    }

    if (view_ == View::EmulateNdefEdit) {
        if (input.del) {
            goBack();
            return;
        }

        if (input.backspace) {
            if (emulateBuffer_.length() > 0) {
                emulateBuffer_.remove(emulateBuffer_.length() - 1);
            }

            draw();
            return;
        }

        if (input.tab || input.left || input.right) {
            emulateUrlMode_ = !emulateUrlMode_;
            draw();
            return;
        }

        if (input.enter) {
            startBleEditedNdefEmulation();
            return;
        }

        if (input.characters.length() > 0) {
            emulateBuffer_ += input.characters;
            draw();
        }

        return;
    }

    if (view_ == View::SaveName) {
        if (input.del) {
            view_ = View::CardActions;
            draw();
            return;
        }

        if (input.backspace) {
            if (saveName_.length() > 0) {
                saveName_.remove(saveName_.length() - 1);
            }

            draw();
            return;
        }

        if (input.enter) {
            saveCurrentDump();
            return;
        }

        if (input.characters.length() > 0 && saveName_.length() < 28) {
            saveName_ += input.characters;
            draw();
        }

        return;
    }

    if (view_ == View::WriteTag) {
        if (input.del) {
            goBack();
            return;
        }

        if (input.backspace) {
            if (writeBuffer_.length() > 0) {
                writeBuffer_.remove(writeBuffer_.length() - 1);
            }

            draw();
            return;
        }

        if (input.tab) {
            writeUrlMode_ = !writeUrlMode_;
            draw();
            return;
        }

        if (input.enter) {
            writeCurrentNdef();
            return;
        }

        if (input.left || input.right) {
            writeUrlMode_ = !writeUrlMode_;
            draw();
            return;
        }

        if (input.characters.length() > 0) {
            writeBuffer_ += input.characters;
            draw();
        }

        return;
    }

    if (view_ == View::ReadProgress && (slotUploadResultVisible_ || ntagDiagnosticVisible_)) {
        if (ntagDiagnosticVisible_ && (input.up || input.down)) {
            const int maxOffset = max(0, static_cast<int>(readLog_.size()) - 3);

            if (input.up && readLogOffset_ > 0) {
                readLogOffset_--;
                drawReadProgress();
            }

            if (input.down && readLogOffset_ < maxOffset) {
                readLogOffset_++;
                drawReadProgress();
            }

            return;
        }

        if (input.enter || input.del || input.backspace) {
            if (slotUploadResultVisible_) {
                slotUploadResultVisible_ = false;
                view_ = View::Slots;
            } else {
                ntagDiagnosticVisible_ = false;
                view_ = diagnosticReturnView_;
            }
            selectedIndex_ = 0;
            listOffset_ = 0;
            draw();
        }

        return;
    }

    if (input.backspace || input.del) {
        goBack();
        return;
    }

    if (view_ == View::CardInfo || view_ == View::MissingUnits) {
        const int lineCount = view_ == View::CardInfo ? cardInfoLineCount() : missingUnitLineCount();
        const int maxOffset = max(0, lineCount - 4);

        if (input.up) {
            if (resultOffset_ > 0) {
                resultOffset_--;
                draw();
            }

            return;
        }

        if (input.down) {
            if (resultOffset_ < maxOffset) {
                resultOffset_++;
                draw();
            }

            return;
        }
    }

    if (input.up) {
        moveSelection(-1);
        return;
    }

    if (input.down) {
        moveSelection(1);
        return;
    }

    if (input.left && view_ == View::Slots) {
        selectedSlot_ = selectedSlot_ == 0 ? 7 : selectedSlot_ - 1;
        draw();
        return;
    }

    if (input.right && view_ == View::Slots) {
        selectedSlot_ = (selectedSlot_ + 1) % 8;
        draw();
        return;
    }

    if (input.tab && view_ == View::Slots) {
        if (pendingSlotUpload_) {
            status_ = "Type follows dump";
            draw();
            return;
        }

        cycleSlotType();
        return;
    }

    if (input.enter) {
        openSelectedItem();
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

void Pn532KillerApp::moveSelection(int delta)
{
    int count = mainMenuCount();

    if (view_ == View::Transport) {
        count = transportCount_;
    } else if (view_ == View::CardActions) {
        count = cardActionCount();
    } else if (view_ == View::DumpList) {
        count = dumpEntries_.size();
    } else if (view_ == View::DumpDetail) {
        count = dumpActionCount();
    } else if (view_ == View::EmulateNdefEdit) {
        return;
    } else if (view_ == View::Slots) {
        count = 4;
    } else if (view_ == View::Lab) {
        count = 3;
    } else if (view_ != View::Menu) {
        return;
    }

    if (count == 0) {
        return;
    }

    selectedIndex_ += delta;

    if (selectedIndex_ < 0) {
        selectedIndex_ = count - 1;
    }

    if (selectedIndex_ >= count) {
        selectedIndex_ = 0;
    }

    updateListOffset(count);
    draw();
}

void Pn532KillerApp::openSelectedItem()
{
    if (view_ == View::Transport) {
        selectTransport();
        return;
    }

    if (view_ == View::Menu) {
        openMainMenuItem();
        return;
    }

    if (view_ == View::CardInfo) {
        if (currentDump_.status == DumpStatus::Empty && lastCard_.uid.length() == 0) {
            goBack();
            return;
        }

        view_ = View::CardActions;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (view_ == View::CardActions) {
        performCardAction();
        return;
    }

    if (view_ == View::RetryResult) {
        view_ = View::CardInfo;
        selectedIndex_ = 0;
        listOffset_ = 0;
        resultOffset_ = 0;
        draw();
        return;
    }

    if (view_ == View::ScanInfo) {
        scanInfo();
        return;
    }

    if (view_ == View::DumpList) {
        if (selectingDumpForSlot_) {
            loadSelectedDumpForSlot();
        } else {
            loadSelectedDump();
        }
        return;
    }

    if (view_ == View::DumpDetail) {
        performDumpAction();
        return;
    }

    if (view_ == View::MissingUnits) {
        view_ = View::DumpDetail;
        selectedIndex_ = 0;
        listOffset_ = 0;
        resultOffset_ = 0;
        draw();
        return;
    }

    if (view_ == View::Slots) {
        if (pendingSlotUpload_) {
            if (selectedIndex_ == 0) uploadCurrentDumpToSlot();
            if (selectedIndex_ == 1) openSlotDumpPicker();
            if (selectedIndex_ == 2) {
                pendingSlotUpload_ = false;
                currentDump_ = NfcDump();
                status_ = "Upload canceled";
                draw();
            }
            if (selectedIndex_ == 3) {
                pendingSlotUpload_ = false;
                currentDump_ = NfcDump();
                goBack();
            }
            return;
        }

        if (selectedIndex_ == 0) startSlot();
        if (selectedIndex_ == 1) stopSlot();
        if (selectedIndex_ == 2) openSlotDumpPicker();
        if (selectedIndex_ == 3) goBack();
        return;
    }

    if (view_ == View::Lab) {
        if (selectedIndex_ == 0) runRawPn532Diagnostic();
        if (selectedIndex_ == 1 || selectedIndex_ == 2) {
            status_ = "Lab: next";
            draw();
        }
        return;
    }
}

void Pn532KillerApp::goBack()
{
    if (view_ == View::Transport) {
        return;
    }

    if (view_ == View::Menu) {
        releaseBleConnection();
        transport_.end();
        usbTransport_.end();
        view_ = View::Transport;
        transportMode_ = TransportMode::None;
        pn532Ready_ = false;
        status_ = "Choose link";
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (view_ == View::DumpDetail) {
        selectingDumpForSlot_ = false;
        view_ = View::DumpList;
    } else if (view_ == View::DumpList && selectingDumpForSlot_) {
        selectingDumpForSlot_ = false;
        view_ = View::Slots;
    } else if (view_ == View::Slots && pendingSlotUpload_) {
        pendingSlotUpload_ = false;
        currentDump_ = NfcDump();
        view_ = View::Menu;
    } else if (view_ == View::MissingUnits) {
        view_ = View::DumpDetail;
    } else if (view_ == View::EmulateNdefEdit) {
        view_ = emulateFromDump_ ? View::DumpDetail : View::Menu;
    } else if (view_ == View::SaveName) {
        view_ = View::CardActions;
    } else if (view_ == View::RetryResult) {
        view_ = View::CardInfo;
    } else if (view_ == View::CardActions) {
        view_ = View::CardInfo;
    } else {
        view_ = View::Menu;
    }

    selectedIndex_ = 0;
    listOffset_ = 0;
    draw();
}

void Pn532KillerApp::drawTransport()
{
    const bool hasUsbError = status_.startsWith("USB err:");

    Screen::drawTitle("ForgeNFC", hasUsbError ? "USB-C serial" : status_.c_str());
    drawBattery();
    auto& display = M5Cardputer.Display;
    int listStartY = 56;

    if (hasUsbError) {
        String line = status_.substring(8);
        line.trim();

        if (line.length() > 30) {
            line = line.substring(0, 30);
        }

        display.setTextColor(Theme::accent);
        display.setTextSize(Theme::bodyTextSize);
        display.setCursor(Theme::margin, 48);
        display.print(line);
        listStartY = 64;
    }

    drawList(transportItems_, transportCount_, selectedIndex_, listOffset_, listStartY);
    Screen::drawInputLine("Enter: Select  Esc: Home");
}

void Pn532KillerApp::drawMenu()
{
    const char* subtitle = "BLE bridge";
    if (transportMode_ == TransportMode::Uart) {
        subtitle = "UART direct";
    } else if (transportMode_ == TransportMode::UsbCdc) {
        subtitle = "USB-C serial";
    }
    Screen::drawTitle("ForgeNFC", subtitle);
    drawBattery();
    auto& display = M5Cardputer.Display;
    const int startY = 50;
    const int rowHeight = 16;
    display.setTextSize(Theme::bodyTextSize);

    for (int visible = 0; visible < visibleItemCount_; visible++) {
        const int index = listOffset_ + visible;

        if (index >= mainMenuCount()) {
            break;
        }

        const int y = startY + visible * rowHeight;
        const bool isSelected = index == selectedIndex_;
        display.setTextColor(isSelected ? Theme::background : Theme::text);

        if (isSelected) {
            display.fillRect(Theme::margin, y - 1, display.width() - Theme::margin * 2, rowHeight, Theme::accent);
        }

        display.setCursor(Theme::margin + 4, y + 3);
        display.print(mainMenuLabel(index));
    }

    drawScrollHints(mainMenuCount(), listOffset_, startY);
    Screen::drawNavigationFooter("Del: Link", "Move");
}

void Pn532KillerApp::drawScanInfo()
{
    Screen::drawTitle("Scan Info", cardTypeName(lastCard_.type));
    drawBattery();
    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);

    if (lastCard_.uid.length() == 0) {
        display.setCursor(Theme::margin, 62);
        display.print(status_);
        Screen::drawInputLine("Enter: Retry  Del: Back");
        return;
    }

    display.setCursor(Theme::margin, 54);
    display.print("UID: "); display.print(lastCard_.uid);
    display.setCursor(Theme::margin, 72);
    display.print("ATQA: "); display.print(lastCard_.atqa);
    display.setCursor(Theme::margin, 90);
    display.print("SAK: "); display.print(lastCard_.sak);
    Screen::drawInputLine("Enter: Scan  Del: Back");
}

void Pn532KillerApp::drawCardInfo()
{
    const CardInfo& card = currentDump_.status == DumpStatus::Empty ? lastCard_ : currentDump_.card;
    Screen::drawTitle("Card Info", cardTypeName(card.type));
    drawBattery();
    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);

    if (card.uid.length() == 0) {
        display.setCursor(Theme::margin, 62);
        display.print(status_);
        Screen::drawInputLine("Del: Back");
        return;
    }

    const int startY = 44;
    const int rowHeight = 15;

    for (int visible = 0; visible < 4; visible++) {
        const int index = resultOffset_ + visible;

        if (index >= cardInfoLineCount()) {
            break;
        }

        display.setCursor(Theme::margin, startY + visible * rowHeight);
        display.print(cardInfoLine(index));
    }

    drawScrollHints(cardInfoLineCount(), resultOffset_, startY);
    Screen::drawInputLine("Enter: Options  Up/Down");
}

void Pn532KillerApp::drawCardActions()
{
    const CardInfo& card = currentDump_.status == DumpStatus::Empty ? lastCard_ : currentDump_.card;
    Screen::drawTitle("Actions", cardTypeName(card.type));
    drawBattery();

    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 42);
    display.print("UID: ");
    display.print(card.uid);

    if (currentDump_.status != DumpStatus::Empty) {
        display.setCursor(Theme::margin, 56);
        display.print(dumpStatusName(currentDump_.status));
        display.print(" ");
        display.print(currentDump_.unitsRead);
        display.print("/");
        display.print(currentDump_.unitsTotal);
    }

    drawCardActionList(76);
    Screen::drawInputLine("Enter: Run  Del: Info");
}

void Pn532KillerApp::drawRetryResult()
{
    Screen::drawTitle("Retry Result", dumpStatusName(currentDump_.status));
    drawBattery();
    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 42);
    display.print("Before: "); display.print(retryBeforeRead_); display.print("/"); display.print(currentDump_.unitsTotal);
    display.setCursor(Theme::margin, 58);
    display.print("After:  "); display.print(retryAfterRead_); display.print("/"); display.print(currentDump_.unitsTotal);
    display.setCursor(Theme::margin, 74);
    display.print("New: "); display.print(max(0, retryAfterRead_ - retryBeforeRead_));
    display.setCursor(Theme::margin, 90);
    display.print("Missing: "); display.print(retryAfterMissing_);
    Screen::drawInputLine("Enter: Info  Del: Back");
}

void Pn532KillerApp::drawSaveName()
{
    Screen::drawTitle("Save Dump", "Name");
    drawBattery();

    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 56);
    display.print(saveName_);
    display.setCursor(Theme::margin, 82);
    display.print(cardTypeName(currentDump_.card.type));
    display.setCursor(Theme::margin, 98);
    display.print(dumpStatusName(currentDump_.status));
    Screen::drawInputLine("Enter: Save  Del: Back");
}

void Pn532KillerApp::drawReadProgress()
{
    Screen::setup();
    Screen::clear();
    Screen::drawTitle(readProgress_.title.c_str(), readProgress_.detail.c_str());
    drawBattery();

    auto& display = M5Cardputer.Display;
    const int barX = Theme::margin;
    const int barY = 56;
    const int barW = display.width() - (Theme::margin * 2);
    const int barH = 10;
    int fillW = 0;

    if (readProgress_.total > 0) {
        fillW = (barW * readProgress_.current) / readProgress_.total;
    }

    display.drawRect(barX, barY, barW, barH, Theme::text);
    display.fillRect(barX + 1, barY + 1, max(0, fillW - 2), barH - 2, Theme::accent);

    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 74);
    display.print(readProgress_.current);
    display.print("/");
    display.print(readProgress_.total);
    display.print(" units");

    int firstLog = max(0, static_cast<int>(readLog_.size()) - 3);

    if (ntagDiagnosticVisible_) {
        firstLog = min(readLogOffset_, max(0, static_cast<int>(readLog_.size()) - 3));
    }

    for (int i = firstLog; i < static_cast<int>(readLog_.size()) && i < firstLog + 3; i++) {
        const int y = 92 + ((i - firstLog) * 14);
        display.setCursor(Theme::margin, y);
        display.print(readLog_[i]);
    }

    if (ntagDiagnosticVisible_) {
        drawScrollHints(readLog_.size(), firstLog, 92, 3);
    }

    if (slotUploadResultVisible_) {
        Screen::drawInputLine("Enter: Slots  Del: Slots");
    } else if (ntagDiagnosticVisible_) {
        Screen::drawInputLine("Enter: Back  Del: Back");
    } else {
        Screen::drawInputLine("Reading... keep tag still");
    }
}

void Pn532KillerApp::drawDumpList()
{
    Screen::drawTitle(selectingDumpForSlot_ ? "Upload Dump" : "Saved Dumps", selectingDumpForSlot_ ? "Pick SD dump" : dumpStore_.status().c_str());
    drawBattery();

    if (dumpEntries_.empty()) {
        auto& display = M5Cardputer.Display;
        display.setTextColor(Theme::text);
        display.setTextSize(Theme::bodyTextSize);
        display.setCursor(Theme::margin, 64);
        display.print("No dumps on SD");
        Screen::drawInputLine("Del: Back");
        return;
    }

    auto& display = M5Cardputer.Display;
    const int startY = 50;
    const int rowHeight = 16;
    display.setTextSize(Theme::bodyTextSize);

    for (int visible = 0; visible < visibleItemCount_; visible++) {
        const int index = listOffset_ + visible;

        if (index >= static_cast<int>(dumpEntries_.size())) {
            break;
        }

        const int y = startY + visible * rowHeight;
        const bool selected = index == selectedIndex_;
        display.setTextColor(selected ? Theme::background : Theme::text);

        if (selected) {
            display.fillRect(Theme::margin, y - 1, display.width() - Theme::margin * 2, rowHeight, Theme::accent);
        }

        display.setCursor(Theme::margin + 4, y + 2);
        display.print(dumpEntries_[index].name);
        display.setCursor(display.width() - Theme::margin - 48, y + 2);
        display.print(dumpStatusName(dumpEntries_[index].status));
    }

    drawScrollHints(dumpEntries_.size(), listOffset_, startY);
    Screen::drawInputLine(selectingDumpForSlot_ ? "Enter: Upload  Del: Slot" : "Enter: Open  Del: Back");
}

void Pn532KillerApp::drawDumpDetail()
{
    Screen::drawTitle("Dump", currentDump_.name.c_str());
    drawBattery();
    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 38);
    display.print("Type: "); display.print(cardTypeName(currentDump_.card.type));
    display.setCursor(Theme::margin, 52);
    display.print("Status: "); display.print(dumpStatusName(currentDump_.status));
    display.setCursor(Theme::margin, 66);
    display.print("Read: "); display.print(currentDump_.unitsRead); display.print("/"); display.print(currentDump_.unitsTotal);
    drawDumpActionList(84);
    Screen::drawInputLine("Enter: Action  Del: List");
}

void Pn532KillerApp::drawMissingUnits()
{
    Screen::drawTitle("Missing", currentDump_.name.length() > 0 ? currentDump_.name.c_str() : "current dump");
    drawBattery();
    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);

    const int startY = 42;
    const int rowHeight = 15;

    for (int visible = 0; visible < 4; visible++) {
        const int index = resultOffset_ + visible;

        if (index >= missingUnitLineCount()) {
            break;
        }

        display.setCursor(Theme::margin, startY + visible * rowHeight);
        display.print(missingUnitLine(index));
    }

    drawScrollHints(missingUnitLineCount(), resultOffset_, startY);
    Screen::drawInputLine("Enter: Back  Up/Down");
}

void Pn532KillerApp::drawBleEmulate()
{
    Screen::drawTitle("BLE Emulate", currentDump_.name.length() > 0 ? currentDump_.name.c_str() : "NDEF custom");
    drawBattery();
    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 42);
    display.print(bleClient_.isEmulating() ? "State: active" : "State: waiting phone");
    display.setCursor(Theme::margin, 58);
    display.print("Time: "); display.print(bleEmulateSecondsLeft()); display.print("s");
    display.setCursor(Theme::margin, 74);
    display.print("APDU: "); display.print(bleClient_.emulationExchangeCount());
    display.setCursor(Theme::margin, 94);
    display.print(status_);
    Screen::drawInputLine("Del: Stop");
}

void Pn532KillerApp::drawEmulateNdefEdit()
{
    Screen::drawTitle("Emulate NDEF", emulateUrlMode_ ? "URL" : "Text");
    drawBattery();
    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 42);
    display.print("Mode: custom");
    display.setCursor(Theme::margin, 58);
    display.print(emulateBuffer_);
    display.setCursor(Theme::margin, 96);
    display.print("Phone near, then Enter");
    Screen::drawInputLine("Tab: Type  Del: Back");
}

void Pn532KillerApp::drawSlots()
{
    const char* normalActions[] = {"Start emulate", "Stop emulate", "Upload dump", "Back"};
    const char* pendingActions[] = {"Write here", "Change dump", "Cancel", "Back"};
    const char* const* actions = pendingSlotUpload_ ? pendingActions : normalActions;
    const int bankIndex = slotTypeIndex(selectedSlotType_);
    const bool loaded = bankIndex >= 0 && slotLoaded_[bankIndex][selectedSlot_];
    const String name = bankIndex >= 0 ? slotNames_[bankIndex][selectedSlot_] : "";
    const String uid = bankIndex >= 0 ? slotUids_[bankIndex][selectedSlot_] : "";
    String subtitle = String(killerSlotTypeName(selectedSlotType_)) + " " + String(selectedSlot_ + 1) + "/8";
    Screen::drawTitle("Killer Slots", subtitle.c_str());
    drawBattery();
    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 42);
    display.print("Bank: ");
    display.print(killerSlotTypeName(selectedSlotType_));
    display.setCursor(Theme::margin, 56);
    display.print(pendingSlotUpload_ ? "Target: " : "Slot: ");
    display.print(loaded ? "loaded" : "empty");
    display.setCursor(Theme::margin, 70);
    display.print(pendingSlotUpload_ ? "Dump: " : "UID: ");
    display.print(pendingSlotUpload_ ? (currentDump_.name.length() > 0 ? currentDump_.name : suggestedDumpName()) : (uid.length() > 0 ? uid : (name.length() > 0 ? name : "-")));
    const int startY = 82;
    const int rowHeight = 16;
    const int visibleActions = 2;

    for (int visible = 0; visible < visibleActions; visible++) {
        const int index = listOffset_ + visible;

        if (index >= 4) {
            break;
        }

        const int y = startY + visible * rowHeight;
        const bool isSelected = index == selectedIndex_;
        display.setTextColor(isSelected ? Theme::background : Theme::text);

        if (isSelected) {
            display.fillRect(Theme::margin, y - 1, display.width() - Theme::margin * 2, rowHeight, Theme::accent);
        }

        display.setCursor(Theme::margin + 4, y + 3);
        display.print(actions[index]);
    }

    drawScrollHints(4, listOffset_, startY, visibleActions);
    Screen::drawInputLine(pendingSlotUpload_ ? "< > Slot  Enter: Write" : "< > Slot  Tab: Type");
}

void Pn532KillerApp::drawWriteTag()
{
    Screen::drawTitle("Write Tag", writeUrlMode_ ? "URL NDEF" : "Text NDEF");
    drawBattery();
    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 54);
    display.print(writeBuffer_);
    display.setCursor(Theme::margin, 94);
    display.print("Tab: URL/Text");
    Screen::drawInputLine("Enter: Write  Del: Back");
}

void Pn532KillerApp::drawLab()
{
    const char* items[] = {"KeepCool raw diag", "Sniffer: TODO", "MFKey: TODO"};
    Screen::drawTitle("Lab", "PN532 raw");
    drawBattery();
    drawList(items, 3, selectedIndex_, listOffset_, 56);
    Screen::drawInputLine("Del: Back");
}

void Pn532KillerApp::drawBattery()
{
    powerManager_.update();
    Screen::drawBatteryIndicator(powerManager_.snapshot().batteryLevel);
}

void Pn532KillerApp::drawList(const char* const* items, int count, int selected, int offset, int startY)
{
    auto& display = M5Cardputer.Display;
    const int rowHeight = 16;
    display.setTextSize(Theme::bodyTextSize);

    for (int visible = 0; visible < visibleItemCount_; visible++) {
        const int index = offset + visible;

        if (index >= count) {
            break;
        }

        const int y = startY + visible * rowHeight;
        const bool isSelected = index == selected;
        display.setTextColor(isSelected ? Theme::background : Theme::text);

        if (isSelected) {
            display.fillRect(Theme::margin, y - 1, display.width() - Theme::margin * 2, rowHeight, Theme::accent);
        }

        display.setCursor(Theme::margin + 4, y + 3);
        display.print(items[index]);
    }

    drawScrollHints(count, offset, startY);
}

void Pn532KillerApp::drawCardActionList(int startY)
{
    auto& display = M5Cardputer.Display;
    const int rowHeight = 16;
    const int visibleActionCount = 2;
    display.setTextSize(Theme::bodyTextSize);

    for (int visible = 0; visible < visibleActionCount; visible++) {
        const int index = listOffset_ + visible;

        if (index >= cardActionCount()) {
            break;
        }

        const int y = startY + visible * rowHeight;
        const bool isSelected = index == selectedIndex_;
        display.setTextColor(isSelected ? Theme::background : Theme::text);

        if (isSelected) {
            display.fillRect(Theme::margin, y - 1, display.width() - Theme::margin * 2, rowHeight, Theme::accent);
        }

        display.setCursor(Theme::margin + 4, y + 3);
        display.print(cardActionLabel(index));
    }

    drawScrollHints(cardActionCount(), listOffset_, startY, visibleActionCount);
}

void Pn532KillerApp::drawDumpActionList(int startY)
{
    auto& display = M5Cardputer.Display;
    const int rowHeight = 16;
    const int visibleActionCount = 2;
    display.setTextSize(Theme::bodyTextSize);

    for (int visible = 0; visible < visibleActionCount; visible++) {
        const int index = listOffset_ + visible;

        if (index >= dumpActionCount()) {
            break;
        }

        const int y = startY + visible * rowHeight;
        const bool isSelected = index == selectedIndex_;
        display.setTextColor(isSelected ? Theme::background : Theme::text);

        if (isSelected) {
            display.fillRect(Theme::margin, y - 1, display.width() - Theme::margin * 2, rowHeight, Theme::accent);
        }

        display.setCursor(Theme::margin + 4, y + 3);
        display.print(dumpActionLabel(index));
    }

    drawScrollHints(dumpActionCount(), listOffset_, startY, visibleActionCount);
}

void Pn532KillerApp::drawScrollHints(int count, int offset, int startY)
{
    drawScrollHints(count, offset, startY, visibleItemCount_);
}

void Pn532KillerApp::drawScrollHints(int count, int offset, int startY, int visibleCount)
{
    if (count <= visibleCount) {
        return;
    }

    auto& display = M5Cardputer.Display;
    display.setTextColor(Theme::accent);
    display.setTextSize(Theme::bodyTextSize);

    if (offset > 0) {
        display.setCursor(display.width() - Theme::margin - 6, startY - 10);
        display.print("^");
    }

    if (offset + visibleCount < count) {
        display.setCursor(display.width() - Theme::margin - 6, startY + (visibleCount * 16) + 1);
        display.print("v");
    }
}

int Pn532KillerApp::cardActionCount() const
{
    if (currentDump_.status == DumpStatus::Empty && lastCard_.uid.length() > 0) {
        return 3;
    }

    if (currentDump_.status == DumpStatus::Empty) {
        return 0;
    }

    if (currentDump_.status == DumpStatus::InfoOnly) {
        return 3;
    }

    if (isMifareClassic(currentDump_.card.type) && currentDump_.status == DumpStatus::Partial) {
        return 5;
    }

    if (isNtag(currentDump_.card.type)) {
        if (transportMode_ == TransportMode::Ble) {
            return 4;
        }

        return currentDump_.status == DumpStatus::Partial ? 5 : 3;
    }

    return 3;
}

String Pn532KillerApp::cardActionLabel(int index) const
{
    if (currentDump_.status == DumpStatus::Empty && lastCard_.uid.length() > 0) {
        const char* items[] = {"Read detected", "Save info", "Back"};
        return items[index];
    }

    if (currentDump_.status == DumpStatus::InfoOnly) {
        const char* items[] = {"Save info", "Raw info", "Back"};
        return items[index];
    }

    if (isMifareClassic(currentDump_.card.type) && currentDump_.status == DumpStatus::Partial) {
        if (transportMode_ == TransportMode::Ble) {
            const char* items[] = {"Save partial", "Retry missing", "Keys", "Write card", "Back"};
            return items[index];
        }

        const char* items[] = {"Save partial", "Retry missing", "Keys", "Write card", "Back"};
        return items[index];
    }

    if (isNtag(currentDump_.card.type)) {
        if (transportMode_ == TransportMode::Ble) {
            if (currentDump_.status == DumpStatus::Full) {
                const char* items[] = {"Save dump", "Emulate dump", "Custom NDEF", "Back"};
                return items[index];
            }

            const char* items[] = {"Save partial", "Retry read", "Diag read", "Back"};
            return items[index];
        }

        if (currentDump_.status == DumpStatus::Partial) {
            const char* items[] = {"Save partial", "Retry read", "Diag read", "Write card", "Back"};
            return items[index];
        }

        const char* items[] = {"Save dump", "Write card", "Back"};
        return items[index];
    }

    if (transportMode_ == TransportMode::Ble) {
        const char* items[] = {"Save dump", "Write card", "Back"};
        return items[index];
    }

    const char* items[] = {"Save dump", "Write card", "Back"};
    return items[index];
}

int Pn532KillerApp::cardInfoLineCount() const
{
    if (currentDump_.status == DumpStatus::Empty) {
        return lastCard_.uid.length() > 0 ? 5 : 1;
    }

    int count = 8;

    if (!currentDump_.missingUnits.empty()) {
        count++;
    }

    if (currentDump_.unitsRead == 0 && currentDump_.unitsTotal > 0) {
        count++;
    }

    count += currentDump_.reportLines.size();
    return count;
}

String Pn532KillerApp::cardInfoLine(int index) const
{
    const CardInfo& card = currentDump_.status == DumpStatus::Empty ? lastCard_ : currentDump_.card;

    if (currentDump_.status == DumpStatus::Empty) {
        if (index == 0) return "UID: " + card.uid;
        if (index == 1) return "Type: " + String(cardTypeName(card.type));
        if (index == 2) return "ATQA: " + card.atqa;
        if (index == 3) return "SAK: " + card.sak;
        if (index == 4) return String("Readable: ") + (card.memoryReadable ? "yes" : "info only");
        return "";
    }

    if (index == 0) return "UID: " + card.uid;
    if (index == 1) return "Type: " + String(cardTypeName(card.type));
    if (index == 2) return "Status: " + String(dumpStatusName(currentDump_.status));
    if (index == 3) return "Read: " + String(currentDump_.unitsRead) + "/" + String(currentDump_.unitsTotal);
    if (index == 4) return "Bytes: " + String(currentDump_.data.size());
    if (index == 5) return "ATQA: " + card.atqa;
    if (index == 6) return "SAK: " + card.sak;
    if (index == 7) return String("Emulate: ") + (card.canEmulate ? "yes" : "no");

    int extraIndex = 8;

    if (currentDump_.unitsRead == 0 && currentDump_.unitsTotal > 0) {
        if (index == extraIndex) return "Memory locked/protected";
        extraIndex++;
    }

    if (!currentDump_.missingUnits.empty() && index == extraIndex) {
        String line = isNtag(card.type) ? "Failed page: " : "Missing: ";
        const int shown = min(5, static_cast<int>(currentDump_.missingUnits.size()));

        for (int i = 0; i < shown; i++) {
            if (i > 0) {
                line += ",";
            }

            line += String(currentDump_.missingUnits[i]);
        }

        if (static_cast<int>(currentDump_.missingUnits.size()) > shown) {
            line += "...";
        }

        return line;
    }

    if (!currentDump_.missingUnits.empty()) {
        extraIndex++;
    }

    const int reportIndex = index - extraIndex;

    if (reportIndex >= 0 && reportIndex < static_cast<int>(currentDump_.reportLines.size())) {
        return currentDump_.reportLines[reportIndex];
    }

    return "";
}

int Pn532KillerApp::dumpActionCount() const
{
    if (currentDump_.status == DumpStatus::InfoOnly) {
        return 3;
    }

    if (transportMode_ == TransportMode::Ble) {
        if (isNtag(currentDump_.card.type) && currentDump_.status == DumpStatus::Full) {
            return 4;
        }

    if (isNtag(currentDump_.card.type) && currentDump_.status == DumpStatus::Partial) {
        return 4;
    }

        if (!isMifareClassic(currentDump_.card.type) || currentDump_.missingUnits.empty()) {
            return 3;
        }

        return 5;
    }

    if (isNtag(currentDump_.card.type)) {
        return currentDump_.status == DumpStatus::Partial ? 3 : 2;
    }

    if (!isMifareClassic(currentDump_.card.type) || currentDump_.missingUnits.empty()) {
        return 3;
    }

    return 5;
}

String Pn532KillerApp::dumpActionLabel(int index) const
{
    if (currentDump_.status == DumpStatus::InfoOnly) {
        const char* items[] = {"Raw info", "Delete", "Back"};
        return items[index];
    }

    if (transportMode_ == TransportMode::Ble) {
        if (isNtag(currentDump_.card.type) && currentDump_.status == DumpStatus::Full) {
            const char* items[] = {"Emulate dump", "Custom NDEF", "Delete", "Back"};
            return items[index];
        }

        if (isNtag(currentDump_.card.type) && currentDump_.status == DumpStatus::Partial) {
            const char* items[] = {"Retry read", "Diag read", "Delete", "Back"};
            return items[index];
        }

        if (!isMifareClassic(currentDump_.card.type) || currentDump_.missingUnits.empty()) {
            const char* items[] = {"Custom NDEF", "Delete", "Back"};
            return items[index];
        }

        const char* items[] = {"View missing", "Retry read", "Custom NDEF", "Delete", "Back"};
        return items[index];
    }

    if (isNtag(currentDump_.card.type)) {
        if (currentDump_.status == DumpStatus::Partial) {
            const char* items[] = {"Retry read", "Diag read", "Delete", "Back"};
            return items[index];
        }

        const char* items[] = {"Delete", "Back"};
        return items[index];
    }

    if (!isMifareClassic(currentDump_.card.type) || currentDump_.missingUnits.empty()) {
        const char* items[] = {"Write card", "Delete", "Back"};
        return items[index];
    }

    const char* items[] = {"View missing", "Retry read", "Write card", "Delete", "Back"};
    return items[index];
}

int Pn532KillerApp::missingUnitLineCount() const
{
    if (currentDump_.missingUnits.empty()) {
        return 2;
    }

    return 1 + ((static_cast<int>(currentDump_.missingUnits.size()) + 7) / 8);
}

String Pn532KillerApp::missingUnitLine(int index) const
{
    if (index == 0) {
        if (currentDump_.missingUnits.empty()) {
            return isNtag(currentDump_.card.type) ? "No failed pages" : "No missing sectors";
        }

        return String(isNtag(currentDump_.card.type) ? "Failed pages: " : "Missing: ") + String(currentDump_.missingUnits.size());
    }

    if (currentDump_.missingUnits.empty()) {
        return index == 1 ? "Retry not needed" : "";
    }

    const int start = (index - 1) * 8;
    String line = "";

    for (int i = start; i < start + 8 && i < static_cast<int>(currentDump_.missingUnits.size()); i++) {
        if (line.length() > 0) {
            line += ",";
        }

        line += String(currentDump_.missingUnits[i]);
    }

    return line;
}

bool Pn532KillerApp::isMifareClassic(CardType type) const
{
    return type == CardType::MifareClassicMini || type == CardType::MifareClassic1K || type == CardType::MifareClassic4K;
}

bool Pn532KillerApp::isNtag(CardType type) const
{
    return type == CardType::MifareUltralight || type == CardType::Ntag213 || type == CardType::Ntag215 || type == CardType::Ntag216;
}

void Pn532KillerApp::updateListOffset(int count)
{
    const int visibleRows = visibleRowsForCurrentView();

    if (selectedIndex_ < listOffset_) {
        listOffset_ = selectedIndex_;
    }

    if (selectedIndex_ >= listOffset_ + visibleRows) {
        listOffset_ = selectedIndex_ - visibleRows + 1;
    }

    if (listOffset_ > count - visibleRows) {
        listOffset_ = max(0, count - visibleRows);
    }
}

int Pn532KillerApp::visibleRowsForCurrentView() const
{
    if (view_ == View::CardActions || view_ == View::DumpDetail || view_ == View::Slots) {
        return 2;
    }

    return visibleItemCount_;
}

int Pn532KillerApp::mainMenuCount() const
{
    return transportMode_ == TransportMode::Ble ? 5 : 6;
}

String Pn532KillerApp::mainMenuLabel(int index) const
{
    if (index == 0) return "Scan Info";
    if (index == 1) return "Read Auto";
    if (index == 2) return "Saved Dumps";

    if (transportMode_ != TransportMode::Ble) {
        if (index == 3) return "Killer Slots";
        if (index == 4) return "Write Tag";
        return "Lab";
    }

    if (index == 3) return transportMode_ == TransportMode::Ble ? "Emulate NDEF" : "Write Tag";
    return "Lab";
}

void Pn532KillerApp::openMainMenuItem()
{
    const String item = mainMenuLabel(selectedIndex_);

    if (item == "Scan Info") {
        scanInfo();
        return;
    }

    if (item == "Read Auto") {
        readAuto();
        return;
    }

    if (item == "Saved Dumps") {
        refreshDumpList();
        view_ = View::DumpList;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (item == "Killer Slots") {
        view_ = View::Slots;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (item == "Write Tag") {
        view_ = View::WriteTag;
        selectedIndex_ = 0;
        listOffset_ = 0;
        draw();
        return;
    }

    if (item == "Emulate NDEF") {
        openBleNdefEditor(false);
        return;
    }

    view_ = View::Lab;
    selectedIndex_ = 0;
    listOffset_ = 0;
    draw();
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

    if (action == "Diag read") {
        runNtagDiagnostic(View::CardActions);
        return;
    }

    if (action == "Keys") {
        status_ = keyStore_.status();
        draw();
        return;
    }

    if (action == "Emulate dump") {
        startBleNdefEmulation();
        return;
    }

    if (action == "Custom NDEF") {
        openBleNdefEditor(false);
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

void Pn532KillerApp::runNtagDiagnostic(View returnView)
{
    diagnosticReturnView_ = returnView;
    ntagDiagnosticVisible_ = true;
    readLogOffset_ = 0;
    beginReadProgress("MFU Diagnostic");
    readProgress_.title = "MFU Diagnostic";
    readProgress_.detail = currentDump_.card.uid.length() > 0 ? currentDump_.card.uid : "raw tests";
    readProgress_.total = 5;
    readProgress_.current = 0;
    readLog_.clear();
    drawReadProgress();

    if (!isNtag(currentDump_.card.type)) {
        readLog_.push_back("!! Not MFU/NTAG");
        readProgress_.done = true;
        drawReadProgress();
        return;
    }

    if (transportMode_ == TransportMode::Ble) {
        readLog_.push_back("!! Diag wired only");
        readLog_.push_back("Use UART/USB-C");
        readProgress_.done = true;
        drawReadProgress();
        return;
    }

    if (!ensureDirectReady()) {
        readLog_.push_back("!! PN532 not ready");
        readProgress_.done = true;
        drawReadProgress();
        return;
    }

    CardInfo info;
    if (!reader_.scan(info)) {
        readLog_.push_back("!! Scan " + reader_.lastError());
        readProgress_.done = true;
        drawReadProgress();
        return;
    }

    readLog_.push_back("Target " + String(pn532_.selectedTargetNumber()) + " " + info.uid);

    ntagDiagnosticCommand("GET_VERSION", {0x60});
    ntagDiagnosticCommand("READ 0", {0x30, 0x00});
    ntagDiagnosticCommand("READ 4", {0x30, 0x04});
    ntagDiagnosticCommand("FAST 0-3", {0x3A, 0x00, 0x03});
    ntagDiagnosticCommand("READ_SIG", {0x3C, 0x00});
    readProgress_.detail = "Done";
    readProgress_.done = true;
    drawReadProgress();
}

void Pn532KillerApp::ntagDiagnosticCommand(const String& label, const std::vector<uint8_t>& tagCommand)
{
    std::vector<uint8_t> command = {0x40, pn532_.selectedTargetNumber()};
    command.insert(command.end(), tagCommand.begin(), tagCommand.end());

    std::vector<uint8_t> response;
    const bool ok = pn532_.rawCommand(command, response, 1500);
    readProgress_.current++;

    if (!ok) {
        readLog_.push_back("!! " + label + " " + pn532_.lastError());
        drawReadProgress();
        return;
    }

    if (response.empty()) {
        readLog_.push_back("!! " + label + " empty");
        drawReadProgress();
        return;
    }

    String line = "OK " + label;

    if (response[0] == 0x00 && response.size() > 1) {
        line += " ";
        line += bytesToHex(&response[1], min(static_cast<size_t>(8), response.size() - 1), false);
    } else if (response[0] == 0x00) {
        line += " status0";
    } else {
        line += " raw ";
        line += bytesToHex(response.data(), min(static_cast<size_t>(8), response.size()), false);
    }

    readLog_.push_back(line);
    drawReadProgress();
}

void Pn532KillerApp::runRawPn532Diagnostic()
{
    if (transportMode_ == TransportMode::Ble) {
        if (!ensureBleReady()) {
            return;
        }
    } else if (!ensureDirectReady()) {
        return;
    }

    ntagDiagnosticVisible_ = true;
    diagnosticReturnView_ = View::Lab;
    readLogOffset_ = 0;
    beginReadProgress("Raw PN532");
    readProgress_.title = "Raw PN532";
    readProgress_.detail = "Compare transports";
    readProgress_.total = 24;
    readProgress_.current = 0;
    readLog_.clear();
    readLog_.push_back(String("Transport ") + (transportMode_ == TransportMode::Ble ? "BLE" : (transportMode_ == TransportMode::UsbCdc ? "USB-CDC" : "UART")));
    drawReadProgress();

    CardInfo info;

    runRawScenario("A SAM min IDX", {0x14, 0x01}, false, false, false, false, info);
    runRawScenario("B SAM Forge IDX", {0x14, 0x01, 0x14, 0x01}, false, false, false, false, info);
    runRawScenario("C RF SAM min IDX", {0x14, 0x01}, true, false, false, false, info);
    runRawScenario("D RF SAM min ICT", {0x14, 0x01}, true, false, false, true, info);
    runRawScenario("E SAM min Select IDX", {0x14, 0x01}, false, false, true, false, info);
    runRawScenario("F MaxRetries IDX", {0x14, 0x01}, false, true, false, false, info);

    readProgress_.detail = "Done";

    String savedPath;
    if (saveRawDiagnosticLog(info, savedPath)) {
        readLog_.push_back("Log saved");
        readLog_.push_back(savedPath);
    } else {
        readLog_.push_back("Log not saved");
    }

    readProgress_.done = true;
    drawReadProgress();
}

bool Pn532KillerApp::runRawScenario(const String& label, const std::vector<uint8_t>& samCommand, bool rfReset, bool maxRetries, bool inSelect, bool communicateThru, CardInfo& info)
{
    readLog_.push_back("-- " + label + " --");

    if (rfReset) {
        readProgress_.detail = label + " RF off";
        appendRawDiagnostic(label + " RF off", diagnoseTransportRaw({0x32, 0x01, 0x00}, 0x33, "RFConfiguration", false, 500));
        readProgress_.current++;
        delay(50);
        readProgress_.detail = label + " RF on";
        appendRawDiagnostic(label + " RF on", diagnoseTransportRaw({0x32, 0x01, 0x01}, 0x33, "RFConfiguration", false, 500));
        readProgress_.current++;
    }

    readProgress_.detail = label + " SAM";
    appendRawDiagnostic(label + " SAM", diagnoseTransportRaw(samCommand, 0x15, "SAMConfiguration", false, 500));
    readProgress_.current++;

    if (maxRetries) {
        readProgress_.detail = label + " MaxRetries";
        appendRawDiagnostic(label + " MaxRetries", diagnoseTransportRaw({0x32, 0x05, 0xFF, 0xFF, 0xFF}, 0x33, "RFConfiguration", false, 500));
        readProgress_.current++;
    }

    readProgress_.detail = label + " InList";
    const Pn532RawDiagnostic poll = diagnoseTransportRaw({0x4A, 0x01, 0x00}, 0x4B, "InListPassiveTarget", false, 500);
    appendRawDiagnostic(label + " InList", poll);
    readProgress_.current++;

    const uint8_t target = targetFromInList(poll, info);
    readLog_.push_back("Target " + String(target));

    if (inSelect) {
        readProgress_.detail = label + " InSelect";
        appendRawDiagnostic(label + " InSelect", diagnoseTransportRaw({0x54, target}, 0x55, "InSelect", true, 500));
        readProgress_.current++;
    }

    readProgress_.detail = label + " READ 04";
    const std::vector<uint8_t> readCommand = communicateThru
        ? std::vector<uint8_t>{0x42, 0x30, 0x04, 0x26, 0xEE}
        : std::vector<uint8_t>{0x40, target, 0x30, 0x04};
    appendRawDiagnostic(label + " READ 04", diagnoseTransportRaw(readCommand, communicateThru ? 0x43 : 0x41, communicateThru ? "InCommunicateThru" : "InDataExchange", true, 500));
    readProgress_.current++;
    drawReadProgress();
    return true;
}

Pn532RawDiagnostic Pn532KillerApp::diagnoseTransportRaw(const std::vector<uint8_t>& command, uint8_t expectedResponseCode, const char* label, bool hasStatusByte, uint16_t timeoutMs)
{
    if (transportMode_ == TransportMode::Ble) {
        return bleClient_.diagnoseRawCommand(command, expectedResponseCode, label, hasStatusByte, timeoutMs);
    }

    return pn532_.diagnoseRawCommand(command, expectedResponseCode, label, hasStatusByte, timeoutMs);
}

uint8_t Pn532KillerApp::targetFromInList(const Pn532RawDiagnostic& diag, CardInfo& info)
{
    if (diag.pn532Payload.size() < 6 || diag.pn532Payload[0] == 0) {
        return 0x01;
    }

    const uint8_t nbTg = diag.pn532Payload[0];
    const uint8_t target = diag.pn532Payload[1];
    const uint16_t atqa = (static_cast<uint16_t>(diag.pn532Payload[2]) << 8) | diag.pn532Payload[3];
    const uint8_t sak = diag.pn532Payload[4];
    const uint8_t uidLength = diag.pn532Payload[5];
    readLog_.push_back("NbTg " + String(nbTg));
    readLog_.push_back("Tg " + String(target));
    readLog_.push_back("SENS_RES " + bytesToHex(&diag.pn532Payload[2], 2, true));
    readLog_.push_back("SEL_RES " + bytesToHex(&diag.pn532Payload[4], 1, true));
    readLog_.push_back("NFCIDLen " + String(uidLength));

    if (diag.pn532Payload.size() >= static_cast<size_t>(6 + uidLength)) {
        info.type = detectIso14443AType(atqa, sak);
        info.uid = bytesToHex(&diag.pn532Payload[6], uidLength, true);
        info.atqa = bytesToHex(&diag.pn532Payload[2], 2);
        info.sak = bytesToHex(&diag.pn532Payload[4], 1);
        readLog_.push_back("UID " + info.uid);
        readLog_.push_back("ATQA " + info.atqa + " SAK " + info.sak);
    }

    return target == 0 ? 0x01 : target;
}

bool Pn532KillerApp::runPolledRawDiagnostic(const String& label, const std::vector<uint8_t>& tagCommand, bool communicateThru, uint16_t timeoutMs, CardInfo& info)
{
    readProgress_.detail = label;
    drawReadProgress();

    CardInfo scanned;

    if (!reader_.scan(scanned)) {
        readLog_.push_back("== " + label + " ==");
        readLog_.push_back("POLL ERR " + reader_.lastError());
        readProgress_.current++;
        drawReadProgress();
        return false;
    }

    info = scanned;
    readLog_.push_back("Poll UID " + scanned.uid);
    readLog_.push_back("ATQA " + scanned.atqa + " SAK " + scanned.sak);
    readLog_.push_back("Target " + String(pn532_.selectedTargetNumber()));

    const Pn532RawDiagnostic diag = communicateThru
        ? pn532_.diagnoseInCommunicateThru(tagCommand, timeoutMs)
        : pn532_.diagnoseInDataExchange(tagCommand, timeoutMs);

    appendRawDiagnostic(label, diag);
    readProgress_.current++;
    drawReadProgress();
    return diag.ackOk && diag.frameOk && diag.responseCodeOk && diag.inDataExchangeStatus == 0x00;
}

void Pn532KillerApp::appendRawDiagnostic(const String& label, const Pn532RawDiagnostic& diag)
{
    readLog_.push_back("== " + label + " ==");
    readLog_.push_back("TX " + (diag.txFrame.empty() ? String("-") : bytesToHex(diag.txFrame.data(), diag.txFrame.size(), true)));
    readLog_.push_back(String("ACK ") + (diag.ackOk ? "yes " : "no ") + (diag.ackBytes.empty() ? String("-") : bytesToHex(diag.ackBytes.data(), diag.ackBytes.size(), true)));
    readLog_.push_back("RX " + (diag.rxFrame.empty() ? String("-") : bytesToHex(diag.rxFrame.data(), diag.rxFrame.size(), true)));
    readLog_.push_back("Frame " + (diag.framePayload.empty() ? String("-") : bytesToHex(diag.framePayload.data(), diag.framePayload.size(), true)));
    readLog_.push_back("PN532 " + (diag.pn532Payload.empty() ? String("-") : bytesToHex(diag.pn532Payload.data(), diag.pn532Payload.size(), true)));
    readLog_.push_back("Status " + (!diag.hasStatus ? String("-") : String(diag.inDataExchangeStatus, HEX)));
    readLog_.push_back("Tag " + (diag.tagPayload.empty() ? String("-") : bytesToHex(diag.tagPayload.data(), diag.tagPayload.size(), true)));
    readLog_.push_back("Parsed " + (diag.parsed.length() > 0 ? diag.parsed : diag.error));
    readLog_.push_back("Err " + (diag.error.length() > 0 ? diag.error : String("-")));
    readLog_.push_back("Time " + String(diag.elapsedMs) + "ms");
    drawReadProgress();
}

bool Pn532KillerApp::saveRawDiagnosticLog(const CardInfo& info, String& savedPath)
{
    savedPath = "";

    if (SD.cardType() == CARD_NONE) {
        return false;
    }

    SD.mkdir("/forgeputer");
    SD.mkdir("/forgeputer/nfc");
    SD.mkdir("/forgeputer/nfc/logs");

    if (!SD.exists("/forgeputer/nfc/logs")) {
        return false;
    }

    String uid = info.uid;
    uid.replace(":", "");
    uid.toLowerCase();

    if (uid.length() == 0) {
        uid = "unknown";
    }

    savedPath = "/forgeputer/nfc/logs/raw_" + uid + "_" + String(millis()) + ".txt";
    File file = SD.open(savedPath, FILE_WRITE);

    if (!file) {
        savedPath = "";
        return false;
    }

    file.println("Forgeputer PN532 raw diagnostic");
    file.print("transport: ");
    file.println(transportMode_ == TransportMode::UsbCdc ? "usb-cdc" : "uart");
    file.print("uid: ");
    file.println(info.uid);
    file.print("atqa: ");
    file.println(info.atqa);
    file.print("sak: ");
    file.println(info.sak);
    file.print("target: ");
    file.println(pn532_.selectedTargetNumber());
    file.println();

    for (const String& line : readLog_) {
        file.println(line);
    }

    file.close();
    return true;
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

    const bool ok = killer_.uploadDump(selectedSlot_, currentDump_);

    if (ok) {
        slotLoaded_[bankIndex][selectedSlot_] = true;
        slotNames_[bankIndex][selectedSlot_] = currentDump_.name.length() > 0 ? currentDump_.name : suggestedDumpName();
        slotUids_[bankIndex][selectedSlot_] = currentDump_.card.uid;
        pendingSlotUpload_ = false;
        status_ = String(killerSlotTypeName(dumpType)) + " slot " + String(selectedSlot_ + 1) + " ready";
    } else {
        status_ = killer_.lastError();
    }

    readProgress_.title = "Upload slot";
    readProgress_.detail = status_;
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

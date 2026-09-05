// Pn532KillerApp - transport, menu, card and read progress screens.

#include "Pn532KillerApp.h"

#include <M5Cardputer.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

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

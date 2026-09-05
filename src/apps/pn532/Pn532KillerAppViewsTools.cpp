// Pn532KillerApp - dump, slot, emulation, write and lab screens.

#include "Pn532KillerApp.h"

#include <M5Cardputer.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

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

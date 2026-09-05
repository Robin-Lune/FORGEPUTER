// Pn532KillerApp - shared drawing primitives (lists, scroll hints, battery).

#include "Pn532KillerApp.h"

#include <M5Cardputer.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

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

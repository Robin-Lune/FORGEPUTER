#include "SettingsApp.h"

#include <M5Cardputer.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

void SettingsApp::init()
{
    selectedIndex_ = 0;
}

void SettingsApp::update()
{
}

void SettingsApp::draw()
{
    Screen::setup();
    Screen::clear();
    Screen::drawTitle("Settings", "System preferences");
    drawList();
    drawFooter();
}

void SettingsApp::onKey(const KeyInput& input)
{
    if (input.up) {
        moveSelection(-1);
    }

    if (input.down) {
        moveSelection(1);
    }
}

void SettingsApp::close()
{
}

void SettingsApp::moveSelection(int delta)
{
    selectedIndex_ += delta;

    if (selectedIndex_ < 0) {
        selectedIndex_ = itemCount_ - 1;
    }

    if (selectedIndex_ >= itemCount_) {
        selectedIndex_ = 0;
    }

    drawList();
}

void SettingsApp::drawList()
{
    auto& display = M5Cardputer.Display;
    const int startY = 54;
    const int rowHeight = 18;

    display.fillRect(0, startY, display.width(), rowHeight * itemCount_, Theme::background);
    display.setTextSize(Theme::bodyTextSize);

    for (int index = 0; index < itemCount_; index++) {
        const int y = startY + (index * rowHeight);
        const bool selected = index == selectedIndex_;

        display.setTextColor(selected ? Theme::background : Theme::text);

        if (selected) {
            display.fillRect(Theme::margin, y - 2, display.width() - (Theme::margin * 2), rowHeight, Theme::accent);
        }

        display.setCursor(Theme::margin + 4, y + 3);
        display.print(items_[index]);
    }
}

void SettingsApp::drawFooter()
{
    Screen::drawNavigationFooter("Esc: Home", "Move");
}

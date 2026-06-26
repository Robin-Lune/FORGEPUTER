#include "SettingsApp.h"

#include <M5Cardputer.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

void SettingsApp::init()
{
    view_ = View::List;
    selectedIndex_ = 0;
    applyBrightness();
}

void SettingsApp::update()
{
}

void SettingsApp::draw()
{
    Screen::setup();
    Screen::clear();

    if (view_ == View::List) {
        Screen::drawTitle("Settings", "System preferences");
        drawList();
        drawFooter();
        return;
    }

    drawDetail();
}

void SettingsApp::onKey(const KeyInput& input)
{
    if (input.backspace || input.del) {
        goBack();
        return;
    }

    if (input.enter) {
        openSelectedItem();
        return;
    }

    if (view_ != View::List) {
        if (selectedIndex_ == 0 && input.left) {
            adjustBrightness(-brightnessStep_);
        }

        if (selectedIndex_ == 0 && input.right) {
            adjustBrightness(brightnessStep_);
        }

        return;
    }

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
    if (view_ != View::List) {
        return;
    }

    selectedIndex_ += delta;

    if (selectedIndex_ < 0) {
        selectedIndex_ = itemCount_ - 1;
    }

    if (selectedIndex_ >= itemCount_) {
        selectedIndex_ = 0;
    }

    drawList();
}

void SettingsApp::adjustBrightness(int delta)
{
    brightness_ += delta;

    if (brightness_ < 10) {
        brightness_ = 10;
    }

    if (brightness_ > 100) {
        brightness_ = 100;
    }

    applyBrightness();
    drawDetail();
}

void SettingsApp::applyBrightness()
{
    const int hardwareBrightness = (brightness_ * 255) / 100;

    M5Cardputer.Display.setBrightness(hardwareBrightness);
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

void SettingsApp::drawDetail()
{
    auto& display = M5Cardputer.Display;
    const char* title = items_[selectedIndex_];

    Screen::clear();
    Screen::drawTitle(title, "Setting detail");

    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 58);

    if (selectedIndex_ == 0) {
        drawBrightnessDetail();
    } else if (selectedIndex_ == 1) {
        display.print("Sound: enabled");
    } else if (selectedIndex_ == 2) {
        display.print("Battery: unknown");
    } else {
        display.print("Forgeputer");
        display.setCursor(Theme::margin, 76);
        display.print("M5Launcher ready");
    }

    Screen::drawInputLine("Esc: Home  Del: Back");
}

void SettingsApp::drawBrightnessDetail()
{
    auto& display = M5Cardputer.Display;
    const int barX = Theme::margin;
    const int barY = 78;
    const int barWidth = display.width() - (Theme::margin * 2);
    const int barHeight = 10;
    const int fillWidth = (barWidth * brightness_) / 100;

    display.print("Level: ");
    display.print(brightness_);
    display.print("%");

    display.drawRect(barX, barY, barWidth, barHeight, Theme::text);

    if (fillWidth > 2) {
        display.fillRect(barX + 1, barY + 1, fillWidth - 2, barHeight - 2, Theme::accent);
    }

    display.setCursor(Theme::margin, 100);
    display.print("</>: Adjust");
}

void SettingsApp::drawFooter()
{
    Screen::drawNavigationFooter("Esc: Home", "Move");
}

void SettingsApp::openSelectedItem()
{
    if (view_ != View::List) {
        return;
    }

    view_ = View::Detail;
    draw();
}

void SettingsApp::goBack()
{
    if (view_ == View::List) {
        return;
    }

    view_ = View::List;
    draw();
}

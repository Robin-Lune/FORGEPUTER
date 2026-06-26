#include "SettingsApp.h"

#include <Arduino.h>
#include <M5Cardputer.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

SettingsApp::SettingsApp(SettingsManager &settingsManager, PowerManager &powerManager, LaunchChargeModeCallback launchChargeMode)
    : brightnessView_(settingsManager),
      batteryView_(powerManager),
      powerManager_(powerManager),
      launchChargeMode_(launchChargeMode)
{
}

void SettingsApp::init()
{
    view_ = View::List;
    selectedIndex_ = 0;
    listOffset_ = 0;
    brightnessView_.applySaved();
}

void SettingsApp::update()
{
    if (!isBatteryDetailOpen()) {
        return;
    }

    const unsigned long now = millis();

    if (now - lastBatteryRefreshMs_ >= batteryRefreshIntervalMs_) {
        drawDetail();
    }
}

void SettingsApp::draw()
{
    Screen::setup();
    Screen::clear();

    if (view_ == View::List)
    {
        Screen::drawTitle("Settings", "System preferences");
        drawBattery();
        drawList();
        drawFooter();
        return;
    }

    drawDetail();
}

void SettingsApp::onKey(const KeyInput &input)
{
    if (input.backspace || input.del)
    {
        goBack();
        return;
    }

    if (view_ != View::List)
    {
        if (selectedIndex_ == Brightness)
        {
            if (brightnessView_.onKey(input))
            {
                returnToList();
                return;
            }

            drawDetail();
        }

        return;
    }

    if (input.enter)
    {
        openSelectedItem();
        return;
    }

    if (input.up)
    {
        moveSelection(-1);
    }

    if (input.down)
    {
        moveSelection(1);
    }
}

void SettingsApp::close()
{
    cancelPendingChanges();
    view_ = View::List;
    updateListOffset();
}

void SettingsApp::moveSelection(int delta)
{
    if (view_ != View::List)
    {
        return;
    }

    selectedIndex_ += delta;

    if (selectedIndex_ < 0)
    {
        selectedIndex_ = itemCount_ - 1;
    }

    if (selectedIndex_ >= itemCount_)
    {
        selectedIndex_ = 0;
    }

    updateListOffset();
    drawList();
}

void SettingsApp::updateListOffset()
{
    if (selectedIndex_ < listOffset_)
    {
        listOffset_ = selectedIndex_;
    }

    if (selectedIndex_ >= listOffset_ + visibleItemCount_)
    {
        listOffset_ = selectedIndex_ - visibleItemCount_ + 1;
    }
}

void SettingsApp::drawList()
{
    auto &display = M5Cardputer.Display;
    const int startY = 54;
    const int rowHeight = 18;

    display.fillRect(0, startY - 6, display.width(), rowHeight * visibleItemCount_ + 14, Theme::background);
    display.setTextSize(Theme::bodyTextSize);

    if (listOffset_ > 0)
    {
        display.setTextColor(Theme::accent);
        display.setCursor(display.width() - Theme::margin - 6, startY - 6);
        display.print("^");
    }

    for (int visibleIndex = 0; visibleIndex < visibleItemCount_; visibleIndex++)
    {
        const int index = listOffset_ + visibleIndex;
        const int y = startY + (visibleIndex * rowHeight);
        const bool selected = index == selectedIndex_;

        if (index >= itemCount_)
        {
            break;
        }

        display.setTextColor(selected ? Theme::background : Theme::text);

        if (selected)
        {
            display.fillRect(Theme::margin, y - 2, display.width() - (Theme::margin * 2), rowHeight, Theme::accent);
        }

        display.setCursor(Theme::margin + 4, y + 3);
        display.print(items_[index]);
    }

    if (listOffset_ + visibleItemCount_ < itemCount_)
    {
        display.setTextColor(Theme::accent);
        display.setCursor(display.width() - Theme::margin - 6, startY + (rowHeight * visibleItemCount_) - 2);
        display.print("v");
    }
}

void SettingsApp::drawBattery()
{
    powerManager_.update();
    Screen::drawBatteryIndicator(powerManager_.snapshot().batteryLevel);
}

void SettingsApp::drawDetail()
{
    auto &display = M5Cardputer.Display;
    const char *title = items_[selectedIndex_];

    Screen::clear();
    Screen::drawTitle(title, "Setting detail");
    drawBattery();

    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 58);

    if (selectedIndex_ == Brightness)
    {
        brightnessView_.draw();
    }
    else if (selectedIndex_ == Sound)
    {
        display.print("Sound: enabled");
    }
    else if (selectedIndex_ == Battery)
    {
        batteryView_.draw();
        lastBatteryRefreshMs_ = millis();
    }
    else if (selectedIndex_ == ChargeMode)
    {
        display.print("Enter Charge Mode");
    }
    else
    {
        display.print("Forgeputer");
        display.setCursor(Theme::margin, 76);
        display.print("M5Launcher ready");
    }

    Screen::drawInputLine("Esc: Home  Del: Back");
}

void SettingsApp::drawFooter()
{
    Screen::drawNavigationFooter("Esc: Home", "Move");
}

void SettingsApp::openSelectedItem()
{
    if (view_ != View::List)
    {
        return;
    }

    if (selectedIndex_ == Brightness)
    {
        brightnessView_.open();
    }

    if (selectedIndex_ == ChargeMode)
    {
        launchChargeMode_();
        return;
    }

    view_ = View::Detail;
    draw();
}

void SettingsApp::goBack()
{
    if (view_ == View::List)
    {
        return;
    }

    cancelPendingChanges();

    view_ = View::List;
    draw();
}

bool SettingsApp::isBatteryDetailOpen() const
{
    return view_ == View::Detail && selectedIndex_ == Battery;
}

void SettingsApp::returnToList()
{
    if (view_ == View::List)
    {
        return;
    }

    view_ = View::List;
    draw();
}

void SettingsApp::cancelPendingChanges()
{
    if (view_ == View::Detail && selectedIndex_ == Brightness)
    {
        brightnessView_.cancel();
    }
}

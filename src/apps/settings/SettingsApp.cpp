#include "SettingsApp.h"

#include <M5Cardputer.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

SettingsApp::SettingsApp(SettingsManager &settingsManager)
    : brightnessView_(settingsManager)
{
}

void SettingsApp::init()
{
    view_ = View::List;
    selectedIndex_ = 0;
    brightnessView_.applySaved();
}

void SettingsApp::update()
{
}

void SettingsApp::draw()
{
    Screen::setup();
    Screen::clear();

    if (view_ == View::List)
    {
        Screen::drawTitle("Settings", "System preferences");
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

    drawList();
}

void SettingsApp::drawList()
{
    auto &display = M5Cardputer.Display;
    const int startY = 54;
    const int rowHeight = 18;

    display.fillRect(0, startY, display.width(), rowHeight * itemCount_, Theme::background);
    display.setTextSize(Theme::bodyTextSize);

    for (int index = 0; index < itemCount_; index++)
    {
        const int y = startY + (index * rowHeight);
        const bool selected = index == selectedIndex_;

        display.setTextColor(selected ? Theme::background : Theme::text);

        if (selected)
        {
            display.fillRect(Theme::margin, y - 2, display.width() - (Theme::margin * 2), rowHeight, Theme::accent);
        }

        display.setCursor(Theme::margin + 4, y + 3);
        display.print(items_[index]);
    }
}

void SettingsApp::drawDetail()
{
    auto &display = M5Cardputer.Display;
    const char *title = items_[selectedIndex_];

    Screen::clear();
    Screen::drawTitle(title, "Setting detail");

    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 58);

    if (selectedIndex_ == Brightness)
    {
        brightnessView_.draw();
    }
    else if (selectedIndex_ == 1)
    {
        display.print("Sound: enabled");
    }
    else if (selectedIndex_ == 2)
    {
        display.print("Battery: unknown");
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

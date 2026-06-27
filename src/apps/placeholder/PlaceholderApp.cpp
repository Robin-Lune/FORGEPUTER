#include "PlaceholderApp.h"

#include <M5Cardputer.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

PlaceholderApp::PlaceholderApp(const char* title, const char* subtitle, PowerManager& powerManager)
    : title_(title),
      subtitle_(subtitle),
      powerManager_(powerManager)
{
}

void PlaceholderApp::init()
{
}

void PlaceholderApp::update()
{
}

void PlaceholderApp::draw()
{
    auto& display = M5Cardputer.Display;

    Screen::setup();
    Screen::clear();
    Screen::drawTitle(title_, subtitle_);
    drawBattery();

    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, 62);
    display.print("Coming soon");

    display.setCursor(Theme::margin, 82);
    display.print("Esc: Home");
}

void PlaceholderApp::onKey(const KeyInput& input)
{
}

void PlaceholderApp::close()
{
}

void PlaceholderApp::drawBattery()
{
    powerManager_.update();
    Screen::drawBatteryIndicator(powerManager_.snapshot().batteryLevel);
}

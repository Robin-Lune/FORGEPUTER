#include "HomeApp.h"

#include "../../ui/Screen.h"

HomeApp::HomeApp(PowerManager& powerManager)
    : powerManager_(powerManager)
{
}

void HomeApp::init()
{
    inputLine_ = "> ";
}

void HomeApp::update()
{
}

void HomeApp::draw()
{
    Screen::setup();
    Screen::clear();
    Screen::drawTitle("Forgeputer", "Cardputer ADV firmware");
    drawBattery();
    drawStatus("Ready");
    drawInputLine();
}

void HomeApp::onKey(const KeyInput& input)
{
    inputLine_ += input.characters;

    if ((input.backspace || input.del) && inputLine_.length() > 2) {
        inputLine_.remove(inputLine_.length() - 1);
    }

    if (input.enter) {
        Serial.print("Input: ");
        Serial.println(inputLine_.substring(2));
        inputLine_ = "> ";
        drawStatus("Input received");
    }

    drawInputLine();
}

void HomeApp::close()
{
}

void HomeApp::drawBattery()
{
    powerManager_.update();
    Screen::drawBatteryIndicator(powerManager_.snapshot().batteryLevel);
}

void HomeApp::drawStatus(const char* message)
{
    Screen::drawStatus(message);
}

void HomeApp::drawInputLine()
{
    Screen::drawInputLine(inputLine_);
}

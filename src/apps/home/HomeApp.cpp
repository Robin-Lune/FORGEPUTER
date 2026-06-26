#include "HomeApp.h"

#include <M5Cardputer.h>

void HomeApp::init()
{
    inputLine_ = "> ";
}

void HomeApp::update()
{
}

void HomeApp::draw()
{
    drawHeader();
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

void HomeApp::drawHeader()
{
    auto& display = M5Cardputer.Display;

    display.setRotation(1);
    display.fillScreen(BLACK);
    display.setTextColor(GREEN);
    display.setTextSize(2);
    display.setCursor(8, 8);
    display.print("Forgeputer");

    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(8, 34);
    display.print("Cardputer ADV firmware");
}

void HomeApp::drawStatus(const char* message)
{
    auto& display = M5Cardputer.Display;

    display.fillRect(8, 58, display.width() - 16, 24, BLACK);
    display.setTextColor(GREEN);
    display.setTextSize(1);
    display.setCursor(8, 58);
    display.print(message);
}

void HomeApp::drawInputLine()
{
    auto& display = M5Cardputer.Display;
    const int y = display.height() - 24;

    display.fillRect(0, y, display.width(), 24, BLACK);
    display.drawFastHLine(0, y, display.width(), GREEN);
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(8, y + 8);
    display.print(inputLine_);
}

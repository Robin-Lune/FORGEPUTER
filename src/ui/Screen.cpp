#include "Screen.h"

#include <M5Cardputer.h>

#include "Theme.h"

namespace Screen {
void setup()
{
    M5Cardputer.Display.setRotation(1);
}

void clear()
{
    M5Cardputer.Display.fillScreen(Theme::background);
}

void drawTitle(const char* title, const char* subtitle)
{
    auto& display = M5Cardputer.Display;

    display.setTextColor(Theme::accent);
    display.setTextSize(Theme::titleTextSize);
    display.setCursor(Theme::margin, Theme::titleY);
    display.print(title);

    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, Theme::subtitleY);
    display.print(subtitle);
}

void drawStatus(const char* message)
{
    auto& display = M5Cardputer.Display;

    display.fillRect(
        Theme::margin,
        Theme::statusY,
        display.width() - (Theme::margin * 2),
        Theme::inputHeight,
        Theme::background
    );
    display.setTextColor(Theme::accent);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, Theme::statusY);
    display.print(message);
}

void drawInputLine(const String& inputLine)
{
    auto& display = M5Cardputer.Display;
    const int y = display.height() - Theme::inputHeight;

    display.fillRect(0, y, display.width(), Theme::inputHeight, Theme::background);
    display.drawFastHLine(0, y, display.width(), Theme::accent);
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, y + Theme::margin);
    display.print(inputLine);
}
}

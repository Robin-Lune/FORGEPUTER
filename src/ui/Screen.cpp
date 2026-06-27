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

    if (subtitle[0] != '\0') {
        display.setTextColor(Theme::text);
        display.setTextSize(Theme::bodyTextSize);
        display.setCursor(Theme::margin, Theme::subtitleY);
        display.print(subtitle);
    }
}

void drawBatteryIndicator(int level)
{
    auto& display = M5Cardputer.Display;
    const int width = 22;
    const int height = 10;
    const int capWidth = 2;
    const int x = display.width() - Theme::margin - width - capWidth - 24;
    const int y = Theme::titleY + 1;
    int fillWidth = 0;

    if (level < 0) {
        level = 0;
    }

    if (level > 100) {
        level = 100;
    }

    fillWidth = ((width - 2) * level) / 100;

    display.fillRect(x - 2, y - 1, width + capWidth + 28, height + 4, Theme::background);
    display.drawRect(x, y, width, height, Theme::text);
    display.fillRect(x + width, y + 3, capWidth, height - 6, Theme::text);

    if (fillWidth > 0) {
        display.fillRect(x + 1, y + 1, fillWidth, height - 2, Theme::accent);
    }

    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(x + width + capWidth + 4, y + 1);
    display.print(level);
    display.print("%");
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

void drawNavigationFooter(const char* homeLabel, const char* moveLabel)
{
    auto& display = M5Cardputer.Display;
    const int y = display.height() - Theme::inputHeight;
    const int textY = y + Theme::margin;
    const int arrowX = 82;
    const int arrowY = y + 7;

    display.fillRect(0, y, display.width(), Theme::inputHeight, Theme::background);
    display.drawFastHLine(0, y, display.width(), Theme::accent);
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, textY);
    display.print(homeLabel);

    display.fillTriangle(arrowX, arrowY, arrowX - 4, arrowY + 7, arrowX + 4, arrowY + 7, Theme::text);
    display.fillTriangle(arrowX + 12, arrowY + 7, arrowX + 8, arrowY, arrowX + 16, arrowY, Theme::text);

    display.setCursor(arrowX + 24, textY);
    display.print(moveLabel);
}

void drawCarouselFooter(const char* leftLabel, const char* rightLabel)
{
    auto& display = M5Cardputer.Display;
    const int y = display.height() - Theme::inputHeight;
    const int textY = y + Theme::margin;
    const int centerX = display.width() / 2;
    const int arrowY = y + 7;

    display.fillRect(0, y, display.width(), Theme::inputHeight, Theme::background);
    display.drawFastHLine(0, y, display.width(), Theme::accent);
    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);

    display.setCursor(Theme::margin, textY);
    display.print(leftLabel);

    display.fillTriangle(centerX - 14, arrowY + 4, centerX - 6, arrowY - 4, centerX - 6, arrowY + 12, Theme::text);
    display.fillTriangle(centerX + 14, arrowY + 4, centerX + 6, arrowY - 4, centerX + 6, arrowY + 12, Theme::text);

    display.setCursor(display.width() - Theme::margin - 58, textY);
    display.print(rightLabel);
}
}

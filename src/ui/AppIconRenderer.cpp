#include "AppIconRenderer.h"

#include <M5Cardputer.h>

#include "Theme.h"

namespace {
void drawRadioWaves(int centerX, int centerY, int radiusStep)
{
    auto& display = M5Cardputer.Display;

    display.fillCircle(centerX, centerY, 3, Theme::accent);
    display.drawCircle(centerX, centerY, radiusStep, Theme::accent);
    display.drawCircle(centerX, centerY, radiusStep * 2, Theme::accent);
}

void drawTextMark(const char* text, int centerX, int centerY)
{
    auto& display = M5Cardputer.Display;

    display.drawRoundRect(centerX - 26, centerY - 18, 52, 36, 4, Theme::accent);
    display.setTextColor(Theme::accent);
    display.setTextSize(2);
    display.setCursor(centerX - 20, centerY - 7);
    display.print(text);
}
}

namespace AppIconRenderer {
void draw(AppIcon icon, int centerX, int centerY)
{
    auto& display = M5Cardputer.Display;

    if (icon == AppIcon::Pn532Killer) {
        display.drawRoundRect(centerX - 22, centerY - 18, 44, 36, 5, Theme::accent);
        display.drawCircle(centerX, centerY, 11, Theme::accent);
        display.fillCircle(centerX, centerY, 3, Theme::accent);
        display.drawLine(centerX - 18, centerY - 15, centerX + 18, centerY + 15, Theme::accent);
        display.drawLine(centerX + 18, centerY - 15, centerX - 18, centerY + 15, Theme::accent);
        return;
    }

    if (icon == AppIcon::Meshtastic) {
        display.fillCircle(centerX, centerY, 3, Theme::accent);
        display.fillCircle(centerX - 20, centerY + 12, 3, Theme::accent);
        display.fillCircle(centerX + 20, centerY + 12, 3, Theme::accent);
        display.fillCircle(centerX, centerY - 18, 3, Theme::accent);
        display.drawLine(centerX, centerY, centerX - 20, centerY + 12, Theme::accent);
        display.drawLine(centerX, centerY, centerX + 20, centerY + 12, Theme::accent);
        display.drawLine(centerX, centerY, centerX, centerY - 18, Theme::accent);
        drawRadioWaves(centerX, centerY, 10);
        return;
    }

    if (icon == AppIcon::Wifi) {
        display.fillCircle(centerX, centerY + 14, 3, Theme::accent);
        display.drawCircle(centerX, centerY + 14, 12, Theme::accent);
        display.drawCircle(centerX, centerY + 14, 23, Theme::accent);
        display.fillRect(centerX - 26, centerY + 15, 52, 24, Theme::background);
        return;
    }

    if (icon == AppIcon::Ble) {
        display.drawFastVLine(centerX, centerY - 24, 48, Theme::accent);
        display.drawLine(centerX, centerY - 24, centerX + 16, centerY - 10, Theme::accent);
        display.drawLine(centerX + 16, centerY - 10, centerX, centerY, Theme::accent);
        display.drawLine(centerX, centerY, centerX + 16, centerY + 10, Theme::accent);
        display.drawLine(centerX + 16, centerY + 10, centerX, centerY + 24, Theme::accent);
        display.drawLine(centerX, centerY, centerX - 16, centerY - 12, Theme::accent);
        display.drawLine(centerX, centerY, centerX - 16, centerY + 12, Theme::accent);
        return;
    }

    if (icon == AppIcon::SubGhz) {
        display.drawFastHLine(centerX - 24, centerY, 48, Theme::accent);
        display.fillTriangle(centerX - 22, centerY, centerX - 14, centerY - 8, centerX - 14, centerY + 8, Theme::accent);
        display.fillTriangle(centerX + 22, centerY, centerX + 14, centerY - 8, centerX + 14, centerY + 8, Theme::accent);
        display.drawCircle(centerX, centerY, 9, Theme::accent);
        display.drawCircle(centerX, centerY, 18, Theme::accent);
        return;
    }

    if (icon == AppIcon::Nrf24) {
        display.drawRect(centerX - 20, centerY - 12, 40, 24, Theme::accent);
        display.drawFastVLine(centerX - 12, centerY - 20, 8, Theme::accent);
        display.drawFastVLine(centerX, centerY - 20, 8, Theme::accent);
        display.drawFastVLine(centerX + 12, centerY - 20, 8, Theme::accent);
        display.drawCircle(centerX, centerY, 5, Theme::accent);
        display.drawLine(centerX + 20, centerY, centerX + 28, centerY - 12, Theme::accent);
        return;
    }

    if (icon == AppIcon::Files) {
        display.drawRect(centerX - 18, centerY - 16, 36, 32, Theme::accent);
        display.drawFastHLine(centerX - 14, centerY - 7, 28, Theme::accent);
        display.drawFastHLine(centerX - 14, centerY + 1, 24, Theme::accent);
        display.drawFastHLine(centerX - 14, centerY + 9, 18, Theme::accent);
        display.fillTriangle(centerX + 8, centerY - 16, centerX + 18, centerY - 16, centerX + 18, centerY - 6, Theme::accent);
        return;
    }

    if (icon == AppIcon::BadUsb) {
        display.drawRect(centerX - 14, centerY - 16, 28, 26, Theme::accent);
        display.fillRect(centerX - 8, centerY + 10, 16, 8, Theme::accent);
        display.drawFastHLine(centerX - 7, centerY - 8, 14, Theme::accent);
        display.drawFastVLine(centerX, centerY - 14, 12, Theme::accent);
        display.drawLine(centerX - 10, centerY - 2, centerX + 10, centerY - 2, Theme::accent);
        return;
    }

    if (icon == AppIcon::Voice) {
        display.drawRoundRect(centerX - 8, centerY - 22, 16, 28, 6, Theme::accent);
        display.drawFastVLine(centerX - 18, centerY - 4, 12, Theme::accent);
        display.drawFastVLine(centerX + 18, centerY - 4, 12, Theme::accent);
        display.drawCircle(centerX, centerY - 4, 22, Theme::accent);
        display.fillRect(centerX - 24, centerY + 8, 48, 18, Theme::background);
        display.drawFastVLine(centerX, centerY + 6, 14, Theme::accent);
        display.drawFastHLine(centerX - 10, centerY + 20, 20, Theme::accent);
        return;
    }

    if (icon == AppIcon::Vitals) {
        display.drawRoundRect(centerX - 24, centerY - 18, 48, 36, 4, Theme::accent);
        display.drawLine(centerX - 18, centerY, centerX - 8, centerY, Theme::accent);
        display.drawLine(centerX - 8, centerY, centerX - 2, centerY - 10, Theme::accent);
        display.drawLine(centerX - 2, centerY - 10, centerX + 6, centerY + 10, Theme::accent);
        display.drawLine(centerX + 6, centerY + 10, centerX + 12, centerY, Theme::accent);
        display.drawLine(centerX + 12, centerY, centerX + 20, centerY, Theme::accent);
        return;
    }

    if (icon == AppIcon::Settings) {
        const int radius = 18;

        display.drawCircle(centerX, centerY, radius, Theme::accent);
        display.drawCircle(centerX, centerY, radius - 7, Theme::accent);
        display.drawFastHLine(centerX - 25, centerY, 50, Theme::accent);
        display.drawFastVLine(centerX, centerY - 25, 50, Theme::accent);
        display.drawLine(centerX - 18, centerY - 18, centerX + 18, centerY + 18, Theme::accent);
        display.drawLine(centerX + 18, centerY - 18, centerX - 18, centerY + 18, Theme::accent);
        return;
    }

    if (icon == AppIcon::Charge) {
        display.drawRect(centerX - 17, centerY - 14, 30, 28, Theme::accent);
        display.fillRect(centerX + 14, centerY - 6, 4, 12, Theme::accent);
        display.fillTriangle(centerX - 2, centerY - 11, centerX - 10, centerY + 2, centerX - 1, centerY + 2, Theme::accent);
        display.fillTriangle(centerX + 2, centerY + 11, centerX + 10, centerY - 2, centerX + 1, centerY - 2, Theme::accent);
        return;
    }

    drawTextMark("APP", centerX, centerY);
}
}

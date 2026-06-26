#include "BrightnessSettingView.h"

#include <M5Cardputer.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

BrightnessSettingView::BrightnessSettingView(SettingsManager& settingsManager)
    : settingsManager_(settingsManager)
{
}

void BrightnessSettingView::open()
{
    brightnessDraft_ = settingsManager_.brightness();
}

bool BrightnessSettingView::onKey(const KeyInput& input)
{
    if (input.left) {
        adjust(-brightnessStep_);
        return false;
    }

    if (input.right) {
        adjust(brightnessStep_);
        return false;
    }

    if (input.enter) {
        confirm();
        return true;
    }

    return false;
}

void BrightnessSettingView::draw()
{
    auto& display = M5Cardputer.Display;
    const int barX = Theme::margin;
    const int barY = 78;
    const int barWidth = display.width() - (Theme::margin * 2);
    const int barHeight = 10;
    const int fillWidth = (barWidth * brightnessDraft_) / 100;

    display.print("Level: ");
    display.print(brightnessDraft_);
    display.print("%");

    display.drawRect(barX, barY, barWidth, barHeight, Theme::text);

    if (fillWidth > 2) {
        display.fillRect(barX + 1, barY + 1, fillWidth - 2, barHeight - 2, Theme::accent);
    }

    display.setCursor(Theme::margin, 100);
    display.print("</>: Adjust  Ok: Save");
}

void BrightnessSettingView::cancel()
{
    applySaved();
}

void BrightnessSettingView::applySaved()
{
    M5Cardputer.Display.setBrightness(settingsManager_.hardwareBrightness());
}

void BrightnessSettingView::adjust(int delta)
{
    brightnessDraft_ += delta;

    if (brightnessDraft_ < 10) {
        brightnessDraft_ = 10;
    }

    if (brightnessDraft_ > 100) {
        brightnessDraft_ = 100;
    }

    preview();
}

void BrightnessSettingView::preview()
{
    M5Cardputer.Display.setBrightness(settingsManager_.hardwareBrightnessFor(brightnessDraft_));
}

void BrightnessSettingView::confirm()
{
    settingsManager_.setBrightness(brightnessDraft_);
    settingsManager_.save();
    applySaved();
}

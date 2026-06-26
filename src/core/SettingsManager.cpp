#include "SettingsManager.h"

int SettingsManager::brightness() const
{
    return brightness_;
}

void SettingsManager::setBrightness(int brightness)
{
    if (brightness < 10) {
        brightness = 10;
    }

    if (brightness > 100) {
        brightness = 100;
    }

    brightness_ = brightness;
}

int SettingsManager::hardwareBrightness() const
{
    return (brightness_ * 255) / 100;
}

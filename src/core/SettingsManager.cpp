#include "SettingsManager.h"

void SettingsManager::begin()
{
    preferences_.begin(storageNamespace_, false);
    brightness_ = normalizeBrightness(preferences_.getInt(brightnessKey_, brightness_));
}

int SettingsManager::brightness() const
{
    return brightness_;
}

void SettingsManager::setBrightness(int brightness)
{
    const int normalizedBrightness = normalizeBrightness(brightness);

    if (normalizedBrightness == brightness_) {
        return;
    }

    brightness_ = normalizedBrightness;
}

void SettingsManager::save()
{
    preferences_.putInt(brightnessKey_, brightness_);
}

int SettingsManager::hardwareBrightness() const
{
    return hardwareBrightnessFor(brightness_);
}

int SettingsManager::hardwareBrightnessFor(int brightness) const
{
    return (normalizeBrightness(brightness) * 255) / 100;
}

int SettingsManager::normalizeBrightness(int brightness) const
{
    if (brightness < 10) {
        return 10;
    }

    if (brightness > 100) {
        return 100;
    }

    return brightness;
}

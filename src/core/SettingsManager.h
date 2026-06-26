#pragma once

#include <Preferences.h>

class SettingsManager {
public:
    void begin();
    int brightness() const;
    void setBrightness(int brightness);
    void save();
    int hardwareBrightness() const;
    int hardwareBrightnessFor(int brightness) const;

private:
    static constexpr const char* storageNamespace_ = "forgeputer";
    static constexpr const char* brightnessKey_ = "brightness";

    Preferences preferences_;
    int brightness_ = 50;

    int normalizeBrightness(int brightness) const;
};

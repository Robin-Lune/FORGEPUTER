#pragma once

#include "../../core/KeyInput.h"
#include "../../core/SettingsManager.h"

class BrightnessSettingView {
public:
    explicit BrightnessSettingView(SettingsManager& settingsManager);

    void open();
    bool onKey(const KeyInput& input);
    void draw();
    void cancel();
    void applySaved();

private:
    static constexpr int brightnessStep_ = 10;

    SettingsManager& settingsManager_;
    int brightnessDraft_ = 50;

    void adjust(int delta);
    void preview();
    void confirm();
};

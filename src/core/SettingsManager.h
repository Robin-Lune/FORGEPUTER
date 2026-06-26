#pragma once

class SettingsManager {
public:
    int brightness() const;
    void setBrightness(int brightness);
    int hardwareBrightness() const;

private:
    int brightness_ = 50;
};

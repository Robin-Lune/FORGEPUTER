#pragma once

#include "../../core/App.h"
#include "../../core/PowerManager.h"
#include "../../core/SettingsManager.h"

class ChargeModeApp : public App {
public:
    ChargeModeApp(PowerManager& powerManager, SettingsManager& settingsManager);

    void init() override;
    void update() override;
    void draw() override;
    void onKey(const KeyInput& input) override;
    void close() override;

private:
    static constexpr unsigned long refreshIntervalMs_ = 60000;
    static constexpr unsigned long screenSleepDelayMs_ = 5000;

    PowerManager& powerManager_;
    SettingsManager& settingsManager_;
    unsigned long lastRefreshMs_ = 0;
    unsigned long lastActivityMs_ = 0;
    int displayedBatteryLevel_ = -1;
    bool screenSleeping_ = false;

    void disableRadios();
    void sleepScreen();
    void wakeScreen();
    void refreshBatteryLevel(bool forceDraw);
    void drawBatteryLevel(int level);
};

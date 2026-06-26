#pragma once

#include "BatteryStatusView.h"
#include "BrightnessSettingView.h"

#include "../../core/App.h"
#include "../../core/PowerManager.h"
#include "../../core/SettingsManager.h"

class SettingsApp : public App {
public:
    using LaunchChargeModeCallback = void (*)();

    SettingsApp(SettingsManager& settingsManager, PowerManager& powerManager, LaunchChargeModeCallback launchChargeMode);

    void init() override;
    void update() override;
    void draw() override;
    void onKey(const KeyInput& input) override;
    void close() override;

private:
    enum Item {
        Brightness = 0,
        Sound,
        Battery,
        ChargeMode,
        About,
    };

    enum class View {
        List,
        Detail,
    };

    static constexpr int itemCount_ = 5;
    static constexpr int visibleItemCount_ = 3;
    static constexpr unsigned long batteryRefreshIntervalMs_ = 1000;
    const char* items_[itemCount_] = {
        "Brightness",
        "Sound",
        "Battery",
        "Charge Mode",
        "About",
    };
    BrightnessSettingView brightnessView_;
    BatteryStatusView batteryView_;
    PowerManager& powerManager_;
    LaunchChargeModeCallback launchChargeMode_;
    View view_ = View::List;
    int selectedIndex_ = 0;
    int listOffset_ = 0;
    unsigned long lastBatteryRefreshMs_ = 0;

    void moveSelection(int delta);
    void updateListOffset();
    void drawBattery();
    void drawList();
    void drawDetail();
    void drawFooter();
    void openSelectedItem();
    void goBack();
    void returnToList();
    void cancelPendingChanges();
    bool isBatteryDetailOpen() const;
};

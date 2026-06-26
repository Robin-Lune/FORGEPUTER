#pragma once

#include "BrightnessSettingView.h"

#include "../../core/App.h"
#include "../../core/SettingsManager.h"

class SettingsApp : public App {
public:
    explicit SettingsApp(SettingsManager& settingsManager);

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
        About,
    };

    enum class View {
        List,
        Detail,
    };

    static constexpr int itemCount_ = 4;
    const char* items_[itemCount_] = {
        "Brightness",
        "Sound",
        "Battery",
        "About",
    };
    BrightnessSettingView brightnessView_;
    View view_ = View::List;
    int selectedIndex_ = 0;

    void moveSelection(int delta);
    void drawList();
    void drawDetail();
    void drawFooter();
    void openSelectedItem();
    void goBack();
    void returnToList();
    void cancelPendingChanges();
};

#pragma once

#include "../../core/App.h"

class SettingsApp : public App {
public:
    void init() override;
    void update() override;
    void draw() override;
    void onKey(const KeyInput& input) override;
    void close() override;

private:
    enum class View {
        List,
        Detail,
    };

    static constexpr int itemCount_ = 4;
    static constexpr int brightnessStep_ = 10;
    const char* items_[itemCount_] = {
        "Brightness",
        "Sound",
        "Battery",
        "About",
    };
    View view_ = View::List;
    int selectedIndex_ = 0;
    int brightness_ = 50;

    void moveSelection(int delta);
    void adjustBrightness(int delta);
    void applyBrightness();
    void drawList();
    void drawDetail();
    void drawBrightnessDetail();
    void drawFooter();
    void openSelectedItem();
    void goBack();
};

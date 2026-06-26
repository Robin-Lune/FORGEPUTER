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
    static constexpr int itemCount_ = 4;
    const char* items_[itemCount_] = {
        "Brightness",
        "Sound",
        "Battery",
        "About",
    };
    int selectedIndex_ = 0;

    void moveSelection(int delta);
    void drawList();
    void drawFooter();
};

#pragma once

#include "../../core/App.h"
#include "../../core/PowerManager.h"

class PlaceholderApp : public App {
public:
    PlaceholderApp(const char* title, const char* subtitle, PowerManager& powerManager);

    void init() override;
    void update() override;
    void draw() override;
    void onKey(const KeyInput& input) override;
    void close() override;

private:
    const char* title_;
    const char* subtitle_;
    PowerManager& powerManager_;

    void drawBattery();
};

#pragma once

#include "../../core/App.h"
#include "../../core/PowerManager.h"

class HomeApp : public App {
public:
    explicit HomeApp(PowerManager& powerManager);

    void init() override;
    void update() override;
    void draw() override;
    void onKey(const KeyInput& input) override;
    void close() override;

private:
    PowerManager& powerManager_;
    String inputLine_ = "> ";

    void drawBattery();
    void drawStatus(const char* message);
    void drawInputLine();
};

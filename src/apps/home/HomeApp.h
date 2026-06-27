#pragma once

#include "../../core/App.h"
#include "../../core/AppDescriptor.h"
#include "../../core/PowerManager.h"

class HomeApp : public App {
public:
    HomeApp(PowerManager& powerManager, const AppDescriptor* apps, int appCount);

    void init() override;
    void update() override;
    void draw() override;
    void onKey(const KeyInput& input) override;
    void close() override;

private:
    PowerManager& powerManager_;
    const AppDescriptor* apps_;
    int appCount_;
    int selectedIndex_ = 0;

    void moveSelection(int delta);
    void drawBattery();
    void drawCarousel();
    void drawIcon();
    void drawSideArrows();
    void drawFooter();
    void openSelectedItem();
};

#include "HomeApp.h"

#include <M5Cardputer.h>

#include "../../ui/AppIconRenderer.h"
#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

HomeApp::HomeApp(PowerManager& powerManager, const AppDescriptor* apps, int appCount)
    : powerManager_(powerManager),
      apps_(apps),
      appCount_(appCount)
{
}

void HomeApp::init()
{
    selectedIndex_ = 0;
}

void HomeApp::update()
{
}

void HomeApp::draw()
{
    Screen::setup();
    Screen::clear();
    Screen::drawTitle("Forgeputer", "");
    drawBattery();
    drawCarousel();
    drawFooter();
}

void HomeApp::onKey(const KeyInput& input)
{
    if (input.enter) {
        openSelectedItem();
        return;
    }

    if (input.left || input.up) {
        moveSelection(-1);
    }

    if (input.right || input.down) {
        moveSelection(1);
    }
}

void HomeApp::close()
{
}

void HomeApp::moveSelection(int delta)
{
    selectedIndex_ += delta;

    if (selectedIndex_ < 0) {
        selectedIndex_ = appCount_ - 1;
    }

    if (selectedIndex_ >= appCount_) {
        selectedIndex_ = 0;
    }

    drawCarousel();
}

void HomeApp::drawBattery()
{
    powerManager_.update();
    Screen::drawBatteryIndicator(powerManager_.snapshot().batteryLevel);
}

void HomeApp::drawCarousel()
{
    auto& display = M5Cardputer.Display;
    const int iconAreaY = 38;
    const int iconAreaHeight = 70;
    const int labelY = 96;

    display.fillRect(0, iconAreaY - 4, display.width(), iconAreaHeight + 24, Theme::background);
    drawSideArrows();
    drawIcon();

    display.setTextColor(Theme::text);
    display.setTextSize(Theme::bodyTextSize);
    display.setCursor(Theme::margin, labelY);
    display.print(selectedIndex_ + 1);
    display.print("/");
    display.print(appCount_);
    display.print(" ");
    display.print(apps_[selectedIndex_].name);
}

void HomeApp::drawIcon()
{
    auto& display = M5Cardputer.Display;
    const int centerX = display.width() / 2;
    const int centerY = 70;

    AppIconRenderer::draw(apps_[selectedIndex_].icon, centerX, centerY);
}

void HomeApp::drawSideArrows()
{
    auto& display = M5Cardputer.Display;
    const int centerY = 70;

    display.fillTriangle(Theme::margin + 2, centerY, Theme::margin + 12, centerY - 8, Theme::margin + 12, centerY + 8, Theme::text);
    display.fillTriangle(display.width() - Theme::margin - 2, centerY, display.width() - Theme::margin - 12, centerY - 8, display.width() - Theme::margin - 12, centerY + 8, Theme::text);
}

void HomeApp::drawFooter()
{
    Screen::drawCarouselFooter("Esc: Settings", "Ok: Select");
}

void HomeApp::openSelectedItem()
{
    if (apps_[selectedIndex_].launch != nullptr) {
        apps_[selectedIndex_].launch();
    }
}

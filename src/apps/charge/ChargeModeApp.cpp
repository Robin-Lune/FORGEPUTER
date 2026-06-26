#include "ChargeModeApp.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <WiFi.h>
#include <esp32-hal-bt.h>

#include "../../ui/Screen.h"
#include "../../ui/Theme.h"

ChargeModeApp::ChargeModeApp(PowerManager& powerManager, SettingsManager& settingsManager)
    : powerManager_(powerManager),
      settingsManager_(settingsManager)
{
}

void ChargeModeApp::init()
{
    disableRadios();
    wakeScreen();
    draw();
}

void ChargeModeApp::update()
{
    const unsigned long now = millis();

    if (!screenSleeping_ && now - lastRefreshMs_ >= refreshIntervalMs_) {
        refreshBatteryLevel(false);
    }

    if (!screenSleeping_ && now - lastActivityMs_ >= screenSleepDelayMs_) {
        sleepScreen();
    }
}

void ChargeModeApp::draw()
{
    Screen::setup();
    Screen::clear();
    Screen::drawTitle("Charge Mode", "");
    refreshBatteryLevel(true);
    Screen::drawInputLine("Esc: Home");
}

void ChargeModeApp::onKey(const KeyInput& input)
{
    if (screenSleeping_) {
        wakeScreen();
        draw();
        return;
    }

    lastActivityMs_ = millis();
}

void ChargeModeApp::close()
{
    wakeScreen();
}

void ChargeModeApp::disableRadios()
{
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();
}

void ChargeModeApp::sleepScreen()
{
    M5Cardputer.Display.setBrightness(0);
    screenSleeping_ = true;
}

void ChargeModeApp::wakeScreen()
{
    M5Cardputer.Display.setBrightness(settingsManager_.hardwareBrightness());
    screenSleeping_ = false;
    lastActivityMs_ = millis();
    lastRefreshMs_ = 0;
}

void ChargeModeApp::refreshBatteryLevel(bool forceDraw)
{
    powerManager_.update();
    int level = powerManager_.snapshot().batteryLevel;

    if (level < 0) {
        level = 0;
    }

    if (!forceDraw && level == displayedBatteryLevel_) {
        lastRefreshMs_ = millis();
        return;
    }

    displayedBatteryLevel_ = level;
    drawBatteryLevel(level);
    lastRefreshMs_ = millis();
}

void ChargeModeApp::drawBatteryLevel(int level)
{
    auto& display = M5Cardputer.Display;
    const int centerX = display.width() / 2;
    const int valueY = 58;

    display.fillRect(0, 52, display.width(), 64, Theme::background);
    display.setTextColor(Theme::accent);
    display.setTextSize(4);
    display.setCursor(centerX - 42, valueY);
    display.print(level);
    display.print("%");
}

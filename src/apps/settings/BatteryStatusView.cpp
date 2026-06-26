#include "BatteryStatusView.h"

#include <M5Cardputer.h>

#include "../../ui/Theme.h"

BatteryStatusView::BatteryStatusView(PowerManager& powerManager)
    : powerManager_(powerManager)
{
}

void BatteryStatusView::draw()
{
    auto& display = M5Cardputer.Display;

    powerManager_.update();
    const PowerSnapshot& snapshot = powerManager_.snapshot();

    drawValue("Lvl", snapshot.batteryLevel, "%", 54);
    drawValue("Bat", snapshot.batteryVoltageMv, "mV", 66);
    drawValue("Cur", snapshot.batteryCurrentMa, "mA", 78);
    drawValue("VBUS", snapshot.vbusVoltageMv, "mV", 90);

    display.setCursor(Theme::margin, 102);
    display.print("Chg: ");
    display.print(chargeStateLabel(snapshot.chargeState));
}

const char* BatteryStatusView::chargeStateLabel(ChargeState state) const
{
    if (state == ChargeState::Charging) {
        return "charging";
    }

    if (state == ChargeState::Discharging) {
        return "discharging";
    }

    return "unknown";
}

void BatteryStatusView::drawValue(const char* label, int value, const char* unit, int y)
{
    auto& display = M5Cardputer.Display;

    display.setCursor(Theme::margin, y);
    display.print(label);
    display.print(": ");

    if (value < 0) {
        display.print("n/a");
        return;
    }

    display.print(value);
    display.print(unit);
}

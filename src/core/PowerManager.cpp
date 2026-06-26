#include "PowerManager.h"

#include <M5Cardputer.h>

bool PowerSnapshot::hasExternalPower() const
{
    return vbusVoltageMv > 4000 || chargeState == ChargeState::Charging;
}

void PowerManager::update()
{
    snapshot_.batteryLevel = M5Cardputer.Power.getBatteryLevel();
    snapshot_.batteryVoltageMv = M5Cardputer.Power.getBatteryVoltage();
    snapshot_.batteryCurrentMa = M5Cardputer.Power.getBatteryCurrent();
    snapshot_.vbusVoltageMv = M5Cardputer.Power.getVBUSVoltage();
    snapshot_.chargeState = readChargeState();
}

const PowerSnapshot& PowerManager::snapshot() const
{
    return snapshot_;
}

ChargeState PowerManager::readChargeState() const
{
    const auto chargeState = M5Cardputer.Power.isCharging();

    if (chargeState == m5::Power_Class::is_charging) {
        return ChargeState::Charging;
    }

    if (chargeState == m5::Power_Class::is_discharging) {
        return ChargeState::Discharging;
    }

    return ChargeState::Unknown;
}

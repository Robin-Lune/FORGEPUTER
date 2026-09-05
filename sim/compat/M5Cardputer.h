// Shim M5Cardputer pour la simulation desktop SDL.
// Expose Display (M5GFX réel, rendu SDL) et Power (valeurs simulées).
#pragma once

#include <Arduino.h>
#include <M5GFX.h>

namespace m5 {

class Power_Class {
public:
    enum is_charging_t {
        is_discharging = 0,
        is_charging = 1,
        charge_unknown = 2,
    };

    // Valeurs simulées : elles reproduisent le comportement observé sur
    // Cardputer ADV réel, où courant et VBUS ne remontent rien d'exploitable.
    int getBatteryLevel() const { return batteryLevel_; }
    int getBatteryVoltage() const { return batteryVoltageMv_; }
    int getBatteryCurrent() const { return 0; }
    int getVBUSVoltage() const { return -1; }
    is_charging_t isCharging() const { return charge_unknown; }

    void setSimulatedLevel(int level) { batteryLevel_ = level; }
    void setSimulatedVoltage(int mv) { batteryVoltageMv_ = mv; }

private:
    int batteryLevel_ = 72;
    int batteryVoltageMv_ = 3894;
};

} // namespace m5

class M5Cardputer_Class {
public:
    M5GFX Display;
    m5::Power_Class Power;

    void begin()
    {
        Display.init();
        Display.setRotation(1);
    }

    void update() {}
};

extern M5Cardputer_Class M5Cardputer;

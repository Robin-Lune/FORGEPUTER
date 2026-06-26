#pragma once

enum class ChargeState {
    Discharging,
    Charging,
    Unknown,
};

struct PowerSnapshot {
    int batteryLevel = -1;
    int batteryVoltageMv = -1;
    int batteryCurrentMa = 0;
    int vbusVoltageMv = -1;
    ChargeState chargeState = ChargeState::Unknown;

    bool hasExternalPower() const;
};

class PowerManager {
public:
    void update();
    const PowerSnapshot& snapshot() const;

private:
    PowerSnapshot snapshot_;

    ChargeState readChargeState() const;
};

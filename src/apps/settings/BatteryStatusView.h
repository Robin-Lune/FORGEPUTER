#pragma once

#include "../../core/PowerManager.h"

class BatteryStatusView {
public:
    explicit BatteryStatusView(PowerManager& powerManager);

    void draw();

private:
    PowerManager& powerManager_;

    const char* chargeStateLabel(ChargeState state) const;
    void drawValue(const char* label, int value, const char* unit, int y);
};
